#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QResizeEvent>
#include <QWidget>

class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    void resizeEvent(QResizeEvent *event) override;
private :
    void updateTime();
    void readPendingDatagrams();

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QUdpSocket *udpSocket;
    QFrame *blackOverlay;
    QGraphicsOpacityEffect *overlayEffect;
    QWidget *overlayWidget;
};

#endif
