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

    // -- TCP 9000 워커 및 스레드 ------------------
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

    // -- TCP 9001 제스처 워커 및 스레드 -----------
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

    // -- 감정 처리 워커 및 스레드 ------------
    m_emotionThread = new QThread(this);
    m_emotionWorker = new EmotionProcessWorker();
    m_emotionWorker->moveToThread(m_emotionThread);

    connect(m_emotionThread, &QThread::started,
            m_emotionWorker, &EmotionProcessWorker::initialize);
    connect(m_emotionThread, &QThread::finished,
            m_emotionWorker, &QObject::deleteLater);

    m_emotionThread->start();

    // -- 음악 재생 워커 및 스레드 ---------------
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

    // -- 시계 타이머 --------------------------------
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();

    // -- 전역 스일 (대리석 느낌 어두운 배경) --------
    this->setStyleSheet(
        "QMainWindow {"
        "background-color:#0d0d0d;"
        "}"
    );

    // -- 날씨 패널 -----------------------------
    WeatherWidget = new WeatherPanel(ui->centralWidget);
    WeatherWidget->setGeometry(960, 500, 900, 400);
    WeatherWidget->hide();
    QTimer::singleShot(
        1000,
        this,
        [=]()
        {
            WeatherWidget->move(960, 500);
            WeatherWidget->show();
        });

    // -- 뮤직바 ---------------------------------
    musicBar = new MusicBar(ui->centralWidget);
    musicBar->setGeometry(960, 260, 900, 200);
    musicBar->show();

    // -- 뉴스 패널 --------------------------------
    newsWidget = new NewsPanel(ui->centralWidget);
    newsWidget->setGeometry(960, 1200, 900, 560);
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

// -- 아두이노 데이터 수신 (메인 스레드) ----------
void MainWindow::onArduinoDataReceived(const QString &data)
{
    processData(data);
}

// -- 제스처 데이터 수신 (메인 스레드) ----------
void MainWindow::onGestureDataReceived(const QString &gesture)
{
    qDebug() << "[제스처 확인] 진입 명령:" << gesture;

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
            qDebug() << "[START - 재개] 일시정지 해제: 멈춘 지점부터 재생.";
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
            qDebug() << "[STOP] 제스처 감지: 음악 일시정지 명령";
            QMetaObject::invokeMethod(m_musicWorker, "pause", Qt::QueuedConnection);
            isPaused = true;
            musicBar->setPlaying(false);
        }
    }
    else if (gesture == "END")
    {
        if (m_musicWorker)
        {
            qDebug() << "[END] 제스처 감지: 모든 오디오 재생 완전 종료.";
            QMetaObject::invokeMethod(m_musicWorker, "stop", Qt::QueuedConnection);
            isPaused = false;
            musicBar->resetPlay();
        }
    }
    else if (gesture == "LEFT")
    {
        qDebug() << "[LEFT] 뉴스 패널";
        showNewsPanel();
    }
    else if (gesture == "RIGHT")
    {
        qDebug() << "[RIGHT] 날씨 패널";
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
    else if (gesture.startsWith("USER:"))
    {
        QString pos = gesture.section(':', 1, 1).trimmed();
        qDebug() << "[USER 위치] 사용자 위치:" << pos;
        relocateUI(pos);
    }
}

// -- 음악 워커 콜백 -----------------------
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

// -- 음악 검색 시작 ---------------------------
void MainWindow::startMusicSearch()
{
    qDebug() << "[START - 신규] 첫 재생 감지: mpv 스트리밍 시작.";

//    if (keyword.isEmpty())
//        keyword = "calm";

    QMetaObject::invokeMethod(m_musicWorker, "searchAndPlay", Qt::QueuedConnection,
                              Q_ARG(QString, keyword));

    isPaused = false;
}

// -- 밝기 오버레이 적용 ---------------------
void MainWindow::applyBrightness(int briVal)
{
    int brightness = qBound(80, (briVal * 255) / 550, 255);

    QString color = QString("rgb(%1,%1,%1)").arg(brightness);

    ui->timeLabel->setStyleSheet(
        QString("color:%1; font-size:64pt; font-weight:300; letter-spacing:2px; background:none;").arg(color));

    ui->dateLabel->setStyleSheet(
        QString("color:%1; font-size:13pt; font-weight:400; letter-spacing:1px; background:none;").arg(QString("rgb(%1,%1,%1)").arg(qMax(100, brightness - 60))));

    ui->tempValue->setStyleSheet(
        QString("color:%1; font-size:24pt; font-weight:300; background:none;").arg(color));

    ui->humiValue->setStyleSheet(
        QString("color:%1; font-size:24pt; font-weight:300; background:none;").arg(color));

    ui->aqiValue->setStyleSheet(
        QString("color:%1; font-size:24pt; font-weight:300; background:none;").arg(color));

    WeatherWidget->setTextBrightness(brightness);
    newsWidget->setTextBrightness(brightness);
}

// -- 데이터 파싱 및 UI 갱신 (메인 스레드) -----
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

        ui->tempValue->setText(QString::number(tempVal, 'f', 1));
        ui->humiValue->setText(QString::number(humiVal, 'f', 1));

        QString aqiText;
        switch (aqiVal)
        {
        case 1: aqiText = "좋음"; break;
        case 2: aqiText = "보통"; break;
        case 3: aqiText = "나쁨"; break;
        case 4: aqiText = "매우나쁨"; break;
        case 5: aqiText = "위험"; break;
        default: aqiText = "--"; break;
        }
        ui->aqiValue->setText(aqiText);

        if (data.contains("BRI:"))
        {
            int briVal = briStr.toInt();
            applyBrightness(briVal);
        }

        if (waitingData)
        {
            QProcess::execute("xset", QStringList() << "dpms" << "force" << "on");
            waitingData = false;
        }
    }
}

