#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QLocale>
#include <QDebug>
#include <QFrame>
#include <QResizeEvent>
#include <QGraphicsOpacityEffect>
#include <QProcess>
#include <QPropertyAnimation>
#include <QPalette>
#include <QColor>
#include <QProcessEnvironment>
//#include <QKeyEvent>    //테스트용

const quint16 ARDUINO_PORT = 9000;
const quint16 GESTURE_PORT = 9001;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), tcpSocket(nullptr), tcpSocket9001(nullptr), emotionProcess(nullptr)
{
    ui->setupUi(this);

    this->resize(1920, 1080);

    // 재생 상태 관리 플래그 초기화
    isPaused = false;

    // ── TCP 서버 시작 ──────────────────────────
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection,
            this, &MainWindow::onNewConnection);

    if (!tcpServer->listen(QHostAddress::Any, ARDUINO_PORT))
    {
        qWarning() << "TCP 서버 시작 실패:" << tcpServer->errorString();
    }
    else
    {
        qDebug() << "아두이노 대기 중 - 포트:" << ARDUINO_PORT;
    }

    emotionProcess = new QProcess(this);
    // ── TCP 9001 서버 시작 ──────
    tcpServer9001 = new QTcpServer(this);
    connect(tcpServer9001, &QTcpServer::newConnection,
            this, &MainWindow::onNewGestureConnection);

    if (!tcpServer9001->listen(QHostAddress::Any, GESTURE_PORT))
    {
        qWarning() << "9001 TCP 서버 시작 실패:" << tcpServer9001->errorString();
    }
    else
    {
        qDebug() << "9001 서버 대기 중 - 포트:" << GESTURE_PORT;
    }

    // 파이썬 표준 출력(print) 가로채기
    connect(emotionProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray output = emotionProcess->readAllStandardOutput();
        qDebug() << "[Python Out]:" << QString::fromUtf8(output).trimmed();
    });
    // 파이썬 표준 에러 출력 가로채기
    connect(emotionProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray errorOutput = emotionProcess->readAllStandardError();
        if (!errorOutput.isEmpty()) {
            qWarning() << "[Python Error]:" << QString::fromUtf8(errorOutput).trimmed();
        }
    });

    // ── 시계 타이머 ────────────────────────────
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();

    // ── 전체 스타일 ────────────────────────────
    this->setStyleSheet("background-color:black;");

    ui->dateLabel->setStyleSheet(
        "color:white;"
        "font-size:12pt;"
        "font-weight:400;"
        "background:none;");
    ui->timeLabel->setStyleSheet(
        "color:white;"
        "font-size:45pt;"
        "font-weight:bold;"
        "margin-top:-5px;"
        "background:none;");
    ui->labelAQI->setStyleSheet(
        "color:white;"
        "font-size:20pt;"
        "background:none;");

    ui->lcdNumberTemp->setSegmentStyle(QLCDNumber::Filled);
    ui->lcdNumberHumi->setSegmentStyle(QLCDNumber::Filled);

    ui->lcdNumberTemp->setStyleSheet("background:#111;");
    ui->lcdNumberHumi->setStyleSheet("background:#111;");

    // ── 화면 ON/OFF 오버레이 (맨 위) ──────────
    blackOverlay = new QFrame(ui->centralWidget);
    blackOverlay->setGeometry(ui->centralWidget->rect());
    blackOverlay->setStyleSheet("background-color:black;");
    blackOverlay->raise();
    blackOverlay->show(); // 시작할 때는 화면 켜진 상태
    //blackOverlay->hide();

    // weather panel(초기위치)
    WeatherWidget = new WeatherPanel(ui->centralWidget);
    WeatherWidget->setGeometry(1100, 1200, 800, 500);
    WeatherWidget->hide();
    QTimer::singleShot(
        1000,
        this,
        [=]()
        {
            WeatherWidget->move(1080, 460);
            WeatherWidget->show();
            blackOverlay->raise();
        });

    // ── 미디어 재생용 QProcess 단독 할당 ──
    mpvProcess = new QProcess(this);

    // ── 오른쪽 위로 위치 설정 ──────────────────
    ui->dateLabel->move(1450, 20);
    ui->timeLabel->move(1380, 50);
    ui->lcdNumberTemp->move(1280, 180);
    ui->label->move(1430, 180);
    ui->lcdNumberHumi->move(1280, 260);
    ui->label_2->move(1430, 260);
    ui->labelAQI->move(1280, 360);

    // 크기 설정
    ui->timeLabel->resize(400, 120);
    ui->lcdNumberTemp->resize(130, 50);
    ui->lcdNumberHumi->resize(130, 50);
    ui->label->resize(120, 50);
    ui->label_2->resize(160, 50);

    newsWidget = new NewsPanel(ui->centralWidget);
    newsWidget->setGeometry(1920, 520, 760, 620);
    newsWidget->hide();

}

