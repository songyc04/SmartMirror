#include "gesture_recognition.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

// =============================================
//  스마트 미러 시뮬레이터 (테스트용 UI)
// =============================================
class SmartMirrorSimulator {
public:
    SmartMirrorSimulator() : _currentPage(0), _volume(50), _isPlaying(false) {
        _pages = {"🕐 시계 / 날씨", "📰 뉴스 피드", "📅 캘린더", "🎵 음악 플레이어", "💪 운동 정보"};
    }

    void onGesture(GestureType gesture, float confidence) {
        switch (gesture) {
            case GestureType::SWIPE_LEFT:
                _currentPage = (_currentPage + 1) % (int)_pages.size();
                std::cout << ">>> 화면 전환: " << _pages[_currentPage] << std::endl;
                break;
            case GestureType::SWIPE_RIGHT:
                _currentPage = (_currentPage - 1 + (int)_pages.size()) % (int)_pages.size();
                std::cout << ">>> 화면 전환: " << _pages[_currentPage] << std::endl;
                break;
            case GestureType::SWIPE_UP:
                _volume = std::min(100, _volume + 10);
                std::cout << ">>> 볼륨: " << _volume << "%" << std::endl;
                break;
            case GestureType::SWIPE_DOWN:
                _volume = std::max(0, _volume - 10);
                std::cout << ">>> 볼륨: " << _volume << "%" << std::endl;
                break;
            case GestureType::HAND_OPEN:
                _isPlaying = true;
                std::cout << ">>> 재생 시작" << std::endl;
                break;
            case GestureType::HAND_FIST:
                _isPlaying = false;
                std::cout << ">>> 정지" << std::endl;
                break;
            default: break;
        }
    }

    void drawUI(cv::Mat& canvas) {
        canvas = cv::Mat::zeros(200, 640, CV_8UC3);

        // 배경
        cv::rectangle(canvas, cv::Rect(0, 0, 640, 200),
                      cv::Scalar(20, 20, 30), -1);

        // 페이지 표시
        std::string pageText = "[ " + _pages[_currentPage] + " ]";
        cv::putText(canvas, pageText, cv::Point(20, 50),
                    cv::FONT_HERSHEY_SIMPLEX, 0.9,
                    cv::Scalar(200, 220, 255), 2);

        // 페이지 인디케이터
        int dotX = 20;
        for (int i = 0; i < (int)_pages.size(); i++) {
            cv::Scalar color = (i == _currentPage)
                ? cv::Scalar(0, 200, 255)
                : cv::Scalar(80, 80, 80);
            cv::circle(canvas, cv::Point(dotX, 80), 6, color, -1);
            dotX += 20;
        }

        // 볼륨 바
        cv::putText(canvas, "VOL", cv::Point(20, 130),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(150,150,150), 1);
        cv::rectangle(canvas, cv::Rect(70, 115, 200, 20),
                      cv::Scalar(50,50,50), -1);
        cv::rectangle(canvas, cv::Rect(70, 115, _volume * 2, 20),
                      cv::Scalar(0, 200, 120), -1);
        cv::putText(canvas, std::to_string(_volume) + "%",
                    cv::Point(280, 130),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200,200,200), 1);

        // 재생 상태
        std::string playStatus = _isPlaying ? "▶ PLAYING" : "■ STOPPED";
        cv::Scalar playColor = _isPlaying
            ? cv::Scalar(0, 255, 100)
            : cv::Scalar(0, 80, 200);
        cv::putText(canvas, playStatus, cv::Point(20, 175),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, playColor, 2);

        // 조작 안내
        cv::putText(canvas,
            "← → 스와이프:화면전환  ↑↓ 스와이프:볼륨  손펼치기:재생  주먹:정지  Q:종료",
            cv::Point(20, 195),
            cv::FONT_HERSHEY_SIMPLEX, 0.35,
            cv::Scalar(100, 100, 100), 1);
    }

private:
    std::vector<std::string> _pages;
    int  _currentPage;
    int  _volume;
    bool _isPlaying;
};

// =============================================
//  메인 함수
// =============================================
int main(int argc, char** argv) {
    std::cout << "==================================" << std::endl;
    std::cout << "  스마트 미러 제스처 인식 시스템  " << std::endl;
    std::cout << "==================================" << std::endl;

    // 카메라 인덱스 (기본 0, 인자로 변경 가능)
    int camIndex = (argc > 1) ? std::stoi(argv[1]) : 0;

    cv::VideoCapture cap(camIndex);
    if (!cap.isOpened()) {
        std::cerr << "[ERROR] 카메라를 열 수 없습니다. (index=" << camIndex << ")" << std::endl;
        std::cerr << "        다른 인덱스를 시도하세요: ./gesture_mirror 1" << std::endl;
        return -1;
    }

    // 카메라 해상도 설정
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    std::cout << "[INFO] 카메라 시작 (index=" << camIndex << ")" << std::endl;
    std::cout << "[INFO] 'q' 키를 누르면 종료합니다." << std::endl;
    std::cout << "[INFO] 's' 키로 민감도 조절, 'd' 키로 디버그 모드 전환" << std::endl;

    // 제스처 인식기 초기화
    GestureRecognizer recognizer;
    recognizer.setSensitivity(1.0f);
    recognizer.setCooldown(800);

    // 스마트 미러 시뮬레이터
    SmartMirrorSimulator mirror;

    // 콜백 등록
    recognizer.setGestureCallback(
        [&mirror](GestureType g, float conf) {
            mirror.onGesture(g, conf);
        }
    );

    cv::Mat frame, mirrorUI;
    bool debugMode = true;

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "[WARN] 빈 프레임 수신" << std::endl;
            continue;
        }

        // 좌우 반전 (거울 모드)
        cv::flip(frame, frame, 1);

        // 제스처 처리
        recognizer.processFrame(frame);

        // 미러 UI 그리기
        mirror.drawUI(mirrorUI);

        // 창 표시
        if (debugMode) {
            cv::imshow("Smart Mirror - Camera", frame);
        }
        cv::imshow("Smart Mirror - Display", mirrorUI);

        // 키 입력 처리
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) {
            std::cout << "[INFO] 종료합니다." << std::endl;
            break;
        } else if (key == 'd') {
            debugMode = !debugMode;
            if (!debugMode) cv::destroyWindow("Smart Mirror - Camera");
            std::cout << "[INFO] 디버그 모드: " << (debugMode ? "ON" : "OFF") << std::endl;
        } else if (key == '+' || key == '=') {
            recognizer.setSensitivity(1.5f);
            std::cout << "[INFO] 민감도 증가" << std::endl;
        } else if (key == '-') {
            recognizer.setSensitivity(0.7f);
            std::cout << "[INFO] 민감도 감소" << std::endl;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
