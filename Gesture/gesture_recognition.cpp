#include "gesture_recognition.hpp"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

// =============================================
//  생성자
// =============================================
GestureRecognizer::GestureRecognizer()
    : _sensitivity(1.0f)
    , _cooldownMs(1200)    // 중복 인식 방지를 위해 쿨다운 시간을 0.8초 -> 1.2초로 상향
    , _trajectorySize(15)  // 궤적 반응 속도를 높이기 위해 프레임 수 축소 (20 -> 15)
    , _swipeThreshold(60.0f) // 스와이프 거리 진입 장벽 완화 (80 -> 60)
    , _minHandArea(2500)   
    , _maxHandArea(55000)  
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
    _swipeThreshold = 60.0f / _sensitivity;
}

void GestureRecognizer::setCooldown(int ms) {
    _cooldownMs = ms;
}

// =============================================
//  메인 처리 루프
// =============================================
GestureType GestureRecognizer::processFrame(cv::Mat& frame) {
    GestureType detected = GestureType::NONE;

    // 1. 손 감지
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

    // ★ [수정 포인트 1] 글로벌 쿨다운 체크를 최상단으로 이동
    // 제스처가 한 번 터진 후 쿨다운이 끝나지 않았다면 연산을 스킵하여 중복 입력을 원천 차단
    if (!isCooldownExpired()) {
        _prevState = state;
        return GestureType::NONE;
    }

    // 4. 손 포즈 감지 (펼치기/주먹)
    detected = detectHandPose(state);

    // 5. 스와이프 감지 (포즈가 NONE일 때만 궤적 분석)
    if (detected == GestureType::NONE && _trajectory.size() >= 6) { 
        detected = detectSwipe();
    }

    // 6. 콜백 발동 및 상태 유지
    // ★ [수정 포인트 2] HAND_OPEN이나 FIST가 연속으로 중복 인식을 때리는 것 방지
    if (detected != GestureType::NONE) {
        if (detected == _lastGesture && (detected == GestureType::HAND_OPEN || detected == GestureType::HAND_FIST)) {
            // 이미 재생 중이거나 정지 중이면 이벤트를 다시 발생시키지 않음
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
//  손 감지 (얼굴 영역 무시 포함)
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

    // [얼굴 영역 무시] 상단 25% 커트
    int ignoreHeight = frame.rows * 0.25;
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
        if (aspectRatio < 0.3f || aspectRatio > 3.0f) continue; // 비율 마진 확대

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

        if (angle < CV_PI / 2.0f) { // 각도 제한 소폭 완화
            fingers++;
        }
    }

    return (fingers == 0) ? 0 : std::min(fingers + 1, 5);
}

// =============================================
//  ★ 개조된 스와이프 감지 (상하좌우 밸런스 패치) ★
// =============================================
GestureType GestureRecognizer::detectSwipe() {
    if (_trajectory.size() < 5) return GestureType::NONE;

    cv::Point2f start = _trajectory.front();
    cv::Point2f end   = _trajectory.back();

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dist = std::sqrt(dx*dx + dy*dy);

    // 최소 이동 거리 미달 시 기각
    if (dist < _swipeThreshold) return GestureType::NONE;

    // ★ [수정 포인트 3] 타이트한 비율 조건(1.5배)을 없애고 
    // 절대값 크기 비교 및 부드러운 가중치(1.1배)로 모든 방향이 고르게 인식되도록 수정
    if (std::abs(dx) > std::abs(dy) * 1.1f) {
        // 좌우 우세
        return (dx < 0) ? GestureType::SWIPE_LEFT : GestureType::SWIPE_RIGHT;
    } 
    else if (std::abs(dy) > std::abs(dx) * 1.1f) {
        // 상하 우세
        return (dy < 0) ? GestureType::SWIPE_UP : GestureType::SWIPE_DOWN;
    }

    return GestureType::NONE;
}

// =============================================
//  손 포즈 감지
// =============================================
GestureType GestureRecognizer::detectHandPose(const HandState& state) {
    if (state.fingerCount >= 4) {
        return GestureType::HAND_OPEN;
    }
    else if (state.fingerCount == 0) {
        std::vector<cv::Point> hull;
        cv::convexHull(state.contour, hull);
        float hullArea = (float)cv::contourArea(hull);
        float ratio    = state.area / (hullArea + 1e-6f);

        if (ratio > 0.70f) // 주먹 인식 감도 향상 (0.75 -> 0.70)
            return GestureType::HAND_FIST;
    }

    return GestureType::NONE;
}

// =============================================
//  디버그 오버레이 및 유틸
// =============================================
void GestureRecognizer::drawDebugOverlay(cv::Mat& frame, const HandState& state) {
    cv::Mat overlay = frame.clone();
    cv::rectangle(overlay, cv::Rect(0, 0, frame.cols, 50), cv::Scalar(0, 0, 0), -1);
    
    int ignoreHeight = frame.rows * 0.25;
    cv::line(frame, cv::Point(0, ignoreHeight), cv::Point(frame.cols, ignoreHeight), cv::Scalar(0, 255, 255), 1, cv::LINE_AA);

    cv::addWeighted(overlay, 0.5, frame, 0.5, 0, frame);

    std::string label = "Gesture: " + gestureToString(_lastGesture);
    cv::putText(frame, label, cv::Point(10, 33), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 120), 2);

    if (!state.isDetected) return;

    std::vector<std::vector<cv::Point>> contours = {state.contour};
    cv::drawContours(frame, contours, 0, cv::Scalar(0, 255, 0), 2);
    cv::circle(frame, state.center, 8, cv::Scalar(0, 120, 255), -1);

    std::string fingerLabel = "Fingers: " + std::to_string(state.fingerCount);
    cv::putText(frame, fingerLabel, cv::Point((int)state.center.x + 10, (int)state.center.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);

    for (int i = 1; i < (int)_trajectory.size(); i++) {
        float alpha = (float)i / _trajectory.size();
        cv::Scalar color(0, (int)(255 * alpha), (int)(255 * (1 - alpha)));
        cv::line(frame, _trajectory[i-1], _trajectory[i], color, 3);
    }
}

bool GestureRecognizer::isCooldownExpired() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastGestureTime).count();
    return elapsed >= _cooldownMs;
}

void GestureRecognizer::resetCooldown() {
    _lastGestureTime = std::chrono::steady_clock::now();
}

std::string GestureRecognizer::gestureToString(GestureType g) {
    switch (g) {
        case GestureType::SWIPE_LEFT:  return "SWIPE LEFT  (다음 화면)";
        case GestureType::SWIPE_RIGHT: return "SWIPE RIGHT (이전 화면)";
        case GestureType::SWIPE_UP:    return "SWIPE UP    (볼륨 증가)";
        case GestureType::SWIPE_DOWN:  return "SWIPE DOWN  (볼륨 감소)";
        case GestureType::HAND_OPEN:   return "HAND OPEN   (재생)";
        case GestureType::HAND_FIST:   return "HAND FIST   (정지)";
        default:                       return "NONE";
    }
}
