#include "weatherpanel.h"
#include "ui_weatherpanel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QGraphicsDropShadowEffect>
#include <QDate>
#include <QDebug>
#include <QTime>
#include <QTextCodec>
#include <QSet>

WeatherPanel::WeatherPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WeatherPanel)
{
    ui->setupUi(this);

    // Network
    manager =
        new QNetworkAccessManager(this);

    forecastManager =
        new QNetworkAccessManager(this);

    connect(forecastManager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherPanel::onForecastReply);

    connect(manager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherPanel::onWeatherReply);

    midLandManager =
        new QNetworkAccessManager(this);

    connect(midLandManager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherPanel::onMidLandReply);

    midTempManager =
        new QNetworkAccessManager(this);

    connect(midTempManager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherPanel::onMidTempReply);

    // Panel Style
    this->setStyleSheet(
        "QWidget {"
        "background:rgba(30,30,30,220);"
        "border:2px solid white;"
        "border-radius:20px;"
        "color:white;"
        "}");

    // Table Size
    ui->tableForecast->setRowCount(3);
    ui->tableForecast->setColumnCount(8);
    ui->tableForecast->verticalHeader()
        ->setVisible(false);
    ui->tableForecast
        ->setColumnWidth(0, 70);

    // Current Weather Style
    ui->labelLocation->setStyleSheet(
        "font-size:24px;"
        "font-weight:bold;"
        "background:transparent;"
        "border:none;"
    );

    ui->labelIcon->setStyleSheet(
        "font-size:78px;"
        "background:transparent;"
        "border:none;"
    );

    ui->labelTemp->setStyleSheet(
        "font-size:58px;"
        "font-weight:bold;"
        "background:transparent;"
        "border:none;"
    );

    ui->labelWeather->setStyleSheet(
        "font-size:32px;"
        "background:transparent;"
        "border:none;"
    );

    ui->labelDetail->setStyleSheet(
        "font-size:18px;"
        "background:transparent;"
        "border:none;"
    );

    // Forecast Card
    ui->forecastCard->setStyleSheet(
        "background:rgba(255,255,255,0.08);"
        "border-radius:25px;"
        "border:none;"
    );

    QGraphicsDropShadowEffect *shadow =
        new QGraphicsDropShadowEffect(this);

    shadow->setBlurRadius(25);
    shadow->setOffset(0,5);
    shadow->setColor(
        QColor(0,0,0,80));
    ui->forecastCard
        ->setGraphicsEffect(shadow);

    // Table
    ui->tableForecast->setShowGrid(false);

    ui->tableForecast->setFrameShape(
        QFrame::NoFrame);

    ui->tableForecast->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    ui->tableForecast->setSelectionMode(
        QAbstractItemView::NoSelection);

    ui->tableForecast->setFocusPolicy(
        Qt::NoFocus);

    ui->tableForecast
        ->setVerticalScrollBarPolicy(
            Qt::ScrollBarAlwaysOff);

    ui->tableForecast
        ->setHorizontalScrollBarPolicy(
            Qt::ScrollBarAlwaysOff);

    // Header
    ui->tableForecast->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Interactive);

    ui->tableForecast->setColumnWidth(0, 90);

    for(int i=1; i<8; i++)
    {
        ui->tableForecast->horizontalHeader()
            ->setSectionResizeMode(
                i,
                QHeaderView::Stretch);
    }

    ui->tableForecast->horizontalHeader()
        ->setFixedHeight(32);

    ui->tableForecast->horizontalHeader()
        ->setDefaultAlignment(
            Qt::AlignLeft | Qt::AlignVCenter);

    ui->tableForecast->setHorizontalHeaderItem(
        0,
        new QTableWidgetItem(""));

    //--------------------------------
    // Table Style
    //--------------------------------

    ui->tableForecast->setStyleSheet(
        "QTableWidget {"
        "background:transparent;"
        "border:none;"
        "font-size:20px;"
        "}"
        "QTableWidget::item {"
        "border:none;"
        "padding:4px;"
        "}");

    ui->tableForecast->horizontalHeader()
        ->setStyleSheet(
            "QHeaderView::section {"
            "background:transparent;"
            "border:none;"
            "font-size:18px;"
            "font-weight:bold;"
            "padding-top:4px;"
            "}");

    // Row Height
    for(int i = 0; i < 3; i++)
    {
        ui->tableForecast
            ->setRowHeight(i, 36);
    }

    ui->tableForecast->setItem(
        0, 0,
        new QTableWidgetItem("날씨"));

    ui->tableForecast->setItem(
        1, 0,
        new QTableWidgetItem("최고"));

    ui->tableForecast->setItem(
        2, 0,
        new QTableWidgetItem("최저"));

    QFont titleFont;
    titleFont.setBold(true);

    ui->tableForecast->item(0,0)->setFont(titleFont);
    ui->tableForecast->item(1,0)->setFont(titleFont);
    ui->tableForecast->item(2,0)->setFont(titleFont);

    // Start
    requestWeather("Seoul");
    requestForecast();

    qDebug() << "requestMidLand start";
    requestMidLand();
    qDebug() << "requestMidTemp start";
    requestMidTemp();
}

