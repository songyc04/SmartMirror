#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <atomic>
#include "gesture_recognition.hpp"

const int PORT        = 9000; // 아두이노 TCP 포트
const int QT_PORT     = 9001; // Qt UDP 포트
const int BUFFER_SIZE = 1024;

// 전역 공유 리소스 및 스레드 제어 플래그
std::atomic<bool> g_running(true);
int g_qt_udp_sock = -1;
sockaddr_in g_qt_addr{};

// 수신 데이터 파싱 및 템플릿 터미널 출력 함수 (기존 유지)
void printParsed(const std::string& data) {
    std::string temp_val = "?";
    std::string humi_val = "?";

    size_t t_pos = data.find("TEMP:");
    size_t h_pos = data.find(",HUMI:");

    if (t_pos != std::string::npos && h_pos != std::string::npos) {
        temp_val = data.substr(t_pos + 5, h_pos - (t_pos + 5));
    }
    if (h_pos != std::string::npos) {
        humi_val = data.substr(h_pos + 6);
        humi_val.erase(humi_val.find_last_not_of(" \n\r\t") + 1);
    }

    std::cout << "┌─────────────────────────┐\n";
    std::cout << "│  온도: " << temp_val << " °C\n";
    std::cout << "│  습도: " << humi_val << " %\n";
    std::cout << "└─────────────────────────┘\n";
}

// [스레드 함수] OpenCV를 구동하여 제스처를 감지하고 Qt로 직접 UDP를 전송하는 루프
void opencv_gesture_worker() {
    std::cout << "[OpenCV 스레드] 제스처 감지 스레드가 시작되었습니다.\n";

    // 젯슨 CSI 카메라용 GStreamer 파이프라인 (안될 경우 cv::VideoCapture cap(0)으로 변경 가능)
    std::string pipeline = "nvarguscamerasrc ! video/x-raw(memory:NVMM), width=640, height=480, format=NV12, framerate=30/1 ! nvvidconv ! video/x-raw, format=BGRx ! videoconvert ! video/x-raw, format=BGR ! appsink";
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    
    if (!cap.isOpened()) {
        std::cerr << "[OpenCV 에러] 카메라를 열 수 없습니다. USB 웹캠 사용 시 인덱스 0으로 재시도합니다...\n";
        cap.open(0);
        if(!cap.isOpened()) {
            std::cerr << "[OpenCV 치명적 에러] 모든 카메라 연결 실패.\n";
            return;
        }
    }

    // 해상도 안전장치 및 최적화 세팅
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);

    GestureRecognizer recognizer;
    recognizer.setSensitivity(1.0f);
    recognizer.setCooldown(800);

    // 제스처 감지 시 실행될 람다 콜백 함수 등록
    recognizer.setGestureCallback([](GestureType g, float confidence) {
        std::string gesture_msg = "";
        switch (g) {
            case GestureType::SWIPE_LEFT:  gesture_msg = "GESTURE:SWIPE_LEFT";  break;
            case GestureType::SWIPE_RIGHT: gesture_msg = "GESTURE:SWIPE_RIGHT"; break;
            case GestureType::SWIPE_UP:    gesture_msg = "GESTURE:SWIPE_UP";    break;
            case GestureType::SWIPE_DOWN:  gesture_msg = "GESTURE:SWIPE_DOWN";  break;
            case GestureType::HAND_OPEN:   gesture_msg = "GESTURE:HAND_OPEN";   break;
            case GestureType::HAND_FIST:   gesture_msg = "GESTURE:HAND_FIST";   break;
            default: return;
        }

        // 즉각 Qt UI가 듣고 있는 UDP 포트로 문자열 메시지 전송
        if (g_qt_udp_sock != -1) {
            sendto(g_qt_udp_sock, gesture_msg.c_str(), gesture_msg.length(), 0,
                   (sockaddr*)&g_qt_addr, sizeof(g_qt_addr));
            std::cout << "[UDP 전송 - 제스처] Qt UI -> " << gesture_msg << "\n";
        }
    });

    cv::Mat frame;
    while (g_running) {
        cap >> frame;
        if (frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 거울 모드 좌우 반전
        cv::flip(frame, frame, 1);

        // 프레임 처리 (내부에서 인식 시 등록한 콜백 함수가 자동 호출됨)
        recognizer.processFrame(frame);

        // 원격 SSH 등 GUI 모니터가 없는 환경에서의 오버헤드 방지를 위해 imshow는 비활성화
        // cv::imshow("Server Camera Debug", frame);
        // if (cv::waitKey(1) == 'q') break;
        
        // 연산량 제어를 위한 미세한 휴식
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    cap.release();
    std::cout << "[OpenCV 스레드] 제스처 감지 스레드가 안전하게 종료되었습니다.\n";
}

int main() {
    // 1. TCP 서버 소켓 생성
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket 생성 실패");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind 실패");
        close(server_fd);
        return 1;
    }

    listen(server_fd, 5);

    // 2. Qt 전송용 UDP 소켓 생성 및 전역 설정
    g_qt_udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    g_qt_addr.sin_family = AF_INET;
    g_qt_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    g_qt_addr.sin_port = htons(QT_PORT);

    std::cout << "[메인 서버] 아두이노 대기 포트: " << PORT << ", Qt UDP 대상 포트: " << QT_PORT << "\n";

    // 3. OpenCV 제스처 감지 워커 스레드 가동
    std::thread opencv_thread(opencv_gesture_worker);

    // 4. 메인 대기/수신 루프 (기존 아두이노 대응 로직 유지)
    while (g_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int conn_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (conn_fd < 0) {
            if (!g_running) break;
            perror("accept 실패");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "\n[메인 서버] 아두이노 연결됨: " << client_ip << "\n";

        char buffer[BUFFER_SIZE];
        while (g_running) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytes = recv(conn_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
                std::cout << "[메인 서버] 아두이노 연결 종료\n";
                break;
            }

            std::string received(buffer);
            std::cout << "[메인 서버] 아두이노 데이터 수신: " << received << "\n";

            // 파싱 결과 출력
            printParsed(received);

            // Qt 프로그램으로 센서 데이터 전송
            sendto(g_qt_udp_sock, buffer, bytes, 0, (sockaddr*)&g_qt_addr, sizeof(g_qt_addr));
        }
        close(conn_fd);
    }

    // 5. 프로세스 정리 및 자원 반환
    g_running = false;
    if (opencv_thread.joinable()) {
        opencv_thread.join();
    }

    close(g_qt_udp_sock);
    close(server_fd);
    std::cout << "[메인 서버] 프로세스가 종료되었습니다.\n";
    return 0;
}
