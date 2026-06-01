#include "weatherpanel.h"
#include "ui_weatherpanel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QGraphicsDropShadowEffect>
#include <QDate>

WeatherPanel::WeatherPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WeatherPanel)
{
    ui->setupUi(this);

    // Network
    manager =
        new QNetworkAccessManager(this);

    connect(manager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherPanel::onWeatherReply);

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
        "color:white;"
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
        "color:white;"
        "background:transparent;"
        "border:none;"
    );

    ui->labelWeather->setStyleSheet(
        "font-size:32px;"
        "color:white;"
        "background:transparent;"
        "border:none;"
    );

    ui->labelDetail->setStyleSheet(
        "font-size:18px;"
        "color:white;"
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
            QHeaderView::Stretch);

    ui->tableForecast->horizontalHeader()
        ->setFixedHeight(32);

    //--------------------------------
    // Table Style
    //--------------------------------

    ui->tableForecast->setStyleSheet(
        "QTableWidget {"
        "background:transparent;"
        "border:none;"
        "font-size:20px;"
        "color:white;"
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
            "font-size:16px;"
            "font-weight:bold;"
            "color:white;"
            "}");

    // Row Height
    for(int i = 0; i < 3; i++)
    {
        ui->tableForecast
            ->setRowHeight(i, 36);
    }

    // Start
    requestWeather("Seoul");
}

WeatherPanel::~WeatherPanel()
{
    delete ui;
}

void WeatherPanel::requestWeather(QString city)
{
    double lat = 37.5665;
    double lon = 126.9780;

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

void WeatherPanel::onWeatherReply(
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

    QJsonObject current =
        obj["current"].toObject();

    double temp =
        current["temperature_2m"].toDouble();

    int humidity =
        current["relative_humidity_2m"].toInt();

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
        QString("습도 %1%")
            .arg(humidity));

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

    //--------------------------------
    // Empty Header
    //--------------------------------

    ui->tableForecast
        ->setHorizontalHeaderItem(
            0,
            new QTableWidgetItem(""));

    //--------------------------------
    // Left Labels
    //--------------------------------

    QTableWidgetItem *weatherLabel =
        new QTableWidgetItem("날씨");

    weatherLabel->setTextAlignment(
        Qt::AlignCenter);

    weatherLabel->setForeground(
        QColor("white"));

    ui->tableForecast->setItem(
        0, 0, weatherLabel);

    QTableWidgetItem *maxLabel =
        new QTableWidgetItem("최고");

    maxLabel->setTextAlignment(
        Qt::AlignCenter);

    maxLabel->setForeground(
        QColor("#ff6666"));

    ui->tableForecast->setItem(
        1, 0, maxLabel);

    QTableWidgetItem *minLabel =
        new QTableWidgetItem("최저");

    minLabel->setTextAlignment(
        Qt::AlignCenter);

    minLabel->setForeground(
        QColor("#66aaff"));

    ui->tableForecast->setItem(
        2, 0, minLabel);

    //--------------------------------
    // Week Names
    //--------------------------------

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

        //--------------------------------
        // Header
        //--------------------------------

        QTableWidgetItem *headerItem =
            new QTableWidgetItem(dayName);

        headerItem->setTextAlignment(
            Qt::AlignCenter);

        headerItem->setForeground(
            QColor("white"));

        ui->tableForecast
            ->setHorizontalHeaderItem(
                i + 1,
                headerItem);

        //--------------------------------
        // Weather Icon
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
        // Icon Row
        //--------------------------------

        QTableWidgetItem *iconItem =
            new QTableWidgetItem(dayIcon);

        iconItem->setTextAlignment(
            Qt::AlignCenter);

        ui->tableForecast->setItem(
            0,
            i + 1,
            iconItem);

        //--------------------------------
        // Max Temp Row
        //--------------------------------

        QTableWidgetItem *maxItem =
            new QTableWidgetItem(
                QString::number(maxTemp)
                + "°");

        maxItem->setTextAlignment(
            Qt::AlignCenter);

        maxItem->setForeground(
            QColor("#ff6666"));

        ui->tableForecast->setItem(
            1,
            i + 1,
            maxItem);

        //--------------------------------
        // Min Temp Row
        //--------------------------------

        QTableWidgetItem *minItem =
            new QTableWidgetItem(
                QString::number(minTemp)
                + "°");

        minItem->setTextAlignment(
            Qt::AlignCenter);

        minItem->setForeground(
            QColor("#66aaff"));

        ui->tableForecast->setItem(
            2,
            i + 1,
            minItem);
    }

    reply->deleteLater();
}

QString WeatherPanel::koreanCityName(
    QString city)
{
    return city;
}
