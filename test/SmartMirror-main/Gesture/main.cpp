#include "gesture_recognition.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

class SmartMirrorSimulator {
public:
    SmartMirrorSimulator() : _currentPage(0), _volume(50), _isPlaying(false) {
        _pages = {"Clock/Weather", "News Feed", "Calendar", "Music Player"};
    }

    void onGesture(GestureType gesture, float confidence) {
        switch (gesture) {
            case GestureType::SWIPE_LEFT:
                _currentPage = (_currentPage + 1) % (int)_pages.size();
                std::cout << ">>> Page: " << _pages[_currentPage] << std::endl;
                break;
            case GestureType::SWIPE_RIGHT:
                _currentPage = (_currentPage - 1 + (int)_pages.size()) % (int)_pages.size();
                std::cout << ">>> Page: " << _pages[_currentPage] << std::endl;
                break;
            case GestureType::SWIPE_UP:
                _volume = std::min(100, _volume + 10);
                std::cout << ">>> Volume: " << _volume << "%" << std::endl;
                break;
            case GestureType::SWIPE_DOWN:
                _volume = std::max(0, _volume - 10);
                std::cout << ">>> Volume: " << _volume << "%" << std::endl;
                break;
            case GestureType::HAND_OPEN:
                _isPlaying = true;
                std::cout << ">>> Playing" << std::endl;
                break;
            case GestureType::HAND_FIST:
                _isPlaying = false;
                std::cout << ">>> Stopped" << std::endl;
                break;
            default: break;
        }
    }

    void drawUI(cv::Mat& canvas) {
        canvas = cv::Mat::zeros(200, 640, CV_8UC3);
        cv::rectangle(canvas, cv::Rect(0,0,640,200), cv::Scalar(20,20,30), -1);

        std::string pageText = "[ " + _pages[_currentPage] + " ]";
        cv::putText(canvas, pageText, cv::Point(20,50),
                    cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(200,220,255), 2);

        int dotX = 20;
        for (int i = 0; i < (int)_pages.size(); i++) {
            cv::Scalar color = (i == _currentPage)
                ? cv::Scalar(0,200,255)
                : cv::Scalar(80,80,80);
            cv::circle(canvas, cv::Point(dotX, 80), 6, color, -1);
            dotX += 20;
        }

        cv::putText(canvas, "VOL", cv::Point(20,130),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(150,150,150), 1);
        cv::rectangle(canvas, cv::Rect(70,115,200,20), cv::Scalar(50,50,50), -1);
        cv::rectangle(canvas, cv::Rect(70,115,_volume*2,20), cv::Scalar(0,200,120), -1);
        cv::putText(canvas, std::to_string(_volume)+"%",
                    cv::Point(280,130),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200,200,200), 1);

        std::string playStatus = _isPlaying ? "> PLAYING" : "| STOPPED";
        cv::Scalar playColor = _isPlaying
            ? cv::Scalar(0,255,100)
            : cv::Scalar(0,80,200);
        cv::putText(canvas, playStatus, cv::Point(20,175),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, playColor, 2);

        cv::putText(canvas,
            "Swipe L/R:Page  Swipe U/D:Vol  Open:Play  Fist:Stop  Q:Quit",
            cv::Point(20, 195),
            cv::FONT_HERSHEY_SIMPLEX, 0.35,
            cv::Scalar(100,100,100), 1);
    }

private:
    std::vector<std::string> _pages;
    int  _currentPage;
    int  _volume;
    bool _isPlaying;
};

int main(int argc, char** argv) {
    std::cout << "==================================" << std::endl;
    std::cout << "  스마트 미러 제스처 인식 시스템  " << std::endl;
    std::cout << "==================================" << std::endl;

    int camIndex = (argc > 1) ? std::stoi(argv[1]) : 0;

    cv::VideoCapture cap(camIndex);
    if (!cap.isOpened()) {
        std::cerr << "[ERROR] 카메라를 열 수 없습니다. (index=" << camIndex << ")" << std::endl;
        std::cerr << "        다른 인덱스를 시도하세요: ./gesture_mirror 1" << std::endl;
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    std::cout << "[INFO] 카메라 시작 (index=" << camIndex << ")" << std::endl;
    std::cout << "[INFO] 'q' 키를 누르면 종료합니다." << std::endl;

    GestureRecognizer recognizer;
    recognizer.setSensitivity(1.0f);
    recognizer.setCooldown(800);

    SmartMirrorSimulator mirror;
    recognizer.setGestureCallback(
        [&mirror](GestureType g, float conf) {
            mirror.onGesture(g, conf);
        }
    );

    cv::Mat frame, mirrorUI;

    while (true) {
        cap >> frame;
        if (frame.empty()) continue;

        cv::flip(frame, frame, 1);
        recognizer.processFrame(frame);
        mirror.drawUI(mirrorUI);

        cv::imshow("Camera", frame);
        cv::imshow("Smart Mirror", mirrorUI);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) {
            std::cout << "[INFO] 종료합니다." << std::endl;
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
