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
#include <QUdpSocket> // UDP 인클루드 추가

const quint16 ARDUINO_PORT = 9000;
const quint16 GESTURE_PORT = 9001; // UDP 제스처 수신용 포트 정의

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpSocket(nullptr)
    , udpSocket(nullptr) // 초기화 리스트 추가
{
    ui->setupUi(this);

    this->resize(1920, 1080);

    // ── TCP 서버 시작 ──────────────────────────
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection,
            this,      &MainWindow::onNewConnection);

    if (!tcpServer->listen(QHostAddress::Any, ARDUINO_PORT)) {
        qWarning() << "TCP 서버 시작 실패:" << tcpServer->errorString();
    } else {
        qDebug() << "아두이노 대기 중 - 포트:" << ARDUINO_PORT;
    }

    // ── UDP 서버(소켓) 시작 ──────────────────────
    udpSocket = new QUdpSocket(this);
    // 모든 IP로부터 GESTURE_PORT(9001)로 들어오는 UDP 패킷을 감지합니다.
    if (!udpSocket->bind(QHostAddress::Any, GESTURE_PORT)) {
        qWarning() << "UDP 소켓 바인딩 실패:" << udpSocket->errorString();
    } else {
        qDebug() << "제스처 UDP 수신 대기 중 - 포트:" << GESTURE_PORT;
        connect(udpSocket, &QUdpSocket::readyRead,
                this,      &MainWindow::onUdpDataReceived);
    }

    // ── 시계 타이머 ────────────────────────────
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();

    // ── 전체 스타일 ────────────────────────────
    this->setStyleSheet("background-color:#333;");

    ui->dateLabel->setStyleSheet(
        "color:white;"
        "font-size:12pt;"
        "font-weight:400;"
        "background:none;"
    );
    ui->timeLabel->setStyleSheet(
        "color:white;"
        "font-size:45pt;"
        "font-weight:bold;"
        "margin-top:-5px;"
        "background:none;"
    );
    ui->labelAQI->setStyleSheet(
        "color:white;"
        "font-size:20pt;"
        "background:none;"
    );
    ui->lcdNumberTemp->setStyleSheet("color:lime; background:#111;");
    ui->lcdNumberHumi->setStyleSheet("color:cyan; background:#111;");

    // ── 밝기 오버레이 (UI 위에 올라가되 이벤트는 통과) ──
    overlayWidget = new QWidget(ui->centralWidget);
    overlayWidget->setGeometry(ui->centralWidget->rect());
    overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlayWidget->setStyleSheet("background-color: rgba(0,0,0,0);");
    //overlayWidget->raise();
    //overlayWidget->show();
    overlayWidget->hide();

    // ── 화면 ON/OFF 오버레이 (맨 위) ──────────
    blackOverlay = new QFrame(ui->centralWidget);
    blackOverlay->setGeometry(ui->centralWidget->rect());
    blackOverlay->setStyleSheet("background-color:black;");
    blackOverlay->hide();

    // weather panel
    WeatherWidget = new WeatherPanel(this);
    WeatherWidget->setGeometry(1100, 1200, 800, 500);
    QTimer::singleShot(1000, this, [=]() {
        showWeatherPanel();
    });

    // gesture detected
    ytDlpProcess = new QProcess(this);
    mpvProcess   = new QProcess(this);

    // yt-dlp 완료 시 mpv 실행
    connect(ytDlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) {
            qWarning() << "yt-dlp 실패";
            return;
        }
        QString url = QString::fromUtf8(ytDlpProcess->readAllStandardOutput()).trimmed();
        if (url.isEmpty()) {
            qWarning() << "URL 추출 실패";
            return;
        }
        qDebug() << "재생 URL:" << url;

        // 기존 mpv 종료 후 재시작
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            mpvProcess->waitForFinished(1000);
        }
        mpvProcess->start("mpv", QStringList()
            << "--no-video"
            << "--input-ipc-server=/tmp/mpv-socket"
            << url);
    });

    // 오른쪽 위로 위치 설정
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
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ── 새 아두이노 연결 (TCP) ────────────────────────────
void MainWindow::onNewConnection()
{
    if (tcpSocket) {
        tcpSocket->disconnectFromHost();
        tcpSocket->deleteLater();
    }

    tcpSocket = tcpServer->nextPendingConnection();
    connect(tcpSocket, &QTcpSocket::readyRead,
            this,      &MainWindow::onDataReceived);
    connect(tcpSocket, &QTcpSocket::disconnected,
            this,      &MainWindow::onClientDisconnected);

    qDebug() << "아두이노 연결됨:" << tcpSocket->peerAddress().toString();
}

// ── 데이터 수신 (TCP) ─────────────────────────────────
void MainWindow::onDataReceived()
{
    if (!tcpSocket) return;

    while (tcpSocket->canReadLine()) {
        QByteArray raw  = tcpSocket->readLine();
        QString    data = QString::fromUtf8(raw).trimmed();
        qDebug() << "수신:" << data;
        processData(data);
    }
}

// ── 연결 종료 (TCP) ───────────────────────────────────
void MainWindow::onClientDisconnected()
{
    qDebug() << "아두이노 연결 종료";
    qDebug() << "아두이노 대기 중 - 포트:" << ARDUINO_PORT;
    if (tcpSocket) {
        tcpSocket->deleteLater();
        tcpSocket = nullptr;
    }
}

