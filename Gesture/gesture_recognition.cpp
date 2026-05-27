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
    , _minHandArea(2500)   // 젯슨 환경에 맞게 최소 크기 약간 하향 조정
    , _maxHandArea(55000)  // 거대한 얼굴 덩어리를 차단하기 위해 최대 크기 제한
    , _lastGesture(GestureType::NONE)
{
    _bgSubtractor = cv::createBackgroundSubtractorMOG2(500, 50, false);
    _lastGestureTime = std::chrono::steady_clock::now() -
                       std::chrono::milliseconds(10000);
    _prevState.isDetected = false;
}

// =============================================
//  콜백 등록 및 설정
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

    // 1. 손 감지 (얼굴 필터링 로직 포함)
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
    if (detected == GestureType::NONE && _trajectory.size() >= 8) { // 딜레이를 줄이기 위해 최소 프레임 10->8 수정
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
//  ★ 개조된 손 감지 (얼굴 간섭 원천 차단 버전) ★
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
    // 아시아인 피부색에 가장 적합한 골든 영역 세팅
    cv::inRange(hsv,
        cv::Scalar(0, 30, 60),
        cv::Scalar(20, 180, 255),
        skinMask);

    // 노이즈 제거 (모폴로지 연산 강도 강화)
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_OPEN,  kernel);
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_CLOSE, kernel);

    // [얼굴 차단 패치 1] 화면 상단 25% 영역은 얼굴일 확률이 높으므로 마스크에서 지워버림
    int ignoreHeight = frame.rows * 0.25;
    if(ignoreHeight > 0) {
        cv::rectangle(skinMask, cv::Rect(0, 0, frame.cols, ignoreHeight), cv::Scalar(0), -1);
    }

    // --- 윤곽선 검출 ---
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return state;

    // 크기 순으로 윤곽선 정렬 (큰 덩어리부터 검사)
    std::sort(contours.begin(), contours.end(), [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
        return cv::contourArea(a) > cv::contourArea(b);
    });

    std::vector<cv::Point> bestHandContour;
    float bestArea = 0;
    int bestFingerCount = 0;
    cv::Point2f bestCenter;

    // [얼굴 차단 패치 2] 무조건 제일 큰 걸 고르지 않고, 손의 조건을 만족하는 덩어리를 탐색
    for (const auto& contour : contours) {
        float area = (float)cv::contourArea(contour);
        
        // 너무 작거나 너무 큰(얼굴+몸통) 덩어리는 스킵
        if (area < _minHandArea || area > _maxHandArea) continue;

        // 가로세로 비율(Bounding Box) 분석
        cv::Rect bRect = cv::boundingRect(contour);
        float aspectRatio = (float)bRect.width / bRect.height;
        
        // 극단적으로 길쭉한 형태(목, 팔 단독 검출 등)는 손이 아니므로 제외
        if (aspectRatio < 0.4f || aspectRatio > 2.5f) continue;

        // 모멘트 중심점 계산
        cv::Moments M = cv::moments(contour);
        if (M.m00 == 0) continue;
        cv::Point2f center((float)(M.m10 / M.m00), (float)(M.m01 / M.m00));

        // 가짜 손 상태 빌드 후 손가락 검증
        HandState tempState;
        tempState.contour = contour;
        tempState.area = area;
        tempState.center = center;
        int fingers = countFingers(tempState, skinMask);

        // [얼굴 차단 패치 3] 얼굴은 볼록 결함 특성상 손가락이 0개가 나오거나 형태가 깨짐
        // 화면 하단쪽에 위치하며 손가락 트래킹 규칙에 근접한 최적의 덩어리 매칭
        if (bestHandContour.empty()) {
            bestHandContour = contour;
            bestArea = area;
            bestFingerCount = fingers;
            bestCenter = center;
        } 
        // 만약 두 번째 덩어리가 손가락이 더 명확하고 아래쪽에 있다면 그걸 손으로 체칭!
        else if (fingers > 0 && center.y > bestCenter.y) {
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

// =============================================
//  손가락 수 카운트 (정밀도 개선)
// =============================================
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

        // 결함 깊이가 손 반경의 최소 25% 이상이어야 손가락 골짜기로 인정
        if (depth < handRadius * 0.25f || depth > handRadius * 1.2f) continue;

        float a = cv::norm(end - far);
        float b = cv::norm(start - far);
        float c = cv::norm(start - end);
        
        if (a * b == 0) continue;
        float angle = std::acos((a*a + b*b - c*c) / (2*a*b));

        // 손가락 사이 각도는 대략 85도 이내 (피부 접힘 오인식 방지)
        if (angle < CV_PI / 2.1f) {
            fingers++;
        }
    }

    // 아무것도 감지 안되었을 때 주먹 상태 검증용 0반환, 그 외엔 손가락 수 매칭
    return (fingers == 0) ? 0 : std::min(fingers + 1, 5);
}