WeatherPanel::~WeatherPanel()
{
    delete ui;
}

void WeatherPanel::requestWeather(QString city)
{
    Q_UNUSED(city);

    QString baseDate =
        QDate::currentDate().toString("yyyyMMdd");

    QTime now = QTime::currentTime();

    int hour = now.hour();

    QString baseTime =
        QString("%100")
            .arg(hour,2,10,QChar('0'));

    QString authKey =
        "umH6GTnpT9-h-hk56Z_fKA";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "VilageFcstInfoService_2.0/"
            "getUltraSrtNcst?"
            "pageNo=1&"
            "numOfRows=1000&"
            "dataType=JSON&"
            "base_date=%1&"
            "base_time=%2&"
            "nx=60&"
            "ny=127&"
            "authKey=%3")
            .arg(baseDate)
            .arg(baseTime)
            .arg(authKey);

    qDebug() << "요청 URL:" << url;

    manager->get(QNetworkRequest(QUrl(url)));
}

void WeatherPanel::requestForecast()
{
    QString authKey =
        "umH6GTnpT9-h-hk56Z_fKA";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ01/"
            "url/fct_afs_dl2.php?"
            "tmfc=0&"
            "disp=1&"
            "help=0&"
            "authKey=%1")
            .arg(authKey);

    qDebug() << "Forecast URL =" << url;

    forecastManager->get(
        QNetworkRequest(QUrl(url)));
}

void WeatherPanel::requestMidLand()
{
    QString tmFc =
        QDate::currentDate().toString("yyyyMMdd");

    if(QTime::currentTime().hour() >= 18)
        tmFc += "1800";
    else
        tmFc += "0600";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "MidFcstInfoService/getMidLandFcst?"
            "pageNo=1&numOfRows=10&dataType=JSON&"
            "regId=11B00000&"
            "tmFc=%1&"
            "authKey=umH6GTnpT9-h-hk56Z_fKA")
        .arg(tmFc);

    qDebug() << "MidLand URL =" << url;

    midLandManager->get(
        QNetworkRequest(QUrl(url)));
}

void WeatherPanel::requestMidTemp()
{
    QString tmFc =
        QDate::currentDate().toString("yyyyMMdd");

    if(QTime::currentTime().hour() >= 18)
        tmFc += "1800";
    else
        tmFc += "0600";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "MidFcstInfoService/getMidTa?"
            "pageNo=1&numOfRows=10&dataType=JSON&"
            "regId=11B10101&"
            "tmFc=%1&"
            "authKey=umH6GTnpT9-h-hk56Z_fKA")
        .arg(tmFc);

    qDebug() << "MidTemp URL =" << url;

    midTempManager->get(
        QNetworkRequest(QUrl(url)));
}