// ── UDP 데이터 수신 처리 (새로 추가됨) ──────────────────────
void MainWindow::onUdpDataReceived()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(udpSocket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort;

        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        // 받은 데이터를 QString으로 변환 및 공백 제거
        QString gesture = QString::fromUtf8(datagram).trimmed();
        qDebug() << "UDP 수신 [" << sender.toString() << ":" << senderPort << "] ->" << gesture;

        // 제스처 처리 함수 호출
        gestureDetected(gesture);
    }
}

// ── 밝기 오버레이 적용 ──────────────────────────
void MainWindow::applyBrightness(int briVal)
{
    if (briVal >= 0 && briVal <= 200) {
        int alpha = ((200 - briVal) * 180) / 200;
        overlayWidget->setStyleSheet(
            QString("background-color: rgba(0,0,0,%1);").arg(alpha));
    } else if (briVal >= 201 && briVal <= 350) {
        overlayWidget->setStyleSheet("background-color: rgba(0,0,0,0);");
    } else {
        int alpha = qBound(0, ((briVal - 350) * 180) / 200, 180);
        overlayWidget->setStyleSheet(
            QString("background-color: rgba(255,255,255,%1);").arg(alpha));
    }
}

// ── 데이터 파싱 및 UI 업데이트 ──────────────────
void MainWindow::processData(const QString &data)
{
    if (data == "OFF") {
        blackOverlay->show();
        blackOverlay->raise();
        waitingData = false;
        return;
    }
    if (data == "ON") {
        waitingData = true;
        return;
    }
    if(data == "SHOW_WEATHER") {
        showWeatherPanel();
        return;
    }
    if(data == "HIDE_WEATHER") {
        hideWeatherPanel();
        return;
    }

    if (data.startsWith("BRI:")) {
        int briVal = data.section("BRI:", 1, 1).trimmed().toInt();
        applyBrightness(briVal);
        return;
    }

    if (data.startsWith("TEMP:")) {
        QString tempStr = data.section("TEMP:", 1, 1).section(",", 0, 0).trimmed();
        QString humiStr = data.section("HUMI:", 1, 1).section(",", 0, 0).trimmed();
        QString aqiStr  = data.section("AQI:",  1, 1).section(",", 0, 0).trimmed();
        QString briStr  = data.section("BRI:",  1, 1).section(",", 0, 0).trimmed();

        double tempVal = tempStr.toDouble();
        double humiVal = humiStr.toDouble();
        int    aqiVal  = aqiStr.toInt();
        int    briVal  = briStr.toInt();

        ui->lcdNumberTemp->display(QString::number(tempVal, 'f', 1));
        ui->lcdNumberHumi->display(QString::number(humiVal, 'f', 1));

        QString emoji;
        switch (aqiVal) {
            case 1: emoji = "🙂"; break;
            case 2: emoji = "😐"; break;
            case 3: emoji = "😷"; break;
            case 4: emoji = "☠️"; break;
            case 5: emoji = "😀"; break;
            default: emoji = "오류"; break;
        }
        ui->labelAQI->setText(emoji);

        if (data.contains("BRI:")) {
            applyBrightness(briVal);
        }

        if (waitingData) {
            blackOverlay->hide();
            waitingData = false;
        }
    }
}

// ── 시계 업데이트 ───────────────────────────────
void MainWindow::updateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QLocale    koLocale(QLocale::Korean, QLocale::SouthKorea);

    ui->dateLabel->setText(koLocale.toString(current, "M월 d일 dddd"));
    ui->timeLabel->setText(current.toString("h:mm"));
}

// ── 창 크기 변경 ────────────────────────────────
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (blackOverlay)  blackOverlay->setGeometry(ui->centralWidget->rect());
    if (overlayWidget) overlayWidget->setGeometry(ui->centralWidget->rect());
}

// ── 제스처 수신 처리 ────────────────────────────
void MainWindow::gestureDetected(const QString &gesture)
{
    qDebug() << "제스처 수신:" << gesture;

    if (gesture == "START") {
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            mpvProcess->waitForFinished(1000);
        }
        if (ytDlpProcess->state() != QProcess::NotRunning) {
            ytDlpProcess->kill();
            ytDlpProcess->waitForFinished(1000);
        }

        ytDlpProcess->start("yt-dlp", QStringList()
            << "ytsearch1:기분 좋을 때 듣는 노래"
            << "--get-url"
            << "--format" << "bestaudio");

    } else if (gesture == "STOP") {
        if (mpvProcess->state() == QProcess::Running) {
            QProcess::execute("sh", QStringList() << "-c"
                << "echo '{\"command\":[\"cycle\",\"pause\"]}' | socat - /tmp/mpv-socket");
        }

    } else if (gesture == "END") {
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            qDebug() << "재생 종료";
        }
    }
}

// Weather Panel 보이기
void MainWindow::showWeatherPanel()
{
    WeatherWidget->show();
    QPropertyAnimation *anim = new QPropertyAnimation(WeatherWidget, "pos");
    anim->setDuration(500);
    anim->setStartValue(QPoint(1100,1200));
    anim->setEndValue(QPoint(1100,500));
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// Weather Panel 숨기기
void MainWindow::hideWeatherPanel()
{
    QPropertyAnimation *anim = new QPropertyAnimation(WeatherWidget, "pos");
    anim->setDuration(500);
    anim->setStartValue(QPoint(1100,500));
    anim->setEndValue(QPoint(1100,1200));
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
