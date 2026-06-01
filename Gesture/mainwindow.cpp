#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QLocale>
#include <QDebug>
#include <QFrame>
#include <QResizeEvent>
#include <QMessageBox>

// ============================================================
//  생성자
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->move(930, 110);

    // ── UDP 소켓 초기화 ───────────────────────────
    initUdpSocket();

    // ── 시계 타이머 ───────────────────────────────
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();

    // ── 전체 스타일 ───────────────────────────────
    this->setStyleSheet("background-color:#333;");
    ui->dateLabel->setStyleSheet(
        "color:white; font-size:12pt; font-weight:400; background:none;");
    ui->timeLabel->setStyleSheet(
        "color:white; font-size:45pt; font-weight:bold; margin-top:-5px; background:none;");
    ui->labelAQI->setStyleSheet(
        "color:white; font-size:20pt; background:none;");
    ui->lcdNumberTemp->setStyleSheet("color:lime; background:#111;");
    ui->lcdNumberHumi->setStyleSheet("color:cyan; background:#111;");

    // ── 밝기 오버레이 ─────────────────────────────
    overlayWidget = new QWidget(ui->centralWidget);
    overlayWidget->setGeometry(ui->centralWidget->rect());
    overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlayWidget->setStyleSheet("background-color: rgba(0,0,0,0);");
    overlayWidget->raise();
    overlayWidget->show();

    // ── 화면 ON/OFF 오버레이 ─────────────────────
    blackOverlay = new QFrame(ui->centralWidget);
    blackOverlay->setGeometry(ui->centralWidget->rect());
    blackOverlay->setStyleSheet("background-color:black;");
    blackOverlay->raise();
    blackOverlay->show();

    // ── 미디어 프로세스 ───────────────────────────
    ytDlpProcess = new QProcess(this);
    mpvProcess   = new QProcess(this);

    connect(ytDlpProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) { qWarning() << "yt-dlp 실패"; return; }
        QString url = QString::fromUtf8(ytDlpProcess->readAllStandardOutput()).trimmed();
        if (url.isEmpty()) { qWarning() << "URL 추출 실패"; return; }
        qDebug() << "재생 URL:" << url;
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            mpvProcess->waitForFinished(1000);
        }
        mpvProcess->start("mpv", QStringList()
            << "--no-video"
            << "--input-ipc-server=/tmp/mpv-socket"
            << url);
    });

    // ── 워커 스레드 시작 ──────────────────────────
    startWorkerThreads();
}

// ============================================================
//  소멸자
// ============================================================
MainWindow::~MainWindow()
{
    // 워커 스레드 안전하게 종료
    if (gestureWorker) {
        gestureWorker->stop();
        gestureWorker->wait(3000);
        delete gestureWorker;
    }
    if (arduinoWorker) {
        arduinoWorker->stop();
        arduinoWorker->wait(3000);
        delete arduinoWorker;
    }
    if (_qtUdpSock != -1) {
        ::close(_qtUdpSock);
    }
    delete ui;
}

// ============================================================
//  UDP 소켓 초기화
// ============================================================
void MainWindow::initUdpSocket()
{
    _qtUdpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (_qtUdpSock < 0) {
        qWarning() << "[MainWindow] UDP 소켓 생성 실패";
        return;
    }
    _qtAddr.sin_family      = AF_INET;
    _qtAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    _qtAddr.sin_port        = htons(QT_UDP_PORT);
    qDebug() << "[MainWindow] UDP 소켓 초기화 완료 - 포트:" << QT_UDP_PORT;
}

// ============================================================
//  워커 스레드 시작
// ============================================================
void MainWindow::startWorkerThreads()
{
    // ── 1. 제스처 워커 스레드 ─────────────────────
    gestureWorker = new GestureWorker(_qtUdpSock, _qtAddr, this);

    // 제스처 감지 시그널 → MainWindow 슬롯 연결
    // Qt::QueuedConnection: 다른 스레드에서 발생한 시그널을 메인 스레드에서 안전하게 처리
    connect(gestureWorker, &GestureWorker::gestureDetected,
            this,          &MainWindow::gestureDetected,
            Qt::QueuedConnection);

    connect(gestureWorker, &GestureWorker::cameraError,
            this,          &MainWindow::onCameraError,
            Qt::QueuedConnection);

    gestureWorker->start();
    qDebug() << "[MainWindow] 제스처 워커 스레드 시작됨";

    // ── 2. 아두이노 워커 스레드 ───────────────────
    arduinoWorker = new ArduinoWorker(ARDUINO_TCP_PORT, _qtUdpSock, _qtAddr, this);

    connect(arduinoWorker, &ArduinoWorker::arduinoDataReceived,
            this,          &MainWindow::onArduinoData,
            Qt::QueuedConnection);

    connect(arduinoWorker, &ArduinoWorker::arduinoConnected,
            this,          &MainWindow::onArduinoConnected,
            Qt::QueuedConnection);

    connect(arduinoWorker, &ArduinoWorker::arduinoDisconnected,
            this,          &MainWindow::onArduinoDisconnected,
            Qt::QueuedConnection);

    arduinoWorker->start();
    qDebug() << "[MainWindow] 아두이노 워커 스레드 시작됨";
}

