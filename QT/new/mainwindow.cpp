#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTimer>
#include <QDateTime>
#include <QLocale>
#include <QDebug>
#include <QFrame>
#include <QResizeEvent>
#include <QGraphicsOpacityEffect>
#include <QProcessEnvironment>
#include <QProcess>
#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->resize(1920, 1080);

    isPaused = false;
    waitingData = true;

    // -- TCP 9000 Worker & Thread ------------------
    m_tcpThread = new QThread(this);
    m_tcpWorker = new TcpSocketWorker();
    m_tcpWorker->moveToThread(m_tcpThread);

    connect(m_tcpThread, &QThread::started,
            m_tcpWorker, &TcpSocketWorker::initialize);
    connect(m_tcpWorker, &TcpSocketWorker::dataReceived,
            this, &MainWindow::onArduinoDataReceived);
    connect(m_tcpThread, &QThread::finished,
            m_tcpWorker, &QObject::deleteLater);

    m_tcpThread->start();

    // -- TCP 9001 Gesture Worker & Thread -----------
    m_gestureThread = new QThread(this);
    m_gestureWorker = new GestureSocketWorker();
    m_gestureWorker->moveToThread(m_gestureThread);

    connect(m_gestureThread, &QThread::started,
            m_gestureWorker, &GestureSocketWorker::initialize);
    connect(m_gestureWorker, &GestureSocketWorker::gestureReceived,
            this, &MainWindow::onGestureDataReceived);
    connect(m_gestureThread, &QThread::finished,
            m_gestureWorker, &QObject::deleteLater);

    m_gestureThread->start();

    // -- Emotion Process Worker & Thread ------------
    m_emotionThread = new QThread(this);
    m_emotionWorker = new EmotionProcessWorker();
    m_emotionWorker->moveToThread(m_emotionThread);

    connect(m_emotionThread, &QThread::started,
            m_emotionWorker, &EmotionProcessWorker::initialize);
    connect(m_emotionThread, &QThread::finished,
            m_emotionWorker, &QObject::deleteLater);

    m_emotionThread->start();

    // -- Music Player Worker & Thread ---------------
    m_musicThread = new QThread(this);
    m_musicWorker = new MusicPlayerWorker();
    m_musicWorker->moveToThread(m_musicThread);

    connect(m_musicThread, &QThread::started,
            m_musicWorker, &MusicPlayerWorker::initialize);
    connect(m_musicWorker, &MusicPlayerWorker::trackInfoReady,
            this, &MainWindow::onTrackInfoReady);
    connect(m_musicWorker, &MusicPlayerWorker::playbackStarted,
            this, &MainWindow::onPlaybackStarted);
    connect(m_musicWorker, &MusicPlayerWorker::playbackPaused,
            this, &MainWindow::onPlaybackPaused);
    connect(m_musicWorker, &MusicPlayerWorker::playbackStopped,
            this, &MainWindow::onPlaybackStopped);
    connect(m_musicWorker, &MusicPlayerWorker::errorOccurred,
            this, [](const QString &msg) {
                qWarning() << "[MusicWorker Error]:" << msg;
            });
    connect(m_musicThread, &QThread::finished,
            m_musicWorker, &QObject::deleteLater);

    m_musicThread->start();

    // -- Clock timer --------------------------------
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();

    // -- Global style -------------------------------
    this->setStyleSheet("background-color:black;");

    ui->dateLabel->setStyleSheet(
        "color:#cccccc;"
        "font-size:11pt;"
        "font-weight:400;"
        "letter-spacing:1px;"
        "background:none;");
    ui->timeLabel->setStyleSheet(
        "color:white;"
        "font-size:48pt;"
        "font-weight:bold;"
        "letter-spacing:-1px;"
        "background:none;");
    ui->labelAQI->setStyleSheet(
        "color:white;"
        "font-size:18pt;"
        "font-weight:500;"
        "background:none;");

    ui->lcdNumberTemp->setSegmentStyle(QLCDNumber::Filled);
    ui->lcdNumberHumi->setSegmentStyle(QLCDNumber::Filled);

    ui->lcdNumberTemp->setStyleSheet("background:#151515; border-radius:8px;");
    ui->lcdNumberHumi->setStyleSheet("background:#151515; border-radius:8px;");

    // -- Weather panel -----------------------------
    WeatherWidget = new WeatherPanel(ui->centralWidget);
    WeatherWidget->setGeometry(900, 1200, 1023, 500);
    WeatherWidget->hide();
    QTimer::singleShot(
        1000,
        this,
        [=]()
        {
            WeatherWidget->move(900, 550);
            WeatherWidget->show();
        });

    // -- MusicBar ---------------------------------
    musicBar = new MusicBar(ui->centralWidget);
    musicBar->setGeometry(960, 380, 920, 150);
    musicBar->show();

    // -- UI layout ----------------------------------
    ui->dateLabel->move(1500, 24);
    ui->timeLabel->move(1440, 48);
    ui->lcdNumberTemp->move(1320, 175);
    ui->label->move(1440, 175);
    ui->lcdNumberHumi->move(1320, 245);
    ui->label_2->move(1440, 245);
    ui->labelAQI->move(1320, 325);

    ui->dateLabel->resize(220, 32);
    ui->timeLabel->resize(380, 110);
    ui->lcdNumberTemp->resize(110, 46);
    ui->lcdNumberHumi->resize(110, 46);
    ui->label->resize(80, 46);
    ui->label_2->resize(100, 46);
    ui->labelAQI->resize(200, 36);

    // -- NewsPanel --------------------------------
    newsWidget = new NewsPanel(ui->centralWidget);
    newsWidget->setGeometry(1920, 520, 760, 620);
    newsWidget->hide();
}

