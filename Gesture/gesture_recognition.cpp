#include "gesture_recognition.hpp"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

// =============================================
//  생성자
// =============================================
GestureRecognizer::GestureRecognizer()
    : _sensitivity(1.0f)
    , _cooldownMs(800)
    , _trajectorySize(20)
    , _swipeThreshold(80.0f)
    , _minHandArea(3000)
    , _maxHandArea(80000)
    , _lastGesture(GestureType::NONE)
{
    _bgSubtractor = cv::createBackgroundSubtractorMOG2(500, 50, false);
    _lastGestureTime = std::chrono::steady_clock::now() -
                       std::chrono::milliseconds(10000);
    _prevState.isDetected = false;
}

// =============================================
//  콜백 등록
// =============================================
void GestureRecognizer::setGestureCallback(GestureCallback callback) {
    _callback = callback;
}

void GestureRecognizer::setSensitivity(float s) {
    _sensitivity = std::max(0.5f, std::min(2.0f, s));
    _swipeThreshold = 80.0f / _sensitivity;
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

    // 4. 쿨다운 체크
    if (!isCooldownExpired()) {
        _prevState = state;
        return GestureType::NONE;
    }

    // 5. 손 포즈 감지 (펼치기/주먹)
    detected = detectHandPose(state);

    // 6. 스와이프 감지 (궤적 기반)
    if (detected == GestureType::NONE && _trajectory.size() >= 10) {
        detected = detectSwipe();
    }

    // 7. 콜백 및 쿨다운 리셋
    if (detected != GestureType::NONE && detected != _lastGesture) {
        resetCooldown();
        _lastGesture = detected;
        _trajectory.clear();
        if (_callback) _callback(detected, 1.0f);
        std::cout << "[Gesture] " << gestureToString(detected) << std::endl;
    } else if (detected == GestureType::NONE) {
        _lastGesture = GestureType::NONE;
    }

    _prevState = state;
    return detected;
}

// =============================================
//  손 감지 (HSV 피부색 + 배경 제거 결합)
// =============================================
HandState GestureRecognizer::detectHand(cv::Mat& frame) {
    HandState state;
    state.isDetected = false;
    state.area = 0;
    state.fingerCount = 0;

    cv::Mat blurred;
    cv::GaussianBlur(frame, blurred, cv::Size(7, 7), 0);

    // --- HSV 피부색 마스크 ---
    cv::Mat hsv;
    cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);

    cv::Mat skinMask;
    cv::inRange(hsv,
        cv::Scalar(0, 20, 70),
        cv::Scalar(20, 255, 255),
        skinMask);

    // 노이즈 제거
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7,7));
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_OPEN,  kernel);
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_CLOSE, kernel);
    cv::GaussianBlur(skinMask, skinMask, cv::Size(5,5), 0);

    // --- 윤곽선 검출 ---
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return state;

    // 가장 큰 윤곽선 선택
    auto maxIt = std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });

    float area = (float)cv::contourArea(*maxIt);
    if (area < _minHandArea || area > _maxHandArea) return state;

    // 손 중심 계산 (모멘트)
    cv::Moments M = cv::moments(*maxIt);
    if (M.m00 == 0) return state;

    state.isDetected  = true;
    state.area        = area;
    state.contour     = *maxIt;
    state.center      = cv::Point2f((float)(M.m10 / M.m00),
                                    (float)(M.m01 / M.m00));
    state.fingerCount = countFingers(state, skinMask);

    return state;
}

// =============================================
//  손가락 수 카운트 (볼록 결함 분석)
// =============================================
int GestureRecognizer::countFingers(const HandState& state, const cv::Mat& mask) {
    if (state.contour.size() < 10) return 0;

    // 볼록 껍질
    std::vector<int> hullIdx;
    cv::convexHull(state.contour, hullIdx);

    if (hullIdx.size() < 3) return 0;

    // 볼록 결함 계산
    std::vector<cv::Vec4i> defects;
    cv::convexityDefects(state.contour, hullIdx, defects);

    int fingers = 0;
    float handRadius = std::sqrt(state.area / CV_PI);

    for (const auto& d : defects) {
        cv::Point start  = state.contour[d[0]];
        cv::Point end    = state.contour[d[1]];
        cv::Point far    = state.contour[d[2]];
        float depth      = d[3] / 256.0f;

        if (depth < handRadius * 0.3f) continue;

        // 손가락 각도 계산
        float a = cv::norm(end - far);
        float b = cv::norm(start - far);
        float c = cv::norm(start - end);
        float angle = std::acos((a*a + b*b - c*c) / (2*a*b + 1e-6f));

        if (angle < CV_PI / 2.2f) {
            fingers++;
        }
    }

    // 볼록 결함 수 + 1 = 손가락 수 (최대 5)
    return std::min(fingers + 1, 5);
}

