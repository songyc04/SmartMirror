#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QProcess>

// ── 소켓 및 시스템 헤더 ────────────────────────────
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const quint16 ARDUINO_PORT = 9000;
const int     QT_UDP_PORT  = 9001;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpSocket(nullptr)
    , gestureWorker(nullptr)
    , _qtUdpSock(-1)
{
    ui->setupUi(this);

    // ⭐ [핵심] 스마트 미러 UI 창을 모니터 화면에서 아예 안 보이게 숨깁니다.
    // 프로그램은 백그라운드 스레드로 계속 정상 동작합니다.
    this->hide();

    // ── 내부 통신용 UDP 소켓 초기화 ──────────
    initUdpSocket();

    // ── TCP 서버 시작 (아두이노 대기용) ─────────────────────
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection,
            this,      &MainWindow::onNewConnection);

    if (!tcpServer->listen(QHostAddress::Any, ARDUINO_PORT)) {
        qWarning() << "TCP 서버 시작 실패:" << tcpServer->errorString();
    } else {
        qDebug() << "[백그라운드 전용] 아두이노 TCP 연결 대기 중 - 포트:" << ARDUINO_PORT;
    }

    // ── 미디어 프로세스 초기화 ─────────────────────
    ytDlpProcess = new QProcess(this);
    mpvProcess   = new QProcess(this);

    connect(ytDlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) { qWarning() << "yt-dlp 실패"; return; }
        QString url = QString::fromUtf8(ytDlpProcess->readAllStandardOutput()).trimmed();
        if (url.isEmpty()) { qWarning() << "URL 추출 실패"; return; }
        
        qDebug() << "재생 URL 추출 성공 -> mpv 구동:" << url;
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            mpvProcess->waitForFinished(1000);
        }
        mpvProcess->start("mpv", QStringList()
            << "--no-video"
            << "--input-ipc-server=/tmp/mpv-socket"
            << url);
    });

    // ── OpenCV 제스처 워커 스레드 가동 ──────────
    startGestureWorker();
}

MainWindow::~MainWindow()
{
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

void MainWindow::startGestureWorker()
{
    gestureWorker = new GestureWorker(_qtUdpSock, _qtAddr, this);
    connect(gestureWorker, &GestureWorker::gestureDetected,
            this,          &MainWindow::gestureDetected,
            Qt::QueuedConnection);
    gestureWorker->start();
    qDebug() << "[백그라운드 전용] OpenCV 제스처 카메라 워커 구동 완료";
}

void MainWindow::onNewConnection()
{
    if (tcpSocket) {
        tcpSocket->disconnectFromHost();
        tcpSocket->deleteLater();
    }
    tcpSocket = tcpServer->nextPendingConnection();
    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::onDataReceived);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onClientDisconnected);
    qDebug() << "아두이노 접속됨:" << tcpSocket->peerAddress().toString();
}

void MainWindow::onDataReceived()
{
    if (!tcpSocket) return;
    while (tcpSocket->canReadLine()) {
        QString data = QString::fromUtf8(tcpSocket->readLine()).trimmed();
        qDebug() << "아두이노 데이터 수신:" << data;
        processData(data); // 센서 패킷 파싱 후 로그만 출력
    }
}

void MainWindow::onClientDisconnected()
{
    qDebug() << "아두이노 접속 끊김";
    if (tcpSocket) {
        tcpSocket->deleteLater();
        tcpSocket = nullptr;
    }
}

void MainWindow::processData(const QString &data)
{
    // 스마트 미러 UI 화면을 없앴으므로, 라벨이나 LCD 출력 부분을 과감히 생략하고 파싱 검증 로그만 남겨둡니다.
    if (data == "ON" || data == "OFF") {
        qDebug() << "[아두이노 명령 상태 변경]:" << data;
    } else if (data.startsWith("TEMP:")) {
        qDebug() << "[센서 패킷 가공 없이 정상 수신 확인]:" << data;
    }
}

// ============================================================
//  제스처 신호 제어 (기존 볼륨, 유튜브 로직 원본 유지)
// ============================================================
void MainWindow::gestureDetected(const QString &gesture)
{
    QString g = gesture;
    if (g.startsWith("GESTURE:")) g = g.mid(8);

    qDebug() << "[제스처 감지 백그라운드 트리거]:" << g;

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

    } else if (g == "STOP" || g == "HAND_FIST") {
        if (mpvProcess->state() == QProcess::Running) {
            QProcess::execute("sh", QStringList() << "-c"
                << "echo '{\"command\":[\"cycle\",\"pause\"]}' | socat - /tmp/mpv-socket");
        }

    } else if (g == "SWIPE_UP") {
        QProcess::execute("sh", QStringList() << "-c" << "amixer set Master 10%+");

    } else if (g == "SWIPE_DOWN") {
        QProcess::execute("sh", QStringList() << "-c" << "amixer set Master 10%-");

    } else if (g == "SWIPE_LEFT") {
        qDebug() << "왼쪽 스와이프 발생 (UI가 없으므로 시스템 기능 바인딩 가능)";

    } else if (g == "SWIPE_RIGHT") {
        qDebug() << "오른쪽 스와이프 발생";

    } else if (g == "END") {
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            qDebug() << "유튜브 음악 완전 재생 종료";
        }
    }
}