MainWindow::~MainWindow()
{
    QMetaObject::invokeMethod(m_emotionWorker, "shutdown", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_musicWorker, "shutdown", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_tcpWorker, "shutdown", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_gestureWorker, "shutdown", Qt::BlockingQueuedConnection);

    m_emotionThread->quit();
    m_musicThread->quit();
    m_tcpThread->quit();
    m_gestureThread->quit();

    m_emotionThread->wait();
    m_musicThread->wait();
    m_tcpThread->wait();
    m_gestureThread->wait();

    delete ui;
}

// -- Arduino data received (main thread) ----------
void MainWindow::onArduinoDataReceived(const QString &data)
{
    processData(data);
}

// -- Gesture data received (main thread) ----------
void MainWindow::onGestureDataReceived(const QString &gesture)
{
    qDebug() << "[Gesture verified] Entry command:" << gesture;

    if (gesture.startsWith("KEYWORD"))
    {
        keyword = gesture.section(':', 1, 1).trimmed();
        qDebug() << "KEYWORD:" << keyword;
    }
    else if (gesture == "START")
    {
        if (!isPaused && m_musicWorker)
        {
            startMusicSearch();
            return;
        }

        if (isPaused && m_musicWorker)
        {
            qDebug() << "[START - Resume] Exiting pause: resuming from where it stopped.";
            QMetaObject::invokeMethod(m_musicWorker, "resume", Qt::QueuedConnection);
            isPaused = false;
            musicBar->setPlaying(true);
        }
        else
        {
            startMusicSearch();
        }
    }
    else if (gesture == "STOP")
    {
        if (m_musicWorker)
        {
            qDebug() << "[STOP] Gesture detected: music pause command";
            QMetaObject::invokeMethod(m_musicWorker, "pause", Qt::QueuedConnection);
            isPaused = true;
            musicBar->setPlaying(false);
        }
    }
    else if (gesture == "END")
    {
        if (m_musicWorker)
        {
            qDebug() << "[END] Gesture detected: fully terminate all audio playback processes.";
            QMetaObject::invokeMethod(m_musicWorker, "stop", Qt::QueuedConnection);
            isPaused = false;
            musicBar->setPlaying(false);
            musicBar->setTrackTitle("");
            musicBar->setCurrentSeconds(0);
            musicBar->onTimerTick();
        }
    }
    else if (gesture == "LEFT")
    {
        qDebug() << "[LEFT] News panel";
        showNewsPanel();
    }
    else if (gesture == "RIGHT")
    {
        qDebug() << "[RIGHT] Weather panel";
        showWeatherPanel();
    }
    else if (gesture == "VOLUME_UP")
    {
        QMetaObject::invokeMethod(m_musicWorker, "volumeUp", Qt::QueuedConnection);
    }
    else if (gesture == "VOLUME_DOWN")
    {
        QMetaObject::invokeMethod(m_musicWorker, "volumeDown", Qt::QueuedConnection);
    }
}