// -- 시계 갱신 ---------------------------------
void MainWindow::updateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QLocale koLocale(QLocale::Korean, QLocale::SouthKorea);

    ui->dateLabel->setText(koLocale.toString(current, "M월 d일 dddd"));
    ui->timeLabel->setText(current.toString("h:mm"));
}

// -- 창 크기 조정 --------------------------------
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}

// -- 날씨 패널 표시 ---------------------------
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

    // 뉴스 패널: 현재 위치에서 화면 아래로 이동
    QPoint newsCurrentPos = newsWidget->pos();
    QPoint newsTargetPos(960 + currentOffsetX, 1200);

    // 날씨 패널: 화면 아래에서 제자리로 이동
    QPoint weatherCurrentPos(960 + currentOffsetX, 1200);
    QPoint weatherTargetPos(960 + currentOffsetX, 500);

    // 시작 전에 날씨 패널 위치를 시작 위치로 설정
    WeatherWidget->move(weatherCurrentPos);

    QPropertyAnimation *newsAnim = new QPropertyAnimation(newsWidget, "pos");
    newsAnim->setDuration(700);
    newsAnim->setStartValue(newsCurrentPos);
    newsAnim->setEndValue(newsTargetPos);

    QPropertyAnimation *weatherAnim = new QPropertyAnimation(WeatherWidget, "pos");
    weatherAnim->setDuration(700);
    weatherAnim->setStartValue(weatherCurrentPos);
    weatherAnim->setEndValue(weatherTargetPos);

    newsAnim->start(QAbstractAnimation::DeleteWhenStopped);
    weatherAnim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(newsAnim, &QPropertyAnimation::finished, newsWidget, &QWidget::hide);
    connect(weatherAnim, &QPropertyAnimation::finished, this, [this]() {
        animationRunning = false;
    });
}

// -- 뉴스 패널 표시 ------------------------------
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

    // 날씨 패널: 현재 위치에서 화면 아래로 이동
    QPoint weatherCurrentPos = WeatherWidget->pos();
    QPoint weatherTargetPos(960 + currentOffsetX, 1200);

    // 뉴스 패널: 화면 아래에서 제자리로 이동
    QPoint newsCurrentPos(960 + currentOffsetX, 1200);
    QPoint newsTargetPos(960 + currentOffsetX, 500);

    // 시작 전에 뉴스 패널 위치를 시작 위치로 설정
    newsWidget->move(newsCurrentPos);

    QPropertyAnimation *weatherAnim = new QPropertyAnimation(WeatherWidget, "pos");
    weatherAnim->setDuration(700);
    weatherAnim->setStartValue(weatherCurrentPos);
    weatherAnim->setEndValue(weatherTargetPos);

    QPropertyAnimation *newsAnim = new QPropertyAnimation(newsWidget, "pos");
    newsAnim->setDuration(700);
    newsAnim->setStartValue(newsCurrentPos);
    newsAnim->setEndValue(newsTargetPos);

    weatherAnim->start(QAbstractAnimation::DeleteWhenStopped);
    newsAnim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(weatherAnim, &QPropertyAnimation::finished, WeatherWidget, &QWidget::hide);
    connect(newsAnim, &QPropertyAnimation::finished, this, [this]() {
        animationRunning = false;
    });
}

// -- 사용자 위치에 따른 UI 재배치 ------------------------------
void MainWindow::relocateUI(const QString &userPos)
{
    int offsetX = 0;

    if (userPos == "LEFT")
    {
        offsetX = 0;
    }
    else if (userPos == "RIGHT")
    {
        offsetX = -900;
    }
    else
    {
        return;
    }

    // 헤더 위젯들 이동
    ui->timeLabel->move(960 + offsetX, 40);
    ui->dateLabel->move(965 + offsetX, 132);
    ui->separator1->move(1230 + offsetX, 48);
    ui->tempIcon->move(1250 + offsetX, 52);
    ui->tempValue->move(1300 + offsetX, 52);
    ui->tempUnit->move(1380 + offsetX, 60);
    ui->tempLabel->move(1300 + offsetX, 92);
    ui->separator2->move(1420 + offsetX, 48);
    ui->humiIcon->move(1440 + offsetX, 52);
    ui->humiValue->move(1490 + offsetX, 52);
    ui->humiUnit->move(1560 + offsetX, 60);
    ui->humiLabel->move(1490 + offsetX, 92);
    ui->separator3->move(1610 + offsetX, 48);
    ui->aqiIcon->move(1630 + offsetX, 52);
    ui->aqiValue->move(1680 + offsetX, 52);
    ui->aqiLabel->move(1680 + offsetX, 92);
    ui->headerSeparator->move(960 + offsetX, 175);

    // 음악 바 이동
    musicBar->move(960 + offsetX, 260);

    // 뉴스/날씨 패널도 함께 이동
    newsWidget->move(960 + offsetX, 500);
    WeatherWidget->move(960 + offsetX, 500);

    qDebug() << "[UI 재배치] 사용자 위치:" << userPos << ", X 오프셋:" << offsetX;
    currentOffsetX = offsetX;
}
