#pragma once

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>
#include <QFrame>
#include <QProcess>
#include <QThread>
#include <QMutex>

// ── OpenCV 관련 헤더 ──────────────────────────
#include <opencv2/opencv.hpp>
#include "gesture_recognition.hpp"

// ── 소켓 관련 헤더 ────────────────────────────
#include <sys/socket.h>
#include <arpa/inet.h>
#include <atomic>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ============================================================
//  GestureWorker: OpenCV 제스처 감지를 별도 스레드에서 실행
//  QThread를 상속받아 run()에서 카메라 루프를 돌립니다.
// ============================================================
class GestureWorker : public QThread
{
    Q_OBJECT

public:
    explicit GestureWorker(int qtUdpSock, sockaddr_in qtAddr, QObject* parent = nullptr)
        : QThread(parent)
        , _qtUdpSock(qtUdpSock)
        , _qtAddr(qtAddr)
        , _running(false)
    {}

    // 외부에서 안전하게 스레드 종료 요청
    void stop() { _running = false; }

signals:
    // 제스처 감지 시 Qt 메인스레드로 신호 전달
    void gestureDetected(const QString& gesture);

    // 카메라 오류 시 신호
    void cameraError(const QString& message);

protected:
    // ── 스레드 메인 루프 ────────────────────────
    void run() override
    {
        qDebug() << "[GestureWorker] 제스처 감지 스레드 시작";

        // 카메라 열기 (V4L2 백엔드 우선, 실패 시 기본)
        cv::VideoCapture cap(0, cv::CAP_V4L2);
        if (!cap.isOpened()) {
            cap.open(0);
            if (!cap.isOpened()) {
                emit cameraError("카메라를 열 수 없습니다.");
                return;
            }
        }

        cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FPS, 30);

        GestureRecognizer recognizer;
        recognizer.setSensitivity(1.0f);
        recognizer.setCooldown(800);

        // 제스처 콜백: UDP 전송 + Qt 시그널 발행
        recognizer.setGestureCallback([this](GestureType g, float /*confidence*/) {
            QString gesture_msg;
            switch (g) {
                case GestureType::SWIPE_LEFT:  gesture_msg = "GESTURE:SWIPE_LEFT";  break;
                case GestureType::SWIPE_RIGHT: gesture_msg = "GESTURE:SWIPE_RIGHT"; break;
                case GestureType::SWIPE_UP:    gesture_msg = "GESTURE:SWIPE_UP";    break;
                case GestureType::SWIPE_DOWN:  gesture_msg = "GESTURE:SWIPE_DOWN";  break;
                case GestureType::HAND_OPEN:   gesture_msg = "GESTURE:HAND_OPEN";   break;
                case GestureType::HAND_FIST:   gesture_msg = "GESTURE:HAND_FIST";   break;
                default: return;
            }

            // UDP로 Qt UI에 전송
            if (_qtUdpSock != -1) {
                QByteArray msg = gesture_msg.toUtf8();
                sendto(_qtUdpSock, msg.constData(), msg.size(), 0,
                       (sockaddr*)&_qtAddr, sizeof(_qtAddr));
                qDebug() << "[UDP 전송] Qt UI ->" << gesture_msg;
            }

            // Qt 시그널로도 전달 (메인스레드 UI 직접 업데이트 가능)
            emit gestureDetected(gesture_msg);
        });

        _running = true;
        cv::Mat frame;

        while (_running) {
            cap >> frame;
            if (frame.empty()) {
                msleep(10);
                continue;
            }

            cv::flip(frame, frame, 1); // 거울 모드
            recognizer.processFrame(frame);
            msleep(5); // CPU 부하 조절
        }

        cap.release();
        qDebug() << "[GestureWorker] 제스처 감지 스레드 종료";
    }

private:
    int           _qtUdpSock;
    sockaddr_in   _qtAddr;
    std::atomic<bool> _running;
};


// ============================================================
//  ArduinoWorker: 아두이노 TCP 서버를 별도 스레드에서 실행
//  아두이노 연결을 기다리며 데이터를 수신해 Qt 시그널로 전달
// ============================================================
class ArduinoWorker : public QThread
{
    Q_OBJECT

public:
    explicit ArduinoWorker(int port, int qtUdpSock, sockaddr_in qtAddr, QObject* parent = nullptr)
        : QThread(parent)
        , _port(port)
        , _qtUdpSock(qtUdpSock)
        , _qtAddr(qtAddr)
        , _running(false)
        , _serverFd(-1)
    {}