MainWindow::~MainWindow()
{
    if (emotionProcess && emotionProcess->state() != QProcess::NotRunning) {
        emotionProcess->kill();
        emotionProcess->waitForFinished(1000);
    }
    if (mpvProcess && mpvProcess->state() != QProcess::NotRunning) {
        mpvProcess->kill();
        mpvProcess->waitForFinished(1000);
    }
    delete ui;
}

// ── 새 아두이노 연결 ────────────────────────────
void MainWindow::onNewConnection()
{
    if (tcpSocket)
    {
        tcpSocket->disconnectFromHost();
        tcpSocket->deleteLater();
    }

    tcpSocket = tcpServer->nextPendingConnection();
    connect(tcpSocket, &QTcpSocket::readyRead,
            this, &MainWindow::onDataReceived);
    connect(tcpSocket, &QTcpSocket::disconnected,
            this, &MainWindow::onClientDisconnected);

    qDebug() << "아두이노 연결됨:" << tcpSocket->peerAddress().toString();
}

// ── 새 9001 연결 ────────────────────────────
void MainWindow::onNewGestureConnection()
{
    if (tcpSocket9001)
    {
        tcpSocket9001->disconnectFromHost();
        tcpSocket9001->deleteLater();
    }

    tcpSocket9001 = tcpServer9001->nextPendingConnection();
    connect(tcpSocket9001, &QTcpSocket::readyRead,
            this, &MainWindow::onGestureDataReceived);
    connect(tcpSocket9001, &QTcpSocket::disconnected,
            this, &MainWindow::onGestureClientDisconnected);

    qDebug() << "9001 연결됨:" << tcpSocket9001->peerAddress().toString();
}

// ── 데이터 수신 ─────────────────────────────────
void MainWindow::onDataReceived()
{
    if (!tcpSocket)
        return;

    while (tcpSocket->canReadLine())
    {
        QByteArray raw = tcpSocket->readLine();
        QString data = QString::fromUtf8(raw).trimmed();

        // 데이터 수신 시 포함될 수 있는 큰따옴표 지우기 (전처리 방어코드)
        if (data.startsWith("\"") && data.endsWith("\"")) {
            data = data.mid(1, data.length() - 2);
        }
        data = data.trimmed();

        qDebug() << "수신:" << data;
        processData(data);
    }
}

// ── 9001 데이터 수신 ─────────────────────────────────
void MainWindow::onGestureDataReceived()
{
    if (!tcpSocket9001)
        return;

    while (tcpSocket9001->canReadLine())
    {
        QByteArray raw = tcpSocket9001->readLine();
        QString data = QString::fromUtf8(raw).trimmed();

        // 제스처 수신 시 포함될 수 있는 큰따옴표 지우기
        if (data.startsWith("\"") && data.endsWith("\"")) {
            data = data.mid(1, data.length() - 2);
        }
        data = data.trimmed();

        qDebug() << "9001 수신:" << data;
        gestureDetected(data);
    }
}

// ── 연결 종료 ───────────────────────────────────
void MainWindow::onClientDisconnected()
{
    qDebug() << "아두이노 연결 종료";
    qDebug() << "아두이노 대기 중 - 포트:" << ARDUINO_PORT;
    if (tcpSocket)
    {
        tcpSocket->deleteLater();
        tcpSocket = nullptr;
    }
}

// ── 9001 연결 종료 ───────────────────────────────────
void MainWindow::onGestureClientDisconnected()
{
    qDebug() << "9001 연결 종료";
    qDebug() << "9001 대기 중 - 포트:" << GESTURE_PORT;
    if (tcpSocket9001)
    {
        tcpSocket9001->deleteLater();
        tcpSocket9001 = nullptr;
    }
}

