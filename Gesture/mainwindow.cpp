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

// ── 리눅스 표준 소켓 및 시스템 헤더 (close, UDP 송신용) ──
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const quint16 ARDUINO_PORT = 9000;
const int     QT_UDP_PORT  = 9001;

// ============================================================
//  MainWindow 생성자
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpSocket(nullptr)
    , gestureWorker(nullptr)
    , _qtUdpSock(-1)
    , waitingData(false)
{
    ui->setupUi(this);
    this->move(930, 110);

    // ── 내부 제스처 데이터 송신용 UDP 소켓 초기화 ──────────
    initUdpSocket();

    // ── TCP 서버 시작 (아두이노 전용) ─────────────────────
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection,
            this,      &MainWindow::onNewConnection);

    if (!tcpServer->listen(QHostAddress::Any, ARDUINO_PORT)) {
        qWarning() << "TCP 서버 시작 실패:" << tcpServer->errorString();
    } else {
        qDebug() << "아두이노 대기 중 - 포트:" << ARDUINO_PORT;
    }

    // ── 시계 타이머 ────────────────────────────
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();

    // ── 전체 스타일 ────────────────────────────
    this->setStyleSheet("background-color:#333;");

    ui->dateLabel->setStyleSheet(
        "color:white; font-size:12pt; font-weight:400; background:none;");
    ui->timeLabel->setStyleSheet(
        "color:white; font-size:45pt; font-weight:bold; margin-top:-5px; background:none;");
    ui->labelAQI->setStyleSheet(
        "color:white; font-size:20pt; background:none;");
    ui->lcdNumberTemp->setStyleSheet("color:lime; background:#111;");
    ui->lcdNumberHumi->setStyleSheet("color:cyan; background:#111;");

    // ── 밝기 오버레이 (UI 위에 올라가되 이벤트는 통과) ──
    overlayWidget = new QWidget(ui->centralWidget);
    overlayWidget->setGeometry(ui->centralWidget->rect());
    overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlayWidget->setStyleSheet("background-color: rgba(0,0,0,0);");
    overlayWidget->raise();
    overlayWidget->show();

    // ── 화면 ON/OFF 오버레이 (맨 위) ──────────
    blackOverlay = new QFrame(ui->centralWidget);
    blackOverlay->setGeometry(ui->centralWidget->rect());
    blackOverlay->setStyleSheet("background-color:black;");
    blackOverlay->raise();
    blackOverlay->show(); // 시작할 때는 화면 켜진 상태

    // ── 미디어 프로세스 초기화 ─────────────────────
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

    // ── OpenCV 제스처 워커 스레드 생성 및 구동 ──────────
    startGestureWorker();
}

// ============================================================
//  MainWindow 소멸자
// ============================================================
MainWindow::~MainWindow()
{
    // 스레드가 안전하게 종료되도록 제어
    if (gestureWorker) {
        gestureWorker->stop();
        gestureWorker->wait(3000);
        delete gestureWorker;
    }
    if (_qtUdpSock != -1) {
        ::close(_qtUdpSock);
    }
    delete ui;
}

// ============================================================
//  내부 통신용 UDP 소켓 설정 함수
// ============================================================
void MainWindow::initUdpSocket()
{
    _qtUdpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (_qtUdpSock < 0) {
        qWarning() << "내부 연동용 UDP 소켓 생성 실패";
        return;
    }
    _qtAddr.sin_family      = AF_INET;
    _qtAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    _qtAddr.sin_port        = htons(QT_UDP_PORT);
}

// ============================================================
//  OpenCV 제스처 스레드 가동 및 신호 연결 함수
// ============================================================
void MainWindow::startGestureWorker()
{
    // 스레드 객체 생성 (UDP 파일 디스크립터와 주소 구조체 전달)
    gestureWorker = new GestureWorker(_qtUdpSock, _qtAddr, this);

    // 스레드에서 감지된 제스처 데이터를 메인 스레드의 슬롯으로 토스
    connect(gestureWorker, &GestureWorker::gestureDetected,
            this,          &MainWindow::gestureDetected,
            Qt::QueuedConnection);

    gestureWorker->start();
    qDebug() << "OpenCV 제스처 감지 워커 스레드가 백그라운드에서 실행되었습니다.";
}

// ── 새 아두이노 연결 ────────────────────────────
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

// ── 데이터 수신 ─────────────────────────────────
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

// ── 연결 종료 ───────────────────────────────────
void MainWindow::onClientDisconnected()
{
    qDebug() << "아두이노 연결 종료";
    qDebug() << "아두이노 대기 중 - 포트:" << ARDUINO_PORT;
    if (tcpSocket) {
        tcpSocket->deleteLater();
        tcpSocket = nullptr;
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
    QLocale   koLocale(QLocale::Korean, QLocale::SouthKorea);

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

// ============================================================
//  제스처 수신 처리 및 UI/시스템 제어 슬롯
// ============================================================
void MainWindow::gestureDetected(const QString &gesture)
{
    qDebug() << "[제스처 최상위 수신]" << gesture;

    // 접두사 "GESTURE:" 처리 유연화
    QString g = gesture;
    if (g.startsWith("GESTURE:")) {
        g = g.mid(8);
    }

    // 1. 음악 시작 제스처
    if (g == "START" || g == "HAND_OPEN") {
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

    // 2. 음악 일시정지 제스처
    } else if (g == "STOP" || g == "HAND_FIST") {
        if (mpvProcess->state() == QProcess::Running) {
            QProcess::execute("sh", QStringList() << "-c"
                << "echo '{\"command\":[\"cycle\",\"pause\"]}' | socat - /tmp/mpv-socket");
        }

    // 3. 볼륨 업 제스처 (손 쓸어 올리기)
    } else if (g == "SWIPE_UP") {
        qDebug() << "[UI 제어] 시스템 볼륨 +10%";
        QProcess::execute("sh", QStringList() << "-c" << "amixer set Master 10%+");

    // 4. 볼륨 다운 제스처 (손 쓸어 내리기)
    } else if (g == "SWIPE_DOWN") {
        qDebug() << "[UI 제어] 시스템 볼륨 -10%";
        QProcess::execute("sh", QStringList() << "-c" << "amixer set Master 10%-");

    // 5. 화면 넘기기 또는 커스텀 액션 (좌/우 스와이프 발생 시 동작 지정부)
    } else if (g == "SWIPE_LEFT") {
        qDebug() << "[UI 제어] 왼쪽 스와이프 액션 실행 (예: 다음 페이지)";
        // 여기에 페이지 전환용 코드 추가 가능: ui->stackedWidget->setCurrentIndex(...);

    } else if (g == "SWIPE_RIGHT") {
        qDebug() << "[UI 제어] 오른쪽 스와이프 액션 실행 (예: 이전 페이지)";

    // 6. 음악 전체 종료 제스처
    } else if (g == "END") {
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            qDebug() << "재생 완전 종료";
        }
    }
}