void WeatherPanel::onWeatherReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    qDebug() << data;

    QJsonDocument doc =
        QJsonDocument::fromJson(data);

    if(doc.isNull())
    {
        qDebug() << "JSON 파싱 실패";
        return;
    }

    QJsonObject root =
        doc.object();

    QJsonObject response =
        root["response"].toObject();

    QJsonObject body =
        response["body"].toObject();

    QJsonObject items =
        body["items"].toObject();

    QJsonArray itemArray =
        items["item"].toArray();
    qDebug() << "item 개수 =" << itemArray.size();

    double temp = 0;
    int humidity = 0;
    double rain = 0;
    double wind = 0;
    int pty = 0;

    for(const QJsonValue &v : itemArray)
    {
        QJsonObject obj =
            v.toObject();

        QString category =
            obj["category"].toString();

        QString value =
            obj["obsrValue"].toString();

        qDebug() << category << value;

        if(category == "T1H")
            temp = value.toDouble();

        else if(category == "REH")
            humidity = value.toInt();

        else if(category == "RN1")
            rain = value.toDouble();

        else if(category == "WSD")
            wind = value.toDouble();

        else if(category == "PTY")
            pty = value.toInt();
    }

    QString weatherText;
    QString icon;

    switch(pty)
    {
    case 0:
        weatherText = "맑음";
        icon = "☀";
        break;

    case 1:
        weatherText = "비";
        icon = "🌧";
        break;

    case 2:
        weatherText = "비/눈";
        icon = "🌨";
        break;

    case 3:
        weatherText = "눈";
        icon = "❄";
        break;

    default:
        weatherText = "흐림";
        icon = "☁";
        break;
    }

    ui->labelIcon->setText(icon);

    ui->labelTemp->setText(
        QString::number(temp,'f',1)
        + "°C");

    ui->labelWeather->setText(
        weatherText);

    ui->labelDetail->setText(
        QString("습도 %1%%   강수 %2mm   풍속 %3m/s")
            .arg(humidity)
            .arg(rain)
            .arg(wind));

    ui->labelLocation->setText(
        "현재 위치 : 서울");

    reply->deleteLater();
}

void WeatherPanel::onForecastReply(
    QNetworkReply *reply)
{
    QStringList weekNames =
    {
        "일","월","화","수","목","금","토"
    };

    QByteArray raw =
        reply->readAll();

    QString text =
        QString::fromLocal8Bit(raw);

    QStringList lines =
        text.split("\n");

    int col = 0;

    QSet<QString> addedDates;

    for(QString line : lines)
    {
        if(!line.startsWith("11A00101"))
            continue;

        QStringList cols =
            line.split(",");

        if(cols.size() < 18)
            continue;

        QString tm =
            cols[2].trimmed();

        QString dateKey =
            tm.left(8);

        if(addedDates.contains(dateKey))
            continue;

        addedDates.insert(dateKey);

        QString minTemp =
            cols[12].trimmed();

        QString maxTemp =
            cols[13].trimmed();

        QString weather =
            cols[17].trimmed();

        QString icon;

        if(weather.contains("맑"))
            icon = "☀";
        else if(weather.contains("구름"))
            icon = "⛅";
        else if(weather.contains("흐"))
            icon = "☁";
        else if(weather.contains("비"))
            icon = "🌧";
        else
            icon = "☁";

        QDate date =
            QDate::fromString(
                tm.left(8),
                "yyyyMMdd");

        QString dayName =
            weekNames[
                date.dayOfWeek() % 7
            ];

        ui->tableForecast
            ->setHorizontalHeaderItem(
                col + 1,
                new QTableWidgetItem("  " + dayName));

        ui->tableForecast
            ->setItem(
                0,
                col + 1,
                new QTableWidgetItem(icon));

        ui->tableForecast
            ->setItem(
                1,
                col + 1,
                new QTableWidgetItem(maxTemp + "°"));

        ui->tableForecast
            ->setItem(
                2,
                col + 1,
                new QTableWidgetItem(minTemp + "°"));

        col++;

        if(col >= 7)
            break;
    }
    reply->deleteLater();
}

QString WeatherPanel::koreanCityName(
    QString city)
{
    return city;
}

