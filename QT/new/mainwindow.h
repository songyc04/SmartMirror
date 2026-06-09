#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QFrame>
#include <QThread>
#include <QPropertyAnimation>
#include "weatherpanel.h"
#include "newspanel.h"
#include "musicbar.h"
#include "tcpsocketworker.h"
#include "gesturesocketworker.h"
#include "musicplayerworker.h"
#include "emotionprocessworker.h"

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
    void onArduinoDataReceived(const QString &data);
    void onGestureDataReceived(const QString &gesture);
    void onTrackInfoReady(const QString &title, int durationSeconds);
    void onPlaybackStarted();
    void onPlaybackPaused();
    void onPlaybackStopped();

private:
    Ui::MainWindow *ui;
    QTimer         *timer;
    QFrame         *blackOverlay;
    WeatherPanel   *WeatherWidget;
    NewsPanel      *newsWidget;
    MusicBar       *musicBar;

    QThread           *m_tcpThread;
    TcpSocketWorker   *m_tcpWorker;

    QThread           *m_gestureThread;
    GestureSocketWorker *m_gestureWorker;

    QThread           *m_musicThread;
    MusicPlayerWorker *m_musicWorker;

    QThread           *m_emotionThread;
    EmotionProcessWorker *m_emotionWorker;

    void processData(const QString &data);
    void applyBrightness(int briVal);
    void showWeatherPanel();
    void showNewsPanel();
    void startMusicSearch();

    bool waitingData = false;
    bool isPaused = false;

    bool isNewsVisible = false;
    bool isWeatherVisible = true;
    bool animationRunning = false;
    QString keyword;
    int searchCount = 5;
};

#endif // MAINWINDOW_H
