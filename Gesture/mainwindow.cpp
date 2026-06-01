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

    // [백그라운드 구동] 스마트 미러 UI 창을 화면에서 숨깁니다.
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

    // ── 미디어 및 파이썬 프로세스 초기화 ─────────────────────
    ytDlpProcess = new QProcess(this);
    mpvProcess   = new QProcess(this);
    faceProcess  = new QProcess(this); // 👈 파이썬 실행용 프로세스

    // ── [DeepFace 파이썬(test.py) 분석 완료 시그널 처리] ──
    connect(faceProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) { 
            qWarning() << "test.py 파이썬 스크립트 실행 실패"; 
            return; 
        }
        
        // 파이썬이 출력한 표준 출력(stdout) 결과 읽기
        QString output = QString::fromUtf8(faceProcess->readAllStandardOutput()).trimmed();
        qDebug() << "[DeepFace 출력 스캔]:" << output;

        // 감정 결과 태그가 포함되어 있는지 확인
        if (output.contains("EMOTION_RESULT:")) {
            // "EMOTION_RESULT:happy" 형태에서 감정 키워드 문자열만 파싱
            QString emotion = output.section("EMOTION_RESULT:", 1, 1).trimmed();
            qDebug() << "🎯 분석된 사용자의 감정 상태:" << emotion;

            // 감정에 따른 유튜브 검색 키워드 매칭 법칙
            QString searchKeyword = "기분 좋을 때 듣는 노래"; // 기본값
            
            if (emotion == "happy") {
                searchKeyword = "신나고 기분 좋은 팝송 플레이리스트";
                qDebug() << "[감정 선곡] 기쁨(happy) 감지 -> 신나는 음악을 재생합니다.";
            } 
            else if (emotion == "sad") {
                searchKeyword = "위로가 되는 잔잔한 발라드 노래";
                qDebug() << "[감정 선곡] 슬픔(sad) 감지 -> 마음을 달래줄 잔잔한 노래를 재생합니다.";
            } 
            else if (emotion == "angry") {
                searchKeyword = "마음이 편안해지는 클래식 명곡";
                qDebug() << "[감정 선곡] 화남(angry) 감지 -> 차분해질 수 있는 클래식을 재생합니다.";
            } 
            else if (emotion == "neutral") {
                searchKeyword = "집중할 때 듣기 좋은 로파이(Lo-fi) 비트";
                qDebug() << "[감정 선곡] 평온(neutral) 감지 -> 일상 배경 음악을 재생합니다.";
            } 
            else {
                searchKeyword = "최신 가요 탑 100 플레이리스트";
            }

            // 기존 재생 중이던 미디어 프로세스 안전 종료
            if (mpvProcess->state() != QProcess::NotRunning) mpvProcess->kill();
            if (ytDlpProcess->state() != QProcess::NotRunning) ytDlpProcess->kill();
            
            // 파싱된 감정 키워드로 유튜브 음원 추출 시작
            ytDlpProcess->start("yt-dlp", QStringList()
                << QString("ytsearch1:%1").arg(searchKeyword)
                << "--get-url" << "--format" << "bestaudio");
                
        } else {
            qWarning() << "❌ 파이썬으로부터 유효한 감정 분석 데이터를 읽지 못했습니다.";
        }
    });

    // yt-dlp 완료 시 mpv 실행 루틴 (기존과 동일)
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
        processData(data);
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
    if (data == "ON" || data == "OFF") {
        qDebug() << "[아두이노 명령 상태 변경]:" << data;
    } else if (data.startsWith("TEMP:")) {
        qDebug() << "[센서 패킷 정상 수신 확인]:" << data;
    }
}

// ============================================================
//  제스처 감지 시 스레드 제어 슬롯 (test.py 파이썬 스크립트 연동)
// ============================================================
void MainWindow::gestureDetected(const QString &gesture)
{
    QString g = gesture;
    if (g.startsWith("GESTURE:")) g = g.mid(8);

    qDebug() << "[제스처 감지 백그라운드 트리거]:" << g;

    // 1. 음악 재생 시작 제스처가 오면 곧바로 재생하지 않고, test.py를 구동하여 표정을 분석합니다.
    if (g == "START" || g == "HAND_OPEN") {
        if (faceProcess->state() == QProcess::Running) {
            qDebug() << "이미 파이썬 감정 분석 스크립트(test.py)가 실행 중입니다.";
            return;
        }
        
        qDebug() << "📢 제스처 확인됨 -> DeepFace 표정 분석 스크립트(test.py) 가동 시작...";
        // python3 test.py 비동기 백그라운드 호출
        faceProcess->start("python3", QStringList() << "test.py");

    // 2. 일시정지, 볼륨 제어 기능 (원본 유지)
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
        qDebug() << "왼쪽 스와이프 발생 (로그 출력)";

    } else if (g == "SWIPE_RIGHT") {
        qDebug() << "오른쪽 스와이프 발생 (로그 출력)";

    } else if (g == "END") {
        if (mpvProcess->state() != QProcess::NotRunning) {
            mpvProcess->kill();
            qDebug() << "유튜브 음악 완전 재생 종료";
        }
    }
}