// -- Music Worker callbacks -----------------------
void MainWindow::onTrackInfoReady(const QString &title, int durationSeconds)
{
    musicBar->setTrackTitle(title);
    musicBar->setTotalSeconds(durationSeconds);
}

void MainWindow::onPlaybackStarted()
{
    musicBar->setPlaying(true);
}

void MainWindow::onPlaybackPaused()
{
    musicBar->setPlaying(false);
}

void MainWindow::onPlaybackStopped()
{
    musicBar->setPlaying(false);
}

// -- Start music search ---------------------------
void MainWindow::startMusicSearch()
{
    qDebug() << "[START - New] First playback detected: launching mpv streaming.";

    if (keyword.isEmpty())
        keyword = "calm";

    QMetaObject::invokeMethod(m_musicWorker, "searchAndPlay", Qt::QueuedConnection,
                              Q_ARG(QString, keyword));

    isPaused = false;
}

// -- Apply brightness overlay ---------------------
void MainWindow::applyBrightness(int briVal)
{
    int brightness = qBound(80, (briVal * 255) / 550, 255);

    QString color = QString("rgb(%1,%1,%1)").arg(brightness);

    ui->dateLabel->setStyleSheet(
        QString("color:%1; font-size:11pt; font-weight:400; letter-spacing:1px; background:none;").arg(color));

    ui->timeLabel->setStyleSheet(
        QString("color:%1; font-size:48pt; font-weight:bold; letter-spacing:-1px; background:none;").arg(color));

    ui->labelAQI->setStyleSheet(
        QString("color:%1; font-size:18pt; font-weight:500; background:none;").arg(color));

    QColor lcdColor(brightness, brightness, brightness);

    QPalette tempPalette = ui->lcdNumberTemp->palette();
    tempPalette.setColor(QPalette::Light, lcdColor);
    tempPalette.setColor(QPalette::Dark, lcdColor.darker());
    ui->lcdNumberTemp->setPalette(tempPalette);

    QPalette humiPalette = ui->lcdNumberHumi->palette();
    humiPalette.setColor(QPalette::Light, lcdColor);
    humiPalette.setColor(QPalette::Dark, lcdColor.darker());
    ui->lcdNumberHumi->setPalette(humiPalette);

    WeatherWidget->setTextBrightness(brightness);
    newsWidget->setTextBrightness(brightness);
}