// =============================================
//  스와이프 감지 (상하좌우 정확도 튜닝)
// =============================================
GestureType GestureRecognizer::detectSwipe() {
    if (_trajectory.size() < 6) return GestureType::NONE;

    cv::Point2f start = _trajectory.front();
    cv::Point2f end   = _trajectory.back();

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dist = std::sqrt(dx*dx + dy*dy);

    // 스와이프 최소 거리 임계값 체크
    if (dist < _swipeThreshold) return GestureType::NONE;

    // 대각선 움직임 애매함을 차단하기 위한 방향 비율 체크 (한쪽 축이 최소 1.5배 이상 우세해야 함)
    if (std::abs(dx) > std::abs(dy) * 1.5f) {
        // 확실한 수평 스와이프
        return (dx < 0) ? GestureType::SWIPE_LEFT : GestureType::SWIPE_RIGHT;
    } else if (std::abs(dy) > std::abs(dx) * 1.5f) {
        // 확실한 수직 스와이프
        return (dy < 0) ? GestureType::SWIPE_UP : GestureType::SWIPE_DOWN;
    }

    return GestureType::NONE;
}

// =============================================
//  손 포즈 감지 (펼치기 vs 주먹)
// =============================================
GestureType GestureRecognizer::detectHandPose(const HandState& state) {
    if (state.fingerCount >= 4) {
        if (_lastGesture != GestureType::HAND_OPEN)
            return GestureType::HAND_OPEN;
    }
    else if (state.fingerCount == 0) {
        std::vector<cv::Point> hull;
        cv::convexHull(state.contour, hull);
        float hullArea = (float)cv::contourArea(hull);
        float ratio    = state.area / (hullArea + 1e-6f);

        // 꽉 찬 주먹 형태의 비율 매칭
        if (ratio > 0.75f && _lastGesture != GestureType::HAND_FIST)
            return GestureType::HAND_FIST;
    }

    return GestureType::NONE;
}

// =============================================
//  디버그 오버레이 (상단 차단 가이드라인 추가)
// =============================================
void GestureRecognizer::drawDebugOverlay(cv::Mat& frame, const HandState& state) {
    cv::Mat overlay = frame.clone();
    cv::rectangle(overlay, cv::Rect(0, 0, frame.cols, 50), cv::Scalar(0, 0, 0), -1);
    
    // [얼굴 인식 차단 존 표시] 화면 상단 25% 노란색 점선 마킹
    int ignoreHeight = frame.rows * 0.25;
    cv::line(frame, cv::Point(0, ignoreHeight), cv::Point(frame.cols, ignoreHeight), cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    cv::putText(frame, "FACE IGNORE ZONE", cv::Point(frame.cols - 150, ignoreHeight - 5), 
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 255), 1);

    cv::addWeighted(overlay, 0.5, frame, 0.5, 0, frame);

    std::string label = "Gesture: " + gestureToString(_lastGesture);
    cv::putText(frame, label, cv::Point(10, 33),
                cv::FONT_HERSHEY_SIMPLEX, 0.8,
                cv::Scalar(0, 255, 120), 2);

    if (!state.isDetected) return;

    std::vector<std::vector<cv::Point>> contours = {state.contour};
    cv::drawContours(frame, contours, 0, cv::Scalar(0, 255, 0), 2);
    cv::circle(frame, state.center, 8, cv::Scalar(0, 120, 255), -1);

    std::string fingerLabel = "Fingers: " + std::to_string(state.fingerCount);
    cv::putText(frame, fingerLabel,
                cv::Point((int)state.center.x + 10, (int)state.center.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);

    for (int i = 1; i < (int)_trajectory.size(); i++) {
        float alpha = (float)i / _trajectory.size();
        cv::Scalar color(0, (int)(255 * alpha), (int)(255 * (1 - alpha)));
        cv::line(frame, _trajectory[i-1], _trajectory[i], color, 3); // 가시성을 위해 두께 2->3 조절
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
