#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDateTime>
#include <QLocale>
#include <QNetworkDatagram>
#include <QDebug>
#include <QFrame>
#include <QSlider>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->move(930, 110);

    // 1. UDP 소켓 생성 및 9001번 포트 바인딩
    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::LocalHost, 9001);
     // 2. 데이터가 도착하면 readPendingDatagrams 함수 실행
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::readPendingDatagrams);

    //날짜
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);

    updateTime();

    //css
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

    // 검은 화면 오버레이
    blackOverlay = new QFrame(this);
    blackOverlay->setGeometry(rect());
    blackOverlay->setStyleSheet("background-color:black;");
    blackOverlay->show();
    blackOverlay->raise();

    overlayWidget = new QWidget(this);

    overlayWidget->setGeometry(rect());
    overlayWidget->raise();

    overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlayWidget->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QLocale koLocale(QLocale::Korean, QLocale::SouthKorea);

    ui->dateLabel->setText(koLocale.toString(current, "M월 d일 dddd"));
    ui->timeLabel->setText(current.toString("h:mm"));

    this->setStyleSheet("background-color: #333;");
}

void MainWindow::readPendingDatagrams() {
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QString data = QString::fromUtf8(datagram.data()).trimmed();

        qDebug() << "Raw Data:" << data;

        if (data == "OFF") {
                blackOverlay->show();
                blackOverlay->raise();
               }
        else if (data == "ON") {
                blackOverlay->hide();
               }

        // 3. 기존 파싱 로직을 Qt 스타일로 적용
        if (data.contains("TEMP:") && data.contains("HUMI:")) {
            QString tempStr = data.section("TEMP:", 1, 1).section(",", 0, 0);
            QString humiStr = data.section("HUMI:", 1, 1).section(",", 0, 0);
            QString aqiStr = data.section("AQI:", 1, 1).section(",", 0, 0);
            QString briStr = data.section("BRI:",1,1);

            double tempVal=tempStr.toDouble();
            double humiVal=humiStr.toDouble();
            int aqiVal=aqiStr.toInt();
            int briVal=briStr.toInt();

            // UI의 LCD 위젯 업데이트
            ui->lcdNumberTemp->display(QString::number(tempVal,'f',1));
            ui->lcdNumberHumi->display(QString::number(humiVal,'f',1));

            QString emoji;

            switch(aqiVal)
            {
            case 0:
                emoji = "오류입니다!!!";
                break;
            case 1:
                emoji = "🙂";
                break;
            case 2:
                emoji = "😐";
                break;
            case 3:
                emoji = "😷";
                break;
            case 4:
                emoji = "☠️";
                break;
            case 5:
                emoji = "😀";
                break;
            }

            ui->labelAQI->setText(emoji);

            if(briVal>=0&&briVal<=200){
                overlayWidget->show();
                int alpha = ((200 - briVal) * 180) / 200;

                overlayWidget->setStyleSheet(
                        QString("background-color: rgba(0,0,0,%1);")
                            .arg(alpha));
            }
            else if(briVal>=201&&briVal<=350){
                overlayWidget->hide();
            }
            else{
                overlayWidget->show();

                int alpha = ((briVal - 350) * 180) / 200;

                alpha = qBound(0, alpha, 180);

                overlayWidget->setStyleSheet(
                        QString("background-color: rgba(255,255,255,%1);")
                            .arg(alpha));
            }
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (blackOverlay)
        blackOverlay->setGeometry(rect());
    if (overlayWidget)
            overlayWidget->setGeometry(rect());
}