// ── 밝기 오버레이 적용 ──────────────────────────
void MainWindow::applyBrightness(int briVal)
{
    int brightness = qBound(80, (briVal * 255) / 550, 255);

    QString color = QString("rgb(%1,%1,%1)").arg(brightness);

    ui->dateLabel->setStyleSheet(
        QString("color:%1; font-size:12pt; font-weight:400; background:none;").arg(color));

    ui->timeLabel->setStyleSheet(
        QString("color:%1; font-size:45pt; font-weight:bold; margin-top:-5px; background:none;").arg(color));

    ui->labelAQI->setStyleSheet(
        QString("color:%1; font-size:20pt; background:none; font-family:'Noto Color Emoji';").arg(color));

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

// ── 데이터 파싱 및 UI 업데이트 ──────────────────
void MainWindow::processData(const QString &data)
{
    // [보정 1순위] 최상단에서 OFF/ON 제어권을 가로채 다른 조건문 분기와의 꼬임 및 오동작 완벽 차단
    if (data == "OFF")
    {
        blackOverlay->show();
        blackOverlay->raise();
        waitingData = false;

        if (emotionProcess && emotionProcess->state() != QProcess::NotRunning) {
            qDebug() << "아두이노 OFF: 파이썬 감정 분석 스크립트(opencv_latest.py)를 완전히 종료합니다.";
            emotionProcess->kill();
            emotionProcess->waitForFinished(1000);
        }

        if (mpvProcess && mpvProcess->state() != QProcess::NotRunning) {
            qDebug() << "아두이노 OFF: 가동 중이던 음악 스트리밍(mpv) 프로세스를 완전 종료합니다.";
            mpvProcess->kill();
            mpvProcess->waitForFinished(1000);
            isPaused = false; // 플래그 초기화
        }
        return;
    }

    if (data == "ON")
    {
        if (emotionProcess && emotionProcess->state() == QProcess::NotRunning) {
            emotionProcess->setWorkingDirectory("/home/jt-user/test/opencv");

            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            // 기존: contains 체크 후 삽입 → 이미 잘못된 DISPLAY일 수도 있음
            // 수정: 무조건 덮어쓰기
            env.insert("DISPLAY", ":0");
            env.insert("XAUTHORITY", "/home/jt-user/.Xauthority"); // ← 추가
            emotionProcess->setProcessEnvironment(env);

            // 프로세스 크래시가 Qt에 전파되지 않도록 에러 시그널 연결
            connect(emotionProcess, &QProcess::errorOccurred,
                    this, [](QProcess::ProcessError error) {
                qWarning() << "파이썬 프로세스 오류 발생:" << error;
                // Qt는 계속 살아있음
            });

            emotionProcess->start("/home/jt-user/deepface_env/bin/python3",
                                  QStringList() << "opencv_latest.py");
            qDebug() << "아두이노 ON: 파이썬 감정 분석 스크립트를 시작합니다.";
        }
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
        case 1: emoji = "좋음"; break;
        case 2: emoji = "보통"; break;
        case 3: emoji = "나쁨"; break;
        case 4: emoji = "위험"; break;
        case 5: emoji = "죽음"; break;
        default: emoji = "오류"; break;
        }
        ui->labelAQI->setText(emoji);

        if (data.contains("BRI:"))
        {
            applyBrightness(briVal);
        }

        if (waitingData)
        {
            blackOverlay->hide();
            waitingData = false;
        }
    }
}

// ── 시계 업데이트 ───────────────────────────────
void MainWindow::updateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QLocale koLocale(QLocale::Korean, QLocale::SouthKorea);

    ui->dateLabel->setText(koLocale.toString(current, "M월 d일 dddd"));
    ui->timeLabel->setText(current.toString("h:mm"));
}

// ── 창 크기 변경 ────────────────────────────────
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (blackOverlay)
        blackOverlay->setGeometry(ui->centralWidget->rect());
}

