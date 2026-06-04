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

const quint16 ARDUINO_PORT = 9000;
const quint16 GESTURE_PORT = 9001;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), tcpSocket(nullptr), tcpSocket9001(nullptr),emotionProcess(nullptr)
{
   ui->setupUi(this);

   this->resize(1920, 1080);

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

   ui->lcdNumberTemp->setSegmentStyle(
       QLCDNumber::Filled);

   ui->lcdNumberHumi->setSegmentStyle(
       QLCDNumber::Filled);

   ui->lcdNumberTemp->setStyleSheet(
       "background:#111;");

   ui->lcdNumberHumi->setStyleSheet(
       "background:#111;");

   // ── 화면 ON/OFF 오버레이 (맨 위) ──────────
   blackOverlay = new QFrame(ui->centralWidget);
   blackOverlay->setGeometry(ui->centralWidget->rect());
   blackOverlay->setStyleSheet("background-color:black;");
   blackOverlay->raise();
   blackOverlay->show(); // 시작할 때는 화면 켜진 상태
   //blackOverlay->hide();//테스트

   // weather panel(초기위치)
   WeatherWidget = new WeatherPanel(ui->centralWidget);
   WeatherWidget->setGeometry(
       1100,
       1200,
       800,
       500);
   WeatherWidget->hide();
   QTimer::singleShot(
       1000,
       this,
       [=]()
       {
          WeatherWidget->move(1080, 460);
          WeatherWidget->show();
          blackOverlay->raise();
          //blackOverlay->hide(); // 테스트1
          //showNewsPanel();      // 테스트2
       });

   // gesture detected
   ytDlpProcess = new QProcess(this);
   mpvProcess = new QProcess(this);

   // yt-dlp 완료 시 mpv 실행
   connect(ytDlpProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
           this, [this](int exitCode, QProcess::ExitStatus)
           {
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
            << url); });
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

   newsWidget =
       new NewsPanel(ui->centralWidget);

   newsWidget->setGeometry(
       1920,
       430,
       760,
       520);

   newsWidget->hide();
}