// =============================================
//  스와이프 감지 (궤적 분석)
// =============================================
GestureType GestureRecognizer::detectSwipe() {
    if (_trajectory.size() < 8) return GestureType::NONE;

    cv::Point2f start = _trajectory.front();
    cv::Point2f end   = _trajectory.back();

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dist = std::sqrt(dx*dx + dy*dy);

    if (dist < _swipeThreshold) return GestureType::NONE;

    // 방향 판별
    if (std::abs(dx) > std::abs(dy)) {
        // 수평 스와이프
        return (dx < 0) ? GestureType::SWIPE_LEFT : GestureType::SWIPE_RIGHT;
    } else {
        // 수직 스와이프
        return (dy < 0) ? GestureType::SWIPE_UP : GestureType::SWIPE_DOWN;
    }
}

// =============================================
//  손 포즈 감지 (펼치기 vs 주먹)
// =============================================
GestureType GestureRecognizer::detectHandPose(const HandState& state) {
    // 손가락 5개 → 손 펼치기 (재생)
    if (state.fingerCount >= 4) {
        if (_lastGesture != GestureType::HAND_OPEN)
            return GestureType::HAND_OPEN;
    }
    // 손가락 0개 → 주먹 (정지)
    else if (state.fingerCount == 0) {
        // 볼록 껍질 비율로 주먹 확인
        std::vector<cv::Point> hull;
        cv::convexHull(state.contour, hull);
        float hullArea = (float)cv::contourArea(hull);
        float ratio    = state.area / (hullArea + 1e-6f);

        if (ratio > 0.85f && _lastGesture != GestureType::HAND_FIST)
            return GestureType::HAND_FIST;
    }

    return GestureType::NONE;
}

// =============================================
//  디버그 오버레이
// =============================================
void GestureRecognizer::drawDebugOverlay(cv::Mat& frame, const HandState& state) {
    // 반투명 상단 바
    cv::Mat overlay = frame.clone();
    cv::rectangle(overlay, cv::Rect(0, 0, frame.cols, 50),
                  cv::Scalar(0, 0, 0), -1);
    cv::addWeighted(overlay, 0.5, frame, 0.5, 0, frame);

    // 제스처 이름 표시
    std::string label = "Gesture: " + gestureToString(_lastGesture);
    cv::putText(frame, label, cv::Point(10, 33),
                cv::FONT_HERSHEY_SIMPLEX, 0.8,
                cv::Scalar(0, 255, 120), 2);

    if (!state.isDetected) return;

    // 윤곽선
    std::vector<std::vector<cv::Point>> contours = {state.contour};
    cv::drawContours(frame, contours, 0, cv::Scalar(0, 255, 0), 2);

    // 손 중심점
    cv::circle(frame, state.center, 8, cv::Scalar(0, 120, 255), -1);

    // 손가락 수
    std::string fingerLabel = "Fingers: " + std::to_string(state.fingerCount);
    cv::putText(frame, fingerLabel,
                cv::Point((int)state.center.x + 10, (int)state.center.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);

    // 궤적
    for (int i = 1; i < (int)_trajectory.size(); i++) {
        float alpha = (float)i / _trajectory.size();
        cv::Scalar color(0, (int)(255 * alpha), (int)(255 * (1 - alpha)));
        cv::line(frame, _trajectory[i-1], _trajectory[i], color, 2);
    }
}

// =============================================
//  유틸 함수들
// =============================================
bool GestureRecognizer::isCooldownExpired() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - _lastGestureTime).count();
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