    void stop()
    {
        _running = false;
        if (_serverFd != -1) {
            ::close(_serverFd);
            _serverFd = -1;
        }
    }

signals:
    void arduinoDataReceived(const QString& data);
    void arduinoConnected(const QString& ip);
    void arduinoDisconnected();

protected:
    void run() override
    {
        qDebug() << "[ArduinoWorker] 아두이노 TCP 서버 스레드 시작";

        _serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (_serverFd < 0) {
            qWarning() << "[ArduinoWorker] 소켓 생성 실패";
            return;
        }

        int opt = 1;
        setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(_port);

        if (bind(_serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            qWarning() << "[ArduinoWorker] bind 실패";
            ::close(_serverFd);
            return;
        }

        listen(_serverFd, 5);
        qDebug() << "[ArduinoWorker] 아두이노 대기 중 - 포트:" << _port;

        _running = true;
        const int BUFFER_SIZE = 1024;

        while (_running) {
            sockaddr_in clientAddr{};
            socklen_t   clientLen = sizeof(clientAddr);

            int connFd = accept(_serverFd, (sockaddr*)&clientAddr, &clientLen);
            if (connFd < 0) {
                if (!_running) break;
                continue;
            }

            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
            emit arduinoConnected(QString(clientIp));
            qDebug() << "[ArduinoWorker] 아두이노 연결됨:" << clientIp;

            char buffer[BUFFER_SIZE];
            while (_running) {
                memset(buffer, 0, sizeof(buffer));
                ssize_t bytes = recv(connFd, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                    emit arduinoDisconnected();
                    break;
                }

                std::string received(buffer);
                QString data = QString::fromStdString(received).trimmed();

                // Qt 시그널로 메인스레드에 전달
                emit arduinoDataReceived(data);

                // Qt UDP 포트로도 전달
                if (_qtUdpSock != -1) {
                    sendto(_qtUdpSock, buffer, bytes, 0,
                           (sockaddr*)&_qtAddr, sizeof(_qtAddr));
                }
            }
            ::close(connFd);
        }

        if (_serverFd != -1) {
            ::close(_serverFd);
            _serverFd = -1;
        }
        qDebug() << "[ArduinoWorker] 아두이노 서버 스레드 종료";
    }

private:
    int           _port;
    int           _qtUdpSock;
    sockaddr_in   _qtAddr;
    std::atomic<bool> _running;
    int           _serverFd;
};


// ============================================================
//  MainWindow
// ============================================================
namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    // ── 아두이노 TCP 슬롯 (기존 유지) ────────────
    void onNewConnection();
    void onDataReceived();
    void onClientDisconnected();

    // ── 제스처 수신 슬롯 ──────────────────────────
    void gestureDetected(const QString& gesture);

    // ── 아두이노 워커 슬롯 ────────────────────────
    void onArduinoData(const QString& data);
    void onArduinoConnected(const QString& ip);
    void onArduinoDisconnected();

    // ── 카메라 오류 슬롯 ──────────────────────────
    void onCameraError(const QString& message);

private slots:
    void updateTime();

private:
    void processData(const QString& data);
    void applyBrightness(int briVal);
    void initUdpSocket();       // UDP 소켓 초기화
    void startWorkerThreads();  // 워커 스레드 시작

    Ui::MainWindow* ui;

    // ── 기존 Qt TCP (Qt 자체 TCP 서버, 필요 시 유지) ──
    QTcpServer* tcpServer  = nullptr;
    QTcpSocket* tcpSocket  = nullptr;

    // ── 타이머 ────────────────────────────────────
    QTimer* timer = nullptr;

    // ── 오버레이 위젯 ─────────────────────────────
    QWidget* overlayWidget = nullptr;
    QFrame*  blackOverlay  = nullptr;

    // ── 미디어 프로세스 ───────────────────────────
    QProcess* ytDlpProcess = nullptr;
    QProcess* mpvProcess   = nullptr;

    // ── 멀티스레드 워커 ───────────────────────────
    GestureWorker*  gestureWorker  = nullptr;  // OpenCV 제스처 스레드
    ArduinoWorker*  arduinoWorker  = nullptr;  // 아두이노 TCP 스레드

    // ── UDP 소켓 (Qt → 제스처/아두이노 데이터 전달용) ──
    int         _qtUdpSock = -1;
    sockaddr_in _qtAddr{};

    // ── 상태 플래그 ───────────────────────────────
    bool waitingData = false;

    // ── 포트 설정 ─────────────────────────────────
    static constexpr int ARDUINO_TCP_PORT = 9000;
    static constexpr int QT_UDP_PORT      = 9001;

protected:
    void resizeEvent(QResizeEvent* event) override;
};
