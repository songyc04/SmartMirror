#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <deque>
#include <functional>
#include <string>
#include <chrono>

enum class GestureType {
    NONE,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN,
    HAND_OPEN,
    HAND_FIST,
};

struct HandState {
    cv::Point2f center;
    float area;
    int fingerCount;
    bool isDetected;
    std::vector<cv::Point> contour;
};

using GestureCallback = std::function<void(GestureType, float)>;

class GestureRecognizer {
public:
    GestureRecognizer();
    ~GestureRecognizer() = default;

    void setGestureCallback(GestureCallback callback);
    GestureType processFrame(cv::Mat& frame);
    void drawDebugOverlay(cv::Mat& frame, const HandState& state);
    void setSensitivity(float sensitivity);
    void setCooldown(int ms);

private:
    HandState detectHand(cv::Mat& frame);
    int countFingers(const HandState& state, const cv::Mat& mask);
    GestureType detectSwipe();
    GestureType detectHandPose(const HandState& state);
    bool isCooldownExpired();
    void resetCooldown();
    std::string gestureToString(GestureType g);

    std::deque<cv::Point2f>  _trajectory;
    HandState                _prevState;
    GestureCallback          _callback;

    float _sensitivity;
    int   _cooldownMs;
    int   _trajectorySize;
    float _swipeThreshold;
    int   _minHandArea;
    int   _maxHandArea;

    // 핵심 수정: 손 제거 대기 플래그
    bool  _waitingForHandRemoval;

    std::chrono::steady_clock::time_point _lastGestureTime;
    cv::Ptr<cv::BackgroundSubtractorMOG2> _bgSubtractor;
    GestureType _lastGesture;
};
