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
#include <QDateTime>

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
        , _lastGesture(GestureType::NONE)
    {
        _lastGestureTime = QDateTime::currentMSecsSinceEpoch();
    }

    void stop() { _running = false; }

signals:
    void gestureDetected(const QString& gesture);
    void cameraError(const QString& message);

protected:
    void run() override
    {
        qDebug() << "[GestureWorker] 제스처 감지 스레드 시작";

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

        // 제스처 콜백 내부에서 역방향 모션 오인식 차단 필터링 수행
        recognizer.setGestureCallback([this](GestureType g, float /*confidence*/) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            qint64 timeDiff = currentTime - _lastGestureTime;
            
            // ── [오인식 해결 핵심] 역방향 불응기 (1.2초 동안 반대 동작 차단) ──
            qint64 directionalCooldown = 1200; 
            if (timeDiff < directionalCooldown) {
                if (_lastGesture == GestureType::SWIPE_LEFT && g == GestureType::SWIPE_RIGHT) {
                    qDebug() << "[필터링] SWIPE_LEFT 직후 복귀 무시 (SWIPE_RIGHT 차단)";
                    return;
                }
                if (_lastGesture == GestureType::SWIPE_RIGHT && g == GestureType::SWIPE_LEFT) {
                    qDebug() << "[필터링] SWIPE_RIGHT 직후 복귀 무시 (SWIPE_LEFT 차단)";
                    return;
                }
                if (_lastGesture == GestureType::SWIPE_UP && g == GestureType::SWIPE_DOWN) {
                    qDebug() << "[필터링] SWIPE_UP 직후 복귀 무시 (SWIPE_DOWN 차단)";
                    return;
                }
                if (_lastGesture == GestureType::SWIPE_DOWN && g == GestureType::SWIPE_UP) {
                    qDebug() << "[필터링] SWIPE_DOWN 직후 복귀 무시 (SWIPE_UP 차단)";
                    return;
                }
            }

            // 새로운 정상 제스처 상태 업데이트
            _lastGesture = g;
            _lastGestureTime = currentTime;

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

            if (_qtUdpSock != -1) {
                QByteArray msg = gesture_msg.toUtf8();
                sendto(_qtUdpSock, msg.constData(), msg.size(), 0,
                       (sockaddr*)&_qtAddr, sizeof(_qtAddr));
            }

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
            
            // ── [오인식 해결 핵심] 과도한 프레임 샘플링 제한 (30ms = 약 33 FPS) ──
            // 너무 초고속으로 좌표 변화를 추적하면 손을 돌려놓는 잔상까지 전부 인식되므로 주기를 늦춥니다.
            msleep(30); 
        }

        cap.release();
        qDebug() << "[GestureWorker] 제스처 감지 스레드 종료";
    }

private:
    int         _qtUdpSock;
    sockaddr_in _qtAddr;
    std::atomic<bool> _running;

    // 마지막 제스처 정보 저장용 변수
    GestureType _lastGesture;
    qint64      _lastGestureTime;
};


// ============================================================
//  ArduinoWorker: 아두이노 TCP 서버를 별도 스레드에서 실행
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

                emit arduinoDataReceived(data);

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
//  MainWindow 클래스 선언부
// ============================================================
namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void onNewConnection();
    void onDataReceived();
    void onClientDisconnected();

    void gestureDetected(const QString& gesture);

    void onArduinoData(const QString& data);
    void onArduinoConnected(const QString& ip);
    void onArduinoDisconnected();

    void onCameraError(const QString& message);

private slots:
    void updateTime();

private:
    void processData(const QString& data);
    void applyBrightness(int briVal);
    void initUdpSocket();       
    void startWorkerThreads();  

    Ui::MainWindow* ui;

    QTcpServer* tcpServer  = nullptr;
    QTcpSocket* tcpSocket  = nullptr;

    QTimer* timer = nullptr;

    QWidget* overlayWidget = nullptr;
    QFrame* blackOverlay  = nullptr;

    QProcess* ytDlpProcess = nullptr;
    QProcess* mpvProcess   = nullptr;

    GestureWorker* gestureWorker  = nullptr;  
    ArduinoWorker* arduinoWorker  = nullptr;  

    int         _qtUdpSock = -1;
    sockaddr_in _qtAddr{};

    bool waitingData = false;

    static constexpr int ARDUINO_TCP_PORT = 9000;
    static constexpr int QT_UDP_PORT      = 9001;

protected:
    void resizeEvent(QResizeEvent* event) override;
};
