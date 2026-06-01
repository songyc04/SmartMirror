#pragma once

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>
#include <QFrame>
#include <QProcess>
#include <QThread>
#include <QDateTime>
#include <QDebug>

// ── OpenCV 관련 헤더 ──────────────────────────
#include <opencv2/opencv.hpp>
#include "gesture_recognition.hpp"

// ── 리눅스 시스템 및 소켓 헤더 ──────────────────
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ============================================================
//  GestureWorker: OpenCV 제스처 감지를 백그라운드에서 실행하는 스레드
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

protected:
    void run() override
    {
        qDebug() << "[GestureWorker] 제스처 감지 스레드 시작";

        // 젯슨 보드의 기본 카메라(CSI 또는 USB 웹캠) 열기
        cv::VideoCapture cap(0, cv::CAP_V4L2);
        if (!cap.isOpened()) {
            cap.open(0);
            if (!cap.isOpened()) {
                qWarning() << "[GestureWorker] 카메러를 열 수 없습니다.";
                return;
            }
        }

        // 해상도 및 FPS 최적화 (젯슨 보드 부하 감소)
        cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FPS, 30);

        GestureRecognizer recognizer;
        recognizer.setSensitivity(1.0f);
        recognizer.setCooldown(800);

        // 제스처 감지 시 실행될 콜백 함수 설정
        recognizer.setGestureCallback([this](GestureType g, float /*confidence*/) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            qint64 timeDiff = currentTime - _lastGestureTime;
            
            // ── [오인식 방지] 역방향 불응기 1.2초(1200ms) 적용 ──
            qint64 directionalCooldown = 1200; 
            if (timeDiff < directionalCooldown) {
                if (_lastGesture == GestureType::SWIPE_LEFT && g == GestureType::SWIPE_RIGHT) return;
                if (_lastGesture == GestureType::SWIPE_RIGHT && g == GestureType::SWIPE_LEFT) return;
                if (_lastGesture == GestureType::SWIPE_UP && g == GestureType::SWIPE_DOWN) return;
                if (_lastGesture == GestureType::SWIPE_DOWN && g == GestureType::SWIPE_UP) return;
            }

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

            // UDP 내부 소켓 전송
            if (_qtUdpSock != -1) {
                QByteArray msg = gesture_msg.toUtf8();
                sendto(_qtUdpSock, msg.constData(), msg.size(), 0,
                       (sockaddr*)&_qtAddr, sizeof(_qtAddr));
            }

            // 메인 UI 스레드로 시그널 발송
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

            cv::flip(frame, frame, 1); // 사용자가 보기 편하게 좌우 반전(거울 모드)
            recognizer.processFrame(frame);
            
            // 과도한 프레임 연산 제한으로 복귀 잔상 오인식 원천 차단 (30ms 주기)
            msleep(30); 
        }

        cap.release();
        qDebug() << "[GestureWorker] 제스처 감지 스레드 종료";
    }

private:
    int         _qtUdpSock;
    sockaddr_in _qtAddr;
    std::atomic<bool> _running;

    GestureType _lastGesture;
    qint64      _lastGestureTime;
};


// ============================================================
//  MainWindow 클래스 정의
// ============================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 아두이노 TCP 서버 관련 슬롯
    void onNewConnection();
    void onDataReceived();
    void onClientDisconnected();
    
    // 시계 업데이트 슬롯
    void updateTime();

    // OpenCV 스레드로부터 제스처 신호를 받는 슬롯
    void gestureDetected(const QString &gesture);

private:
    // 초기화 및 비즈니스 로직 함수
    void processData(const QString &data);
    void applyBrightness(int briVal);
    void initUdpSocket();       
    void startGestureWorker();  

    Ui::MainWindow *ui;

    // 아두이野 TCP 서버 네트워크 변수
    QTcpServer *tcpServer;
    QTcpSocket *tcpSocket;

    // 타이머 및 화면 오버레이 변수
    QTimer  *timer;
    QWidget *overlayWidget;
    QFrame  *blackOverlay;

    // 외부 프로세스 실행용 (유튜브 오디오 재생용)
    QProcess *ytDlpProcess;
    QProcess *mpvProcess;

    // ── OpenCV 제스처 연동을 위해 추가된 핵심 변수들 ──
    GestureWorker *gestureWorker;  // 백그라운드 스레드 객체 포인터
    int            _qtUdpSock;