void WeatherPanel::setTextBrightness(int value)
{
    QString color =
        QString("rgb(%1,%1,%1)")
            .arg(value);

    this->setStyleSheet(
        QString(
            "QWidget {"
            "background:rgba(30,30,30,220);"
            "border:2px solid %1;"
            "border-radius:20px;"
            "color:%1;"
            "}").arg(color));

    QColor textColor(value, value, value);

    for(int row = 0; row < ui->tableForecast->rowCount(); row++)
    {
        for(int col = 0; col < ui->tableForecast->columnCount(); col++)
        {
            QTableWidgetItem *item =
                ui->tableForecast->item(row, col);

            if(!item)
                continue;

            // 날씨 아이콘 행
                   if(row == 0)
                   {
                       item->setForeground(
                           QColor(value,
                                  value,
                                  value));
                   }

                   // 최고 온도
                   else if(row == 1)
                   {
                       item->setForeground(
                           QColor(
                               qMin(value + 80, 255),
                               80,
                               80));
                   }

                   // 최저 온도
                   else if(row == 2)
                   {
                       item->setForeground(
                           QColor(
                               80,
                               160,
                               qMin(value + 80, 255)));
                   }
        }
    }

    for(int col = 0; col < ui->tableForecast->columnCount(); col++)
    {
        QTableWidgetItem *header =
            ui->tableForecast->horizontalHeaderItem(col);

        if(header)
            header->setForeground(textColor);
    }
}

void WeatherPanel::onMidLandReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    qDebug() << "MidLand Data =" << data;

    QJsonDocument doc =
        QJsonDocument::fromJson(data);

    if(doc.isNull())
    {
        qDebug() << "MidLand JSON Parse Error";
        reply->deleteLater();
        return;
    }

    midLandData =
        doc.object()["response"]
            .toObject()["body"]
            .toObject()["items"]
            .toObject()["item"]
            .toArray()[0]
            .toObject();

    qDebug() << "MidLand Object =" << midLandData;

    fillMidForecast();

    reply->deleteLater();
}

void WeatherPanel::onMidTempReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    qDebug() << "MidTemp Data =" << data;

    QJsonDocument doc =
        QJsonDocument::fromJson(data);

    if(doc.isNull())
    {
        qDebug() << "MidTemp JSON Parse Error";
        reply->deleteLater();
        return;
    }

    midTempData =
        doc.object()["response"]
            .toObject()["body"]
            .toObject()["items"]
            .toObject()["item"]
            .toArray()[0]
            .toObject();

    qDebug() << "MidTemp Object =" << midTempData;

    fillMidForecast();

    reply->deleteLater();
}

void WeatherPanel::fillMidForecast()
{
    qDebug() << "fillMidForecast called";
    qDebug() << "midLandData empty =" << midLandData.isEmpty();
    qDebug() << "midTempData empty =" << midTempData.isEmpty();

    if(midLandData.isEmpty())
        return;

    if(midTempData.isEmpty())
        return;

    QStringList weekNames =
    {
        "일","월","화","수","목","금","토"
    };

    for(int day=4; day<=6; day++)
    {
        int col = day+1;

        QDate target =
            QDate::currentDate().addDays(day);

        QString dayName =
            weekNames[
                target.dayOfWeek()%7];

        QString weather =
            midLandData[
                QString("wf%1Am").arg(day)
            ].toString();

        QString icon = "☁";

        if(weather.contains("맑"))
            icon="☀";
        else if(weather.contains("구름"))
            icon="⛅";
        else if(weather.contains("비"))
            icon="🌧";

        int maxTemp =
            midTempData[
                QString("taMax%1").arg(day)
            ].toInt();

        int minTemp =
            midTempData[
                QString("taMin%1").arg(day)
            ].toInt();

        ui->tableForecast->setHorizontalHeaderItem(
            col,
            new QTableWidgetItem(dayName));

        ui->tableForecast->setItem(
            0,col,
            new QTableWidgetItem(icon));

        ui->tableForecast->setItem(
            1,col,
            new QTableWidgetItem(
                QString("%1°").arg(maxTemp)));

        ui->tableForecast->setItem(
            2,col,
            new QTableWidgetItem(
                QString("%1°").arg(minTemp)));
    }
}
