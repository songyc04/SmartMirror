#pragma once

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>
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
//  GestureWorker: OpenCV 제스처 감지 스레드 (기존과 동일)
// ============================================================
class GestureWorker : public QThread
{
    Q_OBJECT
public:
    explicit GestureWorker(int qtUdpSock, sockaddr_in qtAddr, QObject* parent = nullptr)
        : QThread(parent), _qtUdpSock(qtUdpSock), _qtAddr(qtAddr), _running(false), _lastGesture(GestureType::NONE)
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
        cv::VideoCapture cap(0, cv::CAP_V4L2);
        if (!cap.isOpened()) {
            cap.open(0);
            if (!cap.isOpened()) {
                qWarning() << "[GestureWorker] 카메라를 열 수 없습니다.";
                return;
            }
        }
        cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FPS, 30);

        GestureRecognizer recognizer;
        recognizer.setSensitivity(1.0f);
        recognizer.setCooldown(800);

        recognizer.setGestureCallback([this](GestureType g, float) {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            if (currentTime - _lastGestureTime < 1200) { // 역방향 불응기 1.2초
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
            if (_qtUdpSock != -1) {
                QByteArray msg = gesture_msg.toUtf8();
                sendto(_qtUdpSock, msg.constData(), msg.size(), 0, (sockaddr*)&_qtAddr, sizeof(_qtAddr));
            }
            emit gestureDetected(gesture_msg);
        });

        _running = true;
        cv::Mat frame;
        while (_running) {
            cap >> frame;
            if (frame.empty()) { msleep(10); continue; }
            cv::flip(frame, frame, 1);
            recognizer.processFrame(frame);
            msleep(30);
        }
        cap.release();
    }
private:
    int _qtUdpSock; sockaddr_in _qtAddr; std::atomic<bool> _running;
    GestureType _lastGesture; qint64 _lastGestureTime;
};

// ============================================================
//  MainWindow 클래스 정의 (UI 요소 대폭 제거)
// ============================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNewConnection();
    void onDataReceived();
    void onClientDisconnected();
    void gestureDetected(const QString &gesture);

private:
    void processData(const QString &data);
    void initUdpSocket();       
    void startGestureWorker();  

    Ui::MainWindow *ui;

    QTcpServer *tcpServer;
    QTcpSocket *tcpSocket;

    QProcess *ytDlpProcess;
    QProcess *mpvProcess;

    GestureWorker *gestureWorker;
    int            _qtUdpSock;
    sockaddr_in    _qtAddr;
};
