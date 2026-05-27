#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QHeaderView>
#include <QTableWidgetItem>

#include <QGraphicsDropShadowEffect>

#include <QDate>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //--------------------------------
    // Table Size
    //--------------------------------

    ui->tableForecast->setRowCount(3);
    ui->tableForecast->setColumnCount(7);

    //--------------------------------
    // Network
    //--------------------------------

    manager =
        new QNetworkAccessManager(this);

    connect(manager,
            &QNetworkAccessManager::finished,
            this,
            &MainWindow::onWeatherReply);

    //--------------------------------
    // Window
    //--------------------------------

    this->setStyleSheet(
        "QMainWindow {"
        "background:qlineargradient("
        "x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #87CEEB,"
        "stop:1 #EAF4FF);"
        "}");

    //--------------------------------
    // Current Weather
    //--------------------------------

    ui->labelLocation->setStyleSheet(
        "font-size:18px;"
        "font-weight:bold;"
        "color:#333;");

    ui->labelIcon->setStyleSheet(
        "font-size:48px;");

    ui->labelTemp->setStyleSheet(
        "font-size:42px;"
        "font-weight:bold;"
        "color:#111;");

    ui->labelWeather->setStyleSheet(
        "font-size:22px;"
        "color:#222;");

    ui->labelDetail->setStyleSheet(
        "font-size:15px;"
        "color:#333;");

    //--------------------------------
    // Forecast Card
    //--------------------------------

    ui->forecastCard->setStyleSheet(
        "background:rgba(255,255,255,0.65);"
        "border-radius:25px;");

    QGraphicsDropShadowEffect *shadow =
        new QGraphicsDropShadowEffect(this);

    shadow->setBlurRadius(25);

    shadow->setOffset(0,5);

    shadow->setColor(
        QColor(0,0,0,40));

    ui->forecastCard
        ->setGraphicsEffect(shadow);

    //--------------------------------
    // Table
    //--------------------------------

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

    //--------------------------------
    // Header
    //--------------------------------

    ui->tableForecast->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch);

    ui->tableForecast->horizontalHeader()
        ->setFixedHeight(28);

    ui->tableForecast->verticalHeader()
        ->setDefaultSectionSize(40);

    //--------------------------------
    // Style
    //--------------------------------

    ui->tableForecast->setStyleSheet(
        "QTableWidget {"
        "background:transparent;"
        "border:none;"
        "font-size:16px;"
        "color:#222;"
        "}"
        "QTableWidget::item {"
        "border:none;"
        "padding:4px;"
        "}");

    ui->tableForecast->horizontalHeader()
                            ->setFixedHeight(28);

    ui->tableForecast->horizontalHeader()
        ->setStyleSheet(

            "QHeaderView::section {"
            "background:transparent;"
            "border:none;"
            "font-size:16px;"
            "font-weight:bold;"
            "color:#333;"
            "}");

    ui->tableForecast->verticalHeader()
        ->setStyleSheet(
            "QHeaderView::section {"
            "background:transparent;"
            "border:none;"
            "font-size:15px;"
            "font-weight:bold;"
            "color:#444;"
            "}");

    //--------------------------------
    // Row Height
    //--------------------------------

    for(int i = 0; i < 3; i++)
    {
        ui->tableForecast
            ->setRowHeight(i, 30);
    }

    //--------------------------------
    // Row Labels
    //--------------------------------

    QStringList rowLabels;

    rowLabels << "날씨"
              << "최고"
              << "최저";

    ui->tableForecast
        ->setVerticalHeaderLabels(
            rowLabels);

    //--------------------------------
    // Start City
    //--------------------------------

    requestWeather("Seoul");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::requestWeather(
    QString city)
{
    double lat = 37.5665;
    double lon = 126.9780;

    //--------------------------------
    // City Coordinates
    //--------------------------------

    if(city == "Busan")
    {
        lat = 35.1796;
        lon = 129.0756;
    }
    else if(city == "Tokyo")
    {
        lat = 35.6762;
        lon = 139.6503;
    }
    else if(city == "London")
    {
        lat = 51.5072;
        lon = -0.1276;
    }

    //--------------------------------
    // Open-Meteo URL
    //--------------------------------

    QString urlString =
        QString(
            "https://api.open-meteo.com/v1/forecast"
            "?latitude=%1"
            "&longitude=%2"
            "&daily=weathercode,"
            "temperature_2m_max,"
            "temperature_2m_min"
            "&current=temperature_2m,"
            "apparent_temperature,"
            "relative_humidity_2m,"
            "wind_speed_10m,"
            "weather_code"
            "&timezone=Asia/Seoul"
            "&forecast_days=7")
            .arg(lat)
            .arg(lon);

    QUrl url(urlString);

    QNetworkRequest request(url);

    manager->get(request);
}

