#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFrame>
#include <QTcpServer>
#include <QTcpSocket>
#include <QProcess>
#include <QPropertyAnimation>
#include "weatherpanel.h"

// UDP 소켓 전방 선언 추가
class QUdpSocket;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateTime();
    void onNewConnection();
    void onDataReceived();
    void onClientDisconnected();
    void gestureDetected(const QString& gesture);
    void onUdpDataReceived(); // ⭐ UDP 데이터 수신을 위한 슬롯 추가

private:
    Ui::MainWindow *ui;
    QTimer         *timer;
    QFrame         *blackOverlay;
    QWidget        *overlayWidget;
    WeatherPanel   *WeatherWidget;

    QTcpServer     *tcpServer;
    QTcpSocket     *tcpSocket;   // 현재 연결된 아두이노 소켓
    QUdpSocket     *udpSocket;   // ⭐ UDP 제스처 수신용 소켓 추가

    QProcess       *ytDlpProcess;
    QProcess       *mpvProcess;

    void processData(const QString &data);
    void applyBrightness(int briVal);

    void showWeatherPanel();
    void hideWeatherPanel();

    bool waitingData = false;
};

#endif // MAINWINDOW_H
