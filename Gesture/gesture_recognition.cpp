#include "gesture_recognition.hpp"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

// =============================================
//  생성자
// =============================================
GestureRecognizer::GestureRecognizer()
    : _sensitivity(1.0f)
    , _cooldownMs(1200)    // 스와이프 연속 발동 반응 속도를 위해 쿨다운 살짝 하향 (1500 -> 1200)
    , _trajectorySize(8)   // [스와이프 패치 1] 빠른 인식을 위해 필요 프레임 축소 (15 -> 8)
    , _swipeThreshold(45.0f) // [스와이프 패치 2] 미세한 움직임도 잡도록 임계값 하향 (60 -> 45)
    , _minHandArea(2500)   
    , _maxHandArea(28000)  
    , _lastGesture(GestureType::NONE)
{
    _bgSubtractor = cv::createBackgroundSubtractorMOG2(500, 50, false);
    _lastGestureTime = std::chrono::steady_clock::now() -
                       std::chrono::milliseconds(10000);
    _prevState.isDetected = false;
}

void GestureRecognizer::setGestureCallback(GestureCallback callback) {
    _callback = callback;
}

void GestureRecognizer::setSensitivity(float s) {
    _sensitivity = std::max(0.5f, std::min(2.0f, s));
    _swipeThreshold = 45.0f / _sensitivity;
}

void GestureRecognizer::setCooldown(int ms) {
    _cooldownMs = ms;
}

// =============================================
//  메인 처리 루프
// =============================================
GestureType GestureRecognizer::processFrame(cv::Mat& frame) {
    GestureType detected = GestureType::NONE;

    // 1. 손 감지 (얼굴 배제 포함)
    HandState state = detectHand(frame);

    // 2. 디버그 오버레이
    drawDebugOverlay(frame, state);

    if (!state.isDetected) {
        _trajectory.clear();
        _prevState = state;
        return GestureType::NONE;
    }

    // 3. 궤적 추가
    _trajectory.push_back(state.center);
    if ((int)_trajectory.size() > _trajectorySize)
        _trajectory.pop_front();

    // 쿨다운 체크
    if (!isCooldownExpired()) {
        _prevState = state;
        return GestureType::NONE;
    }

    // 4. 스와이프 감지 우선순위 상향 조정!
    // 손이 이동 중일 때는 포즈보다 스와이프(상하좌우)를 먼저 체크하도록 흐름 제어
    if (_trajectory.size() >= 4) { 
        detected = detectSwipe();
    }

    // 5. 스와이프가 안 일어났을 때만 제자리 손 포즈(재생/정지) 분석
    if (detected == GestureType::NONE) {
        detected = detectHandPose(state);
    }

    // 6. 콜백 발동 및 상태 유지
    if (detected != GestureType::NONE) {
        if (detected == _lastGesture && (detected == GestureType::HAND_OPEN || detected == GestureType::HAND_FIST)) {
            _trajectory.clear();
            return GestureType::NONE;
        }

        resetCooldown();
        _lastGesture = detected;
        _trajectory.clear();
        if (_callback) _callback(detected, 1.0f);
        std::cout << "[Gesture 발동] " << gestureToString(detected) << std::endl;
    }

    _prevState = state;
    return detected;
}

// =============================================
//  손 감지
// =============================================
HandState GestureRecognizer::detectHand(cv::Mat& frame) {
    HandState state;
    state.isDetected = false;
    state.area = 0;
    state.fingerCount = 0;

    cv::Mat blurred;
    cv::GaussianBlur(frame, blurred, cv::Size(7, 7), 0);

    cv::Mat hsv;
    cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);

    cv::Mat skinMask;
    cv::inRange(hsv, cv::Scalar(0, 30, 60), cv::Scalar(20, 180, 255), skinMask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_OPEN,  kernel);
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_CLOSE, kernel);

    // 얼굴 영역 확실하게 35% 커트
    int ignoreHeight = frame.rows * 0.35;
    if(ignoreHeight > 0) {
        cv::rectangle(skinMask, cv::Rect(0, 0, frame.cols, ignoreHeight), cv::Scalar(0), -1);
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return state;

    std::sort(contours.begin(), contours.end(), [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
        return cv::contourArea(a) > cv::contourArea(b);
    });

    std::vector<cv::Point> bestHandContour;
    float bestArea = 0;
    int bestFingerCount = 0;
    cv::Point2f bestCenter;

    for (const auto& contour : contours) {
        float area = (float)cv::contourArea(contour);
        if (area < _minHandArea || area > _maxHandArea) continue;

        cv::Rect bRect = cv::boundingRect(contour);
        float aspectRatio = (float)bRect.width / bRect.height;
        if (aspectRatio < 0.3f || aspectRatio > 3.0f) continue; 

        cv::Moments M = cv::moments(contour);
        if (M.m00 == 0) continue;
        cv::Point2f center((float)(M.m10 / M.m00), (float)(M.m01 / M.m00));

        HandState tempState;
        tempState.contour = contour;
        tempState.area = area;
        tempState.center = center;
        int fingers = countFingers(tempState, skinMask);

        if (bestHandContour.empty()) {
            bestHandContour = contour;
            bestArea = area;
            bestFingerCount = fingers;
            bestCenter = center;
        } else if (fingers > 0 && center.y > bestCenter.y) {
            bestHandContour = contour;
            bestArea = area;
            bestFingerCount = fingers;
            bestCenter = center;
            break; 
        }
    }

    if (bestHandContour.empty()) return state;

    state.isDetected  = true;
    state.area        = bestArea;
    state.contour     = bestHandContour;
    state.center      = bestCenter;
    state.fingerCount = bestFingerCount;

    return state;
}

int GestureRecognizer::countFingers(const HandState& state, const cv::Mat& mask) {
    if (state.contour.size() < 10) return 0;

    std::vector<int> hullIdx;
    cv::convexHull(state.contour, hullIdx);
    if (hullIdx.size() < 3) return 0;

    std::vector<cv::Vec4i> defects;
    try {
        cv::convexityDefects(state.contour, hullIdx, defects);
    } catch (...) {
        return 0;
    }

    int fingers = 0;
    float handRadius = std::sqrt(state.area / CV_PI);

    for (const auto& d : defects) {
        cv::Point start  = state.contour[d[0]];
        cv::Point end    = state.contour[d[1]];
        cv::Point far    = state.contour[d[2]];
        float depth      = d[3] / 256.0f;

        if (depth < handRadius * 0.20f || depth > handRadius * 1.3f) continue;

        float a = cv::norm(end - far);
        float b = cv::norm(start - far);
        float c = cv::norm(start - end);
        
        if (a * b == 0) continue;
        float angle = std::acos((a*a + b*b - c*c) / (2*a*b));

        if (angle < CV_PI / 2.0f) { 
            fingers++;
        }
    }

    return (fingers == 0) ? 0 : std::min(fingers + 1, 5);
}

// =============================================
//  ★ 개조된 스와이프 감지 (360도 수학적 방사형 분할) ★
// =============================================
GestureType GestureRecognizer::detectSwipe() {
    if (_trajectory.size() < 4) return GestureType::NONE;

    cv::Point2f start = _trajectory.front();
    cv::Point2f end   = _trajectory.back();

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dist = std::sqrt(dx*