void MainWindow::onWeatherReply(
    QNetworkReply *reply)
{
    QByteArray data =
        reply->readAll();

    QJsonDocument doc =
        QJsonDocument::fromJson(data);

    if(doc.isNull())
        return;

    QJsonObject obj =
        doc.object();

    //--------------------------------
    // Current Weather
    //--------------------------------

    QJsonObject current =
        obj["current"].toObject();

    double temp =
        current["temperature_2m"].toDouble();

    double feelsLike =
        current["apparent_temperature"].toDouble();

    int humidity =
        current["relative_humidity_2m"].toInt();

    double wind =
        current["wind_speed_10m"].toDouble();

    int weatherCode =
        current["weather_code"].toInt();

    QString weatherText = "맑음";
    QString icon = "☀";

    if(weatherCode <= 3)
    {
        weatherText = "흐림";
        icon = "☁";
    }
    else if(weatherCode >= 51 &&
            weatherCode <= 67)
    {
        weatherText = "비";
        icon = "🌧";
    }
    else if(weatherCode >= 71)
    {
        weatherText = "눈";
        icon = "❄";
    }

    ui->labelIcon->setText(icon);

    ui->labelTemp->setText(
        QString::number(temp)
        + "°C");

    ui->labelWeather->setText(
        weatherText);

    ui->labelDetail->setText(
                QString(
                    "체감 %1°   습도 %2%%   풍속 %3 km/h")
                    .arg(feelsLike)
                    .arg(humidity)
                    .arg(wind)
                );

    //--------------------------------
    // Location
    //--------------------------------

    ui->labelLocation->setText(
        "현재 위치 : 서울");

    //--------------------------------
    // Forecast
    //--------------------------------

    QJsonObject daily =
        obj["daily"].toObject();

    QJsonArray times =
        daily["time"].toArray();

    QJsonArray weathercodes =
        daily["weathercode"].toArray();

    QJsonArray maxTemps =
        daily["temperature_2m_max"].toArray();

    QJsonArray minTemps =
        daily["temperature_2m_min"].toArray();

    int days = times.size();

    ui->tableForecast->setColumnCount(days);

    QStringList week =
    {
        "일","월","화","수",
        "목","금","토"
    };

    for(int i = 0; i < days; i++)
    {
        //--------------------------------
        // Date
        //--------------------------------

        QString date =
            times[i].toString();

        QDate qdate =
            QDate::fromString(
                date,
                "yyyy-MM-dd");

        QString dayName =
            week[qdate.dayOfWeek() % 7];

        QTableWidgetItem *headerItem =
            new QTableWidgetItem(dayName);

        headerItem->setTextAlignment(
            Qt::AlignCenter);

        headerItem->setForeground(
            QColor("#333"));

        ui->tableForecast
            ->setHorizontalHeaderItem(
                i,
                headerItem);

        //--------------------------------
        // Weather Code
        //--------------------------------

        int code =
            weathercodes[i].toInt();

        QString dayIcon = "☀";

        if(code <= 3)
            dayIcon = "☁";

        else if(code >= 51 &&
                code <= 67)
            dayIcon = "🌧";

        else if(code >= 71)
            dayIcon = "❄";

        //--------------------------------
        // Temperatures
        //--------------------------------

        double maxTemp =
            maxTemps[i].toDouble();

        double minTemp =
            minTemps[i].toDouble();

        //--------------------------------
        // Icon Item
        //--------------------------------

        QTableWidgetItem *iconItem =
            new QTableWidgetItem(dayIcon);

        iconItem->setTextAlignment(
            Qt::AlignCenter);

        ui->tableForecast->setItem(
            0,
            i,
            iconItem);

        //--------------------------------
        // Max Temp
        //--------------------------------

        QTableWidgetItem *maxItem =
            new QTableWidgetItem(
                QString::number(maxTemp)
                + "°");

        maxItem->setTextAlignment(
            Qt::AlignCenter);

        maxItem->setForeground(
            QColor("#E53935"));

        ui->tableForecast->setItem(
            1,
            i,
            maxItem);

        //--------------------------------
        // Min Temp
        //--------------------------------

        QTableWidgetItem *minItem =
            new QTableWidgetItem(
                QString::number(minTemp)
                + "°");

        minItem->setTextAlignment(
            Qt::AlignCenter);

        minItem->setForeground(
            QColor("#1E88E5"));

        ui->tableForecast->setItem(
            2,
            i,
            minItem);
    }

    reply->deleteLater();
}

QString MainWindow::koreanCityName(
    QString city)
{
    if(city == "Seoul")
        return "서울";

    if(city == "Busan")
        return "부산";

    if(city == "Tokyo")
        return "도쿄";

    if(city == "London")
        return "런던";

    return city;
}
