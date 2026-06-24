#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QProcess>

// ── 리눅스 표준 네트워크 및 소켓 헤더 (UDP 수신용) ──
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

const quint16 ARDUINO_PORT = 9000;
const int     QT_UDP_PORT  = 9001;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpSocket(nullptr)
    , _qtUdpSock(-1) // 파이썬이 보낸 데이터를 받을 소켓
{
    ui->setupUi(this);
    
    // UI 화면(시계, 아두이노 LCD 등)을 원본 그대로 정상적으로 표시합니다.
    this->setStyleSheet("background-color:#333;");
    // ... (기존 시계, 라벨, LCD 스타일시트 설정 코드들 원본 그대로 유지) ...

    // ── 아두이노 TCP 서버 시작 ──────────────────────────
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);
    if (!tcpServer->listen(QHostAddress::Any, ARDUINO_PORT)) {
        qWarning() << "아두이노 TCP 서버 시작 실패:" << tcpServer->errorString();
    }

    // ── 미디어 재생 프로세스 초기화 ─────────────────────
    ytDlpProcess = new QProcess(this);
    mpvProcess   = new QProcess(this);

    // yt-dlp 완료 시 mpv 실행 루틴 (원본 유지)
    connect(ytDlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) return;
        QString url = QString::fromUtf8(ytDlpProcess->readAllStandardOutput()).trimmed();
        if (url.isEmpty()) return;
        
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            mpvProcess->waitForFinished(1000);
        }
        mpvProcess->start("mpv", QStringList() << "--no-video" << "--input-ipc-server=/tmp/mpv-socket" << url);
    });

    // ── 내부 연동용 UDP 수신 소켓 생성 및 Qt 이벤트 바인딩 ──
    initUdpSocket();
}

MainWindow::~MainWindow()
{
    if (_qtUdpSock != -1) {
        ::close(_qtUdpSock);
    }
    delete ui;
}

// ============================================================
//  파이썬이 보내는 UDP 데이터를 감지하기 위한 리눅스 소켓 설정
// ============================================================
void MainWindow::initUdpSocket()
{
    _qtUdpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (_qtUdpSock < 0) {
        qWarning() << "C++ UDP 수신 소켓 생성 실패";
        return;
    }

    // 포트 재사용 설정
    int opt = 1;
    setsockopt(_qtUdpSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    _qtAddr.sin_family      = AF_INET;
    _qtAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 로컬 전체 수신
    _qtAddr.sin_port        = htons(QT_UDP_PORT);

    if (bind(_qtUdpSock, (struct sockaddr*)&_qtAddr, sizeof(_qtAddr)) < 0) {
        qWarning() << "C++ UDP 바인딩 실패 (9001포트 충돌 확인 필요)";
        return;
    }

    // Qt 시스템에서 UDP 소켓에 데이터가 들어오면 자동으로 이벤트 슬롯을 깨우도록 연동
    QSocketNotifier* notifier = new QSocketNotifier(_qtUdpSock, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this]() {
        char buf[256] = {0,};
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        
        int len = recvfrom(_qtUdpSock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&clientAddr, &addrLen);
        if (len > 0) {
            QString msg = QString::fromUtf8(buf, len).trimmed();
            // 수신된 데이터를 제스처 처리기 함수로 토스합니다.
            this->gestureDetected(msg);
        }
    });
    
    qDebug() << "파이썬 연동용 9001번 UDP 수신 포트 활성화 완료.";
}

// ... 중략 (onNewConnection, onDataReceived, processData 등 아두이노 TCP 통신부 원본 그대로 유지) ...

// ============================================================
//  파이썬 AI 엔진으로부터 전달받은 제스처 및 감정 데이터 처리
// ============================================================
void MainWindow::gestureDetected(const QString &gesture)
{
    qDebug() << "[파이썬 수신 데이터]:" << gesture;

    // 1. 파이썬 DeepFace 분석 결과가 들어온 경우 ("EMOTION_RESULT:happy" 등)
    if (gesture.startsWith("EMOTION_RESULT:")) {
        QString emotion = gesture.section("EMOTION_RESULT:", 1, 1).trimmed();
        qDebug() << "🎯 파이썬이 판별한 주인의 표정:" << emotion;

        QString searchKeyword = "기분 좋을 때 듣는 노래"; // 기본값
        if (emotion == "happy") searchKeyword = "신나고 기분 좋은 팝송 플레이리스트";
        else if (emotion == "sad") searchKeyword = "위로가 되는 잔잔한 발라드 노래";
        else if (emotion == "angry") searchKeyword = "마음이 편안해지는 클래식 명곡";
        else if (emotion == "neutral") searchKeyword = "집중할 때 듣기 좋은 로파이 비트";

        // 음원 주소 추출 및 미디어 재생 시작
        if (mpvProcess->state() != QProcess::NotRunning) mpvProcess->kill();
        if (ytDlpProcess->state() != QProcess::NotRunning) ytDlpProcess->kill();
        
        ytDlpProcess->start("yt-dlp", QStringList()
            << QString("ytsearch1:%1").arg(searchKeyword)
            << "--get-url" << "--format" << "bestaudio");
        return;
    }

    // 2. 일반 손동작 제스처 명령이 들어온 경우 ("GESTURE:STOP" 등)
    QString g = gesture;
    if (g.startsWith("GESTURE:")) g = g.mid(8);

    if (g == "START") {
        // 손을 펼치면 파이썬 내부에서 DeepFace 연산이 자동으로 도므로 
        // C++은 결과값("EMOTION_RESULT:")이 UDP로 올 때까지 대기하면 됩니다.
        qDebug() << "손 펼침 감지 -> 파이썬의 DeepFace 표정 분석 결과를 대기합니다.";

    } else if (g == "STOP") {
        if (mpvProcess->state() == QProcess::Running) {
            QProcess::execute("sh", QStringList() << "-c" << "echo '{\"command\":[\"cycle\",\"pause\"]}' | socat - /tmp/mpv-socket");
        }
    } else if (g == "SWIPE_UP") {
        QProcess::execute("sh", QStringList() << "-c" << "amixer set Master 10%+");
    } else if (g == "SWIPE_DOWN") {
        QProcess::execute("sh", QStringList() << "-c" << "amixer set Master 10%-");
    } else if (g == "END") {
        if (mpvProcess->state() != QProcess::NotRunning) mpvProcess->kill();
    }
}