// -- Data parsing and UI update (main thread) -----
void MainWindow::processData(const QString &data)
{
    if (data == "OFF")
    {
        QProcess::execute("xset", QStringList() << "dpms" << "force" << "off");
        waitingData = false;

        QMetaObject::invokeMethod(m_emotionWorker, "stopProcess", Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_musicWorker, "stop", Qt::QueuedConnection);
        isPaused = false;

        return;
    }

    if (data == "ON")
    {
        QMetaObject::invokeMethod(m_emotionWorker, "startProcess", Qt::QueuedConnection);
        waitingData = true;
        return;
    }

    if (data.startsWith("BRI:") && !data.contains("TEMP:"))
    {
        int briVal = data.section("BRI:", 1, 1).trimmed().toInt();
        applyBrightness(briVal);
        return;
    }

    if (data.startsWith("TEMP:"))
    {
        QString tempStr = data.section("TEMP:", 1, 1).section(",", 0, 0).trimmed();
        QString humiStr = data.section("HUMI:", 1, 1).section(",", 0, 0).trimmed();
        QString aqiStr = data.section("AQI:", 1, 1).section(",", 0, 0).trimmed();
        QString briStr = data.section("BRI:", 1, 1).section(",", 0, 0).trimmed();

        double tempVal = tempStr.toDouble();
        double humiVal = humiStr.toDouble();
        int aqiVal = aqiStr.toInt();
        int briVal = briStr.toInt();

        ui->lcdNumberTemp->display(QString::number(tempVal, 'f', 1));
        ui->lcdNumberHumi->display(QString::number(humiVal, 'f', 1));

        QString emoji;
        switch (aqiVal)
        {
        case 1: emoji = "Good"; break;
        case 2: emoji = "Moderate"; break;
        case 3: emoji = "Bad"; break;
        case 4: emoji = "Hazardous"; break;
        case 5: emoji = "Deadly"; break;
        default: emoji = "Error"; break;
        }
        ui->labelAQI->setText(emoji);

        if (data.contains("BRI:"))
        {
            applyBrightness(briVal);
        }

        if (waitingData)
        {
            QProcess::execute("xset", QStringList() << "dpms" << "force" << "on");
            waitingData = false;
        }
    }
}

// -- Clock update ---------------------------------
void MainWindow::updateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QLocale koLocale(QLocale::Korean, QLocale::SouthKorea);

    ui->dateLabel->setText(koLocale.toString(current, "MMM d dddd"));
    ui->timeLabel->setText(current.toString("h:mm"));
}

// -- Window resize --------------------------------
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}

// -- Show Weather Panel ---------------------------
void MainWindow::showWeatherPanel()
{
    if (animationRunning)
        return;

    if (isWeatherVisible)
        return;

    animationRunning = true;

    isWeatherVisible = true;
    isNewsVisible = false;

    WeatherWidget->show();

    QPropertyAnimation *newsAnim = new QPropertyAnimation(newsWidget, "pos");
    newsAnim->setDuration(700);
    newsAnim->setStartValue(newsWidget->pos());
    newsAnim->setEndValue(QPoint(1920, 520));

    QPropertyAnimation *weatherAnim = new QPropertyAnimation(WeatherWidget, "pos");
    weatherAnim->setDuration(700);
    weatherAnim->setStartValue(QPoint(900, 1200));
    weatherAnim->setEndValue(QPoint(900, 550));

    newsAnim->start(QAbstractAnimation::DeleteWhenStopped);
    weatherAnim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(newsAnim, &QPropertyAnimation::finished, newsWidget, &QWidget::hide);
    connect(weatherAnim, &QPropertyAnimation::finished, this, [this]() {
        animationRunning = false;
    });
}

// -- Show News Panel ------------------------------
void MainWindow::showNewsPanel()
{
    if (animationRunning)
        return;

    if (isNewsVisible)
        return;

    animationRunning = true;

    isNewsVisible = true;
    isWeatherVisible = false;

    newsWidget->show();

    QPropertyAnimation *weatherAnim = new QPropertyAnimation(WeatherWidget, "pos");
    weatherAnim->setDuration(700);
    weatherAnim->setStartValue(QPoint(900, 550));
    weatherAnim->setEndValue(QPoint(900, 1200));

    QPropertyAnimation *newsAnim = new QPropertyAnimation(newsWidget, "pos");
    newsAnim->setDuration(700);
    newsAnim->setStartValue(QPoint(1920, 520));
    newsAnim->setEndValue(QPoint(1180, 520));

    weatherAnim->start(QAbstractAnimation::DeleteWhenStopped);
    newsAnim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(weatherAnim, &QPropertyAnimation::finished, WeatherWidget, &QWidget::hide);
    connect(newsAnim, &QPropertyAnimation::finished, this, [this]() {
        animationRunning = false;
    });
}
