#include "gesture_recognition.hpp"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

// =============================================
//  생성자
// =============================================
GestureRecognizer::GestureRecognizer()
    : _sensitivity(1.0f)
    , _cooldownMs(600)      
    , _trajectorySize(12)   
    , _swipeThreshold(80.0f) 
    , _minHandArea(2500)   
    , _maxHandArea(70000)   
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
    _swipeThreshold = 40.0f / _sensitivity;
}

void GestureRecognizer::setCooldown(int ms) {
    _cooldownMs = ms;
}

// =============================================
//  메인 처리 루프
// =============================================
GestureType GestureRecognizer::processFrame(cv::Mat& frame) {
    GestureType detected = GestureType::NONE;

    // 1. 손 감지 (이제 상단 제한이 없습니다)
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

    // 4. 움직임이 발생하는지 먼저 체크 (상하좌우 스와이프 판단 우선)
    if (_trajectory.size() >= 4) { 
        detected = detectSwipe();
    }

    // 5. 손이 멈춰있거나 스와이프가 아니라면 정적인 포즈(재생/정지) 분석
    if (detected == GestureType::NONE) {
        detected = detectHandPose(state);
    }

    // 6. 제스처 발동 조건 검증
    if (detected != GestureType::NONE) {
        // [중복 인식 철저 차단 잠금장치]
        // 현재 감지된 제스처가 직전에 성공한 제스처(HAND_OPEN 또는 HAND_FIST)와 동일하다면 
        // 손을 계속 펴고/쥐고 있는 상태이므로 무시하고 이벤트를 발생시키지 않습니다.
        if (detected == _lastGesture && (detected == GestureType::HAND_OPEN || detected == GestureType::HAND_FIST)) {
            return GestureType::NONE; 
        }

        resetCooldown();
        _lastGesture = detected;
        _trajectory.clear(); // 제스처 성공 시 궤적 리셋
        
        if (_callback) _callback(detected, 1.0f);
        std::cout << "[Gesture 발동] " << gestureToString(detected) << std::endl;
    } 
    else {
        // 손가락 수의 변화가 생기거나 손을 움직이기 시작하면 포즈 중복 잠금을 해제하기 위해
        // 현재 상태가 확실히 주먹이나 보가 아닐 때만 _lastGesture를 NONE으로 풀어줍니다.
        if (state.fingerCount > 0 && state.fingerCount < 4) {
            _lastGesture = GestureType::NONE;
        }
    }

    _prevState = state;
    return detected;
}

// =============================================
//  손 감지 (일정 이상 올라가면 안 되던 제한 완벽 삭제)
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

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return state;

    // 가장 큰 윤곽선 순으로 정렬
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
        if (aspectRatio < 0.25f || aspectRatio > 3.5f) continue; 

        cv::Moments M = cv::moments(contour);
        if (M.m00 == 0) continue;
        cv::Point2f center((float)(M.m10 / M.m00), (float)(M.m01 / M.m00));

        HandState tempState;
        tempState.contour = contour;
        tempState.area = area;
        tempState.center = center;
        int fingers = countFingers(tempState, skinMask);

        // 가장 신뢰도 높은 첫 번째 유효 덩어리를 손으로 채택
        bestHandContour = contour;
        bestArea = area;
        bestFingerCount = fingers;
        bestCenter = center;
        break;
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

        if (depth < handRadius * 0.18f || depth > handRadius * 1.4f) continue;

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
//  스와이프 감지 (각도 판정 기반 4방향 균형 분할)
// =============================================
GestureType GestureRecognizer::detectSwipe() {
    if (_trajectory.size() < 4) return GestureType::NONE;

    cv::Point2f start = _trajectory.front();
    cv::Point2f end   = _trajectory.back();

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dist = std::sqrt(dx*dx + dy*dy);

    // 최소 이동거리 기준 만족 여부
    if (dist < _swipeThreshold) return GestureType::NONE;

    // 움직임 벡터 각도 계산 (-180 ~ +180도)
    float angle = std::atan2(dy, dx); 
    float degree = angle * 180.0f / CV_PI;

    // 사방 90도 영역 정확한 라디안 분할 판정
    if (degree >= -45.0f && degree < 45.0f) {
        return GestureType::SWIPE_RIGHT; 
    } 
    else if (degree >= 45.0f && degree < 135.0f) {
        return GestureType::SWIPE_DOWN;  
    } 
    else if (degree >= -135.0f && degree < -45.0f) {
        return GestureType::SWIPE_UP;    
    } 
    else {
        return GestureType::SWIPE_LEFT;  
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

        if (ratio > 0.72f) {
            return GestureType::HAND_FIST;
        }
    }

    return GestureType::NONE;
}

// =============================================
//  디버그 오버레이 (★ green 오타 완전 해결 파트)
// =============================================
void GestureRecognizer::drawDebugOverlay(cv::Mat& frame, const HandState& state) {
    cv::Mat overlay = frame.clone();
    cv::rectangle(overlay, cv::Rect(0, 0, frame.cols, 50), cv::Scalar(0, 0, 0), -1);
    cv::addWeighted(overlay, 0.5, frame, 0.5, 0, frame);

    cv::putText(frame, "FULL SCREEN TRACKING ACTIVE", cv::Point(frame.cols - 220, 28),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);

    std::string label = "Gesture: " + gestureToString(_lastGesture);
    cv::putText(frame, label, cv::Point(10, 33), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 120), 2);

    if (!state.isDetected) return;

    // ★ 이 줄의 'green' 오타를 cv::Scalar(0, 255, 0) 즉, 순수 초록색 값으로 완벽히 고쳤습니다!
    std::vector<std::vector<cv::Point>> contours = {state.contour};
    cv::drawContours(frame, contours, 0, cv::Scalar(0, 255, 0), 2);
    cv::circle(frame, state.center, 8, cv::Scalar(0, 120, 255), -1);

    std::string infoLabel = "Fingers: " + std::to_string(state.fingerCount) + " | Area: " + std::to_string((int)state.area);
    cv::putText(frame, infoLabel, cv::Point((int)state.center.x + 10, (int)state.center.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1, cv::LINE_AA);

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