// ── mpv 단독 제스처 처리 루틴 ──────────────────
void MainWindow::gestureDetected(const QString &gesture)
{
    qDebug() << "🔍 [제스처 검증 완료] 진입 명령어:" << gesture;


    if (gesture.startsWith("KEYWORD"))
    {
        keyword = gesture.section(':', 1, 1).trimmed();
    }
    // VOLUME_UP = START 역할
    else if (gesture == "START")
    {
        // 멈춘 지점부터 이어 재생하기 (IPC 통신 이용)
        if (isPaused && mpvProcess->state() == QProcess::Running)
        {
            qDebug() << "▶️ [START - Resume] 일시정지 상태 탈출: 멈춘 지점부터 다시 재생합니다.";
            QProcess::execute("sh", QStringList() << "-c"
                                                  << "echo '{\"command\":[\"set_property\",\"pause\",false]}' | socat - /tmp/mpv-socket");
            isPaused = false;
        }
        else // 최초 재생 혹은 프로세스가 꺼진 상태일 때 새 인스턴스 생성
        {
            qDebug() << "🎬 [START - New] 최초 재생 감지: mpv 스트리밍을 처음부터 구동합니다.";

            if (mpvProcess->state() != QProcess::NotRunning)
            {
                mpvProcess->kill();
                mpvProcess->waitForFinished(1000);
            }

            mpvProcess->setProcessChannelMode(QProcess::ForwardedChannels);
            QString searchTarget;
            if (keyword != NULL)
            {
                searchTarget = "ytdl://ytsearch1:" + keyword + " 플레이리스트";
            }
            else
            {
                searchTarget = "ytdl://ytsearch1:잔잔한 플레이리스트";
            }

            QStringList arguments;
            arguments << "--no-video"
                      << "--input-ipc-server=/tmp/mpv-socket"
                      << "--ytdl-format=bestaudio/best"
                      << "--ao=alsa,pulse"
                      << "--gapless-audio=yes"
                      << searchTarget;

            qDebug() << "🎵 [mpv] 내부 파서 엔진 구동 및 스트리밍 개시...";
            mpvProcess->start("/usr/bin/mpv", arguments);

            if (!mpvProcess->waitForStarted(1000)) {
                qWarning() << "❌ 오류: mpv 프로세스를 구동하지 못했습니다.";
            }
            isPaused = false;
        }
    }
    // STOP 수신 시: 노래를 일시정지하고 일시정지 플래그 가동
    else if (gesture == "STOP")
    {
        if (mpvProcess->state() == QProcess::Running)
        {
            qDebug() << "⏸️ [STOP] 제스처 감지: 음악 일시정지 명령을 쏘아줍니다.";
            QProcess::execute("sh", QStringList() << "-c"
                                                  << "echo '{\"command\":[\"set_property\",\"pause\",true]}' | socat - /tmp/mpv-socket");
            isPaused = true;
        }
    }
    // END 수신 시: mpv 프로세스를 완전히 종료하고 플래그 해제
    else if (gesture == "END")
    {
        if (mpvProcess->state() != QProcess::NotRunning)
        {
            qDebug() << "⏹️ [END] 제스처 감지: 모든 오디오 재생 프로세스를 완전 종료합니다.";
            mpvProcess->kill();
            mpvProcess->waitForFinished(1000);
            isPaused = false;
        }
    }
    //왼쪽은 뉴스, 오른쪽은 날씨
    else if (gesture == "LEFT")
    {
        qDebug() << "⬅ LEFT : 뉴스 패널";
        showNewsPanel();
    }
    else if (gesture == "RIGHT")
    {
        qDebug() << "➡ RIGHT : 날씨 패널";
        showWeatherPanel();
    }
}

// ── Weather Panel 보이기 ──────────────────────────
void MainWindow::showWeatherPanel()
{
    if(animationRunning)
        return;

    if(isWeatherVisible)
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
    weatherAnim->setStartValue(QPoint(1080,1200));
    weatherAnim->setEndValue(QPoint(1080, 460));

    newsAnim->start(QAbstractAnimation::DeleteWhenStopped);
    weatherAnim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(newsAnim, &QPropertyAnimation::finished, newsWidget, &QWidget::hide);
    connect(weatherAnim,&QPropertyAnimation::finished,this,[this](){
        animationRunning = false;
    });
}

// ── News Panel 보이기 ──────────────────────────
void MainWindow::showNewsPanel()
{
    if(animationRunning)
        return;

    if(isNewsVisible)
        return;

    animationRunning = true;

    isNewsVisible = true;
    isWeatherVisible = false;

    newsWidget->show();

    QPropertyAnimation *weatherAnim = new QPropertyAnimation(WeatherWidget, "pos");
    weatherAnim->setDuration(700);
    weatherAnim->setStartValue(QPoint(1080,460));
    weatherAnim->setEndValue(QPoint(1080, 1200));

    QPropertyAnimation *newsAnim = new QPropertyAnimation(newsWidget, "pos");
    newsAnim->setDuration(waitingData);
    newsAnim->setStartValue(QPoint(1920,520));
    newsAnim->setEndValue(QPoint(1180, 520));

    weatherAnim->start(QAbstractAnimation::DeleteWhenStopped);
    newsAnim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(weatherAnim, &QPropertyAnimation::finished, WeatherWidget, &QWidget::hide);
    connect(newsAnim,&QPropertyAnimation::finished,this,[this](){
        animationRunning = false;
    });
}
/*
//테스트용
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch(event->key())
    {
    case Qt::Key_1:
        showNewsPanel();
        break;

    case Qt::Key_2:
        showWeatherPanel();
        break;

    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}
*/
