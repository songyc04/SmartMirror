#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <deque>
#include <functional>
#include <string>
#include <chrono>

// =============================================
//  제스처 타입 정의
// =============================================
enum class GestureType {
    NONE,
    SWIPE_LEFT,       // 좌 스와이프 → 다음 화면
    SWIPE_RIGHT,      // 우 스와이프 → 이전 화면
    SWIPE_UP,         // 위 스와이프 → 볼륨 증가
    SWIPE_DOWN,       // 아래 스와이프 → 볼륨 감소
    HAND_OPEN,        // 손 펼치기 → 재생
    HAND_FIST,        // 주먹 → 정지
};

// =============================================
//  손 상태 구조체
// =============================================
struct HandState {
    cv::Point2f center;         // 손 중심 좌표
    float area;                 // 손 영역 크기
    int fingerCount;            // 펼친 손가락 수
    bool isDetected;            // 손 감지 여부
    std::vector<cv::Point> contour; // 손 윤곽선
};

// =============================================
//  제스처 이벤트 콜백 타입
// =============================================
using GestureCallback = std::function<void(GestureType, float)>;

// =============================================
//  제스처 인식기 클래스
// =============================================
class GestureRecognizer {
public:
    GestureRecognizer();
    ~GestureRecognizer() = default;

    // 콜백 등록
    void setGestureCallback(GestureCallback callback);

    // 메인 처리 함수 (매 프레임 호출)
    GestureType processFrame(cv::Mat& frame);

    // 디버그 오버레이 그리기
    void drawDebugOverlay(cv::Mat& frame, const HandState& state);

    // 설정
    void setSensitivity(float sensitivity);  // 0.5 ~ 2.0
    void setCooldown(int ms);                // 제스처 쿨다운 (ms)

private:
    // 손 감지
    HandState detectHand(cv::Mat& frame);

    // 손가락 수 카운트
    int countFingers(const HandState& state, const cv::Mat& mask);

    // 스와이프 감지
    GestureType detectSwipe();

    // 손 상태 감지 (펼치기/주먹)
    GestureType detectHandPose(const HandState& state);

    // 유틸
    bool isCooldownExpired();
    void resetCooldown();
    std::string gestureToString(GestureType g);

    // 내부 상태
    std::deque<cv::Point2f>  _trajectory;    // 손 이동 궤적
    HandState                _prevState;
    GestureCallback          _callback;

    // 설정값
    float _sensitivity;
    int   _cooldownMs;
    int   _trajectorySize;
    float _swipeThreshold;
    int   _minHandArea;
    int   _maxHandArea;

    // 쿨다운 타이머
    std::chrono::steady_clock::time_point _lastGestureTime;

    // 배경 제거기
    cv::Ptr<cv::BackgroundSubtractorMOG2> _bgSubtractor;

    // 이전 제스처 상태 (중복 방지)
    GestureType _lastGesture;
};