MainWindow::~MainWindow()
{
   // 프로그램 종료 시 파이썬이 실행 중이라면 함께 종료 처리
   if (emotionProcess && emotionProcess->state() != QProcess::NotRunning) {
      emotionProcess->kill();
      emotionProcess->waitForFinished(1000);
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
    // 0~550 범위를 80~255로 변환
    int brightness = qBound(80, (briVal * 255) / 550, 255);

    QString color =
        QString("rgb(%1,%1,%1)")
            .arg(brightness);

    // 시계/날짜
    ui->dateLabel->setStyleSheet(
        QString(
            "color:%1;"
            "font-size:12pt;"
            "font-weight:400;"
            "background:none;")
            .arg(color));

    ui->timeLabel->setStyleSheet(
        QString(
            "color:%1;"
            "font-size:45pt;"
            "font-weight:bold;"
            "margin-top:-5px;"
            "background:none;")
            .arg(color));

    // AQI
    ui->labelAQI->setStyleSheet(
        QString(
            "color:%1;"
            "font-size:20pt;"
            "background:none;"
            "font-family:'Noto Color Emoji';")
            .arg(color));

    // 온도/습도 LCD
    QColor lcdColor(
        brightness,
        brightness,
        brightness);

    QPalette tempPalette =
        ui->lcdNumberTemp->palette();

    tempPalette.setColor(
        QPalette::Light,
        lcdColor);

    tempPalette.setColor(
        QPalette::Dark,
        lcdColor.darker());

    ui->lcdNumberTemp->setPalette(
        tempPalette);

    QPalette humiPalette =
        ui->lcdNumberHumi->palette();

    humiPalette.setColor(
        QPalette::Light,
        lcdColor);

    humiPalette.setColor(
        QPalette::Dark,
        lcdColor.darker());

    ui->lcdNumberHumi->setPalette(
        humiPalette);

    WeatherWidget->setTextBrightness(brightness);
    newsWidget->setTextBrightness(brightness);
}

// ── 데이터 파싱 및 UI 업데이트 ──────────────────
void MainWindow::processData(const QString &data)
{
   // ON / OFF 처리
   if (data == "OFF")
   {
      blackOverlay->show();
      blackOverlay->raise();
      waitingData = false;
      return;
   }
   if (data == "ON")
   {
      waitingData = true;
      // [수정] ON 수신 시 중복 실행 상태가 아니라면 파이썬 스크립트를 비동기로 호출합니다.
      if (emotionProcess && emotionProcess->state() == QProcess::NotRunning) {
          emotionProcess->setWorkingDirectory("/home/jt-user/SmartMirror/opencv");
         QString pythonExecutable = "/home/jt-user/deepface_env/bin/python3"; // 윈도우 환경 환경인 경우 "python"으로 변경 가능
         QStringList arguments;

         // ⚠️ 실행하고자 하는 파이썬 스크립트의 '절대 경로'를 정확하게 입력해 주세요.
         arguments << "opencv1.py";

         emotionProcess->start(pythonExecutable, arguments);//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
         qDebug() << "아두이노 ON: 파이썬 감정 분석 스크립트를 시작합니다.";
      }
      return;
   }
   if (data == "SHOW_NEWS") //나중에 수정
   {
      showNewsPanel();
      return;
   }
   if (data == "SHOW_WEATHER")  //여기도 나중에 수정
   {
      showWeatherPanel();
      return;
   }

   // BRI 단독 처리processData
   if (data.startsWith("BRI:"))
   {
      int briVal = data.section("BRI:", 1, 1).trimmed().toInt();
      applyBrightness(briVal);
      return;
   }

   // 온습도 + AQI + BRI 풀 패킷 처리
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

      // AQI 이모지
      QString emoji;
      switch (aqiVal)
      {
      case 1:
         emoji = "좋음";
         break;
      case 2:
         emoji = "보통";
         break;
      case 3:
         emoji = "나쁨";
         break;
      case 4:
         emoji = "위험";
         break;
      case 5:
         emoji = "죽음";
         break;
      default:
         emoji = "오류";
         break;
      }
      ui->labelAQI->setText(emoji);

      // BRI가 포함된 경우 밝기도 반영
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
   /*if (overlayWidget)
      overlayWidget->setGeometry(ui->centralWidget->rect());*/
}

// ── 제스처 수신 처리 ────────────────────────────
void MainWindow::gestureDetected(const QString &gesture)
{
   qDebug() << "제스처 수신:" << gesture;

   if (gesture == "START")
   {
      // 이미 재생 중이면 기존 프로세스 종료
      if (mpvProcess->state() != QProcess::NotRunning)
      {
         mpvProcess->kill();
         mpvProcess->waitForFinished(1000);
      }
      if (ytDlpProcess->state() != QProcess::NotRunning)
      {
         ytDlpProcess->kill();
         ytDlpProcess->waitForFinished(1000);
      }

      // yt-dlp로 첫 번째 영상 URL 추출 (비동기)
      ytDlpProcess->start("yt-dlp", QStringList()
                                        << "ytsearch1:기분 좋을 때 듣는 노래"
                                        << "--get-url"
                                        << "--format" << "bestaudio");
   }
   else if (gesture == "STOP")
   {
      // mpv IPC 소켓으로 pause 토글
      if (mpvProcess->state() == QProcess::Running)
      {
         QProcess::execute("sh", QStringList() << "-c"
                                               << "echo '{\"command\":[\"cycle\",\"pause\"]}' | socat - /tmp/mpv-socket");
      }
   }
   else if (gesture == "END")
   {
      if (mpvProcess->state() != QProcess::NotRunning)
      {
         mpvProcess->kill();
         qDebug() << "재생 종료";
      }
   }
}

// Weather Panel 보이기
void MainWindow::showWeatherPanel()
{
   WeatherWidget->show();

   QPropertyAnimation *newsAnim =
       new QPropertyAnimation(
           newsWidget,
           "pos");

   newsAnim->setDuration(700);

   newsAnim->setStartValue(
       newsWidget->pos());

   newsAnim->setEndValue(
       QPoint(1920, 150));

   QPropertyAnimation *weatherAnim =
       new QPropertyAnimation(
           WeatherWidget,
           "pos");

   weatherAnim->setDuration(700);

   weatherAnim->setStartValue(
       WeatherWidget->pos());

   weatherAnim->setEndValue(
       QPoint(1080, 460));

   newsAnim->start(
       QAbstractAnimation::DeleteWhenStopped);

   weatherAnim->start(
       QAbstractAnimation::DeleteWhenStopped);

   connect(newsAnim, &QPropertyAnimation::finished,
           newsWidget, &QWidget::hide);
}

// News Panel 보이기
void MainWindow::showNewsPanel()
{
   newsWidget->show();

   QPropertyAnimation *weatherAnim =
       new QPropertyAnimation(
           WeatherWidget,
           "pos");

   weatherAnim->setDuration(700);

   weatherAnim->setStartValue(
       WeatherWidget->pos());

   weatherAnim->setEndValue(
       QPoint(-900, 460));

   QPropertyAnimation *newsAnim =
       new QPropertyAnimation(
           newsWidget,
           "pos");

   newsAnim->setDuration(700);

   newsAnim->setStartValue(
       newsWidget->pos());

   newsAnim->setEndValue(
       QPoint(1180, 580));

   weatherAnim->start(
       QAbstractAnimation::DeleteWhenStopped);

   newsAnim->start(
       QAbstractAnimation::DeleteWhenStopped);

   connect(weatherAnim, &QPropertyAnimation::finished,
           WeatherWidget, &QWidget::hide);
}
