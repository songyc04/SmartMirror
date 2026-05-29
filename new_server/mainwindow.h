#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFrame>
#include <QTcpServer>
#include <QTcpSocket>

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

private:
    Ui::MainWindow *ui;
    QTimer         *timer;
    QFrame         *blackOverlay;
    QWidget        *overlayWidget;

    QTcpServer     *tcpServer;
    QTcpSocket     *tcpSocket;   // 현재 연결된 아두이노 소켓

    void processData(const QString &data);
    void applyBrightness(int briVal);
};

#endif // MAINWINDOW_H