// ============================================================
//  아두이노 워커 슬롯
// ============================================================
void MainWindow::onArduinoData(const QString& data)
{
    qDebug() << "[아두이노 수신]" << data;
    processData(data);
}

void MainWindow::onArduinoConnected(const QString& ip)
{
    qDebug() << "[아두이노 연결됨]" << ip;
}

void MainWindow::onArduinoDisconnected()
{
    qDebug() << "[아두이노 연결 종료]";
}

// ============================================================
//  카메라 오류 슬롯
// ============================================================
void MainWindow::onCameraError(const QString& message)
{
    qWarning() << "[카메라 오류]" << message;
    // 필요 시 UI에 알림 표시
    // QMessageBox::warning(this, "카메라 오류", message);
}

// ============================================================
//  기존 Qt TCP 슬롯 (호환성 유지)
// ============================================================
void MainWindow::onNewConnection()
{
    if (tcpSocket) {
        tcpSocket->disconnectFromHost();
        tcpSocket->deleteLater();
    }
    tcpSocket = tcpServer ? tcpServer->nextPendingConnection() : nullptr;
    if (!tcpSocket) return;

    connect(tcpSocket, &QTcpSocket::readyRead,
            this,      &MainWindow::onDataReceived);
    connect(tcpSocket, &QTcpSocket::disconnected,
            this,      &MainWindow::onClientDisconnected);
    qDebug() << "[Qt TCP] 연결됨:" << tcpSocket->peerAddress().toString();
}

void MainWindow::onDataReceived()
{
    if (!tcpSocket) return;
    while (tcpSocket->canReadLine()) {
        QString data = QString::fromUtf8(tcpSocket->readLine()).trimmed();
        processData(data);
    }
}

void MainWindow::onClientDisconnected()
{
    qDebug() << "[Qt TCP] 연결 종료";
    if (tcpSocket) {
        tcpSocket->deleteLater();
        tcpSocket = nullptr;
    }
}

// ============================================================
//  밝기 오버레이 적용
// ============================================================
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

// ============================================================
//  데이터 파싱 및 UI 업데이트
// ============================================================
void MainWindow::processData(const QString& data)
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
    if (data.startsWith("BRI:")) {
        int briVal = data.section("BRI:", 1, 1).trimmed().toInt();
        applyBrightness(briVal);
        return;
    }
    if (data.startsWith("TEMP:")) {
        double tempVal = data.section("TEMP:", 1, 1).section(",", 0, 0).trimmed().toDouble();
        double humiVal = data.section("HUMI:", 1, 1).section(",", 0, 0).trimmed().toDouble();
        int    aqiVal  = data.section("AQI:",  1, 1).section(",", 0, 0).trimmed().toInt();
        int    briVal  = data.section("BRI:",  1, 1).section(",", 0, 0).trimmed().toInt();

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

        if (data.contains("BRI:")) applyBrightness(briVal);

        if (waitingData) {
            blackOverlay->hide();
            waitingData = false;
        }
    }
}

// ============================================================
//  제스처 수신 처리
// ============================================================
void MainWindow::gestureDetected(const QString& gesture)
{
    qDebug() << "[제스처 수신]" << gesture;

    // GESTURE: 접두사 제거
    QString g = gesture;
    if (g.startsWith("GESTURE:")) g = g.mid(8);

    if (g == "HAND_OPEN" || g == "START") {
        // 재생 시작
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

    } else if (g == "HAND_FIST" || g == "STOP") {
        // 일시정지 토글
        if (mpvProcess->state() == QProcess::Running) {
            QProcess::execute("sh", QStringList() << "-c"
                << "echo '{\"command\":[\"cycle\",\"pause\"]}' | socat - /tmp/mpv-socket");
        }

    } else if (g == "SWIPE_LEFT") {
        qDebug() << "[제스처] 다음 화면";
        // Qt UI 페이지 전환 로직 연결 가능

    } else if (g == "SWIPE_RIGHT") {
        qDebug() << "[제스처] 이전 화면";

    } else if (g == "SWIPE_UP") {
        qDebug() << "[제스처] 볼륨 증가";
        QProcess::execute("sh", QStringList() << "-c"
            << "amixer set Master 10%+");

    } else if (g == "SWIPE_DOWN") {
        qDebug() << "[제스처] 볼륨 감소";
        QProcess::execute("sh", QStringList() << "-c"
            << "amixer set Master 10%-");

    } else if (g == "END") {
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            qDebug() << "[제스처] 재생 종료";
        }
    }
}

// ============================================================
//  시계 업데이트
// ============================================================
void MainWindow::updateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QLocale   koLocale(QLocale::Korean, QLocale::SouthKorea);
    ui->dateLabel->setText(koLocale.toString(current, "M월 d일 dddd"));
    ui->timeLabel->setText(current.toString("h:mm"));
}

// ============================================================
//  창 크기 변경
// ============================================================
void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (blackOverlay)  blackOverlay->setGeometry(ui->centralWidget->rect());
    if (overlayWidget) overlayWidget->setGeometry(ui->centralWidget->rect());
}
