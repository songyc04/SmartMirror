#include "weatherpanel.h"
#include "ui_weatherpanel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QGraphicsDropShadowEffect>
#include <QDate>
#include <QTime>
#include <QSet>

// ---------------------------------------------
// Helper: weather string to emoji icon
// ---------------------------------------------
static QString weatherToIcon(const QString &text)
{
    if (text.contains("clear") || text.contains("sunny"))   return "S";
    if (text.contains("cloud")) return "P";
    if (text.contains("overcast"))   return "C";
    if (text.contains("rain"))   return "R";
    if (text.contains("snow"))   return "N";
    return "C";
}

// ---------------------------------------------
// Helper: SKY code to emoji icon
// ---------------------------------------------
static QString skyCodeToIcon(const QString &skyCode)
{
    if (skyCode == "DB01") return "S";   // clear
    if (skyCode == "DB02") return "P";  // partly cloudy
    if (skyCode == "DB03") return "P";  // mostly cloudy
    if (skyCode == "DB04") return "C";  // overcast
    return "C";
}

WeatherPanel::WeatherPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WeatherPanel)
{
    ui->setupUi(this);

    // -- Network manager -------------------------
    manager = new QNetworkAccessManager(this);
    connect(manager,
            &QNetworkAccessManager::finished,
            this, &WeatherPanel::onWeatherReply);

    midLandManager = new QNetworkAccessManager(this);
    connect(midLandManager,
            &QNetworkAccessManager::finished,
            this, &WeatherPanel::onMidLandReply);

    midTempManager = new QNetworkAccessManager(this);
    connect(midTempManager,
            &QNetworkAccessManager::finished,
            this, &WeatherPanel::onMidTempReply);

    shortTermManager = new QNetworkAccessManager(this);
    connect(shortTermManager,
            &QNetworkAccessManager::finished,
            this, &WeatherPanel::onShortTermReply);

    dailyManager = new QNetworkAccessManager(this);
    connect(dailyManager,
            &QNetworkAccessManager::finished,
            this,
            &WeatherPanel::onDailyForecastReply);
    // Set region
    setRegion(1);

    // -- Panel style -----------------------------
    this->setStyleSheet(
        "QWidget {"
        "background:rgba(15,18,25,230);"
        "border:1px solid rgba(142,205,247,0.25);"
        "border-radius:20px;"
        "color:white;"
        "}");

    // -- Table initial setup ---------------------
    ui->tableForecast->setRowCount(3);
    ui->tableForecast->setColumnCount(8);
    ui->tableForecast->verticalHeader()->setVisible(false);
    ui->tableForecast->setShowGrid(false);
    ui->tableForecast->setFrameShape(QFrame::NoFrame);
    ui->tableForecast->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableForecast->setSelectionMode(QAbstractItemView::NoSelection);
    ui->tableForecast->setFocusPolicy(Qt::NoFocus);
    ui->tableForecast->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tableForecast->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Column width
    ui->tableForecast->setColumnWidth(0, 80);
    ui->tableForecast->horizontalHeader()
        ->setSectionResizeMode(0, QHeaderView::Interactive);
    for (int i = 1; i < 8; i++)
    {
        ui->tableForecast->horizontalHeader()
            ->setSectionResizeMode(i, QHeaderView::Stretch);
    }

    // Header
    ui->tableForecast->horizontalHeader()->setFixedHeight(32);
    ui->tableForecast->horizontalHeader()
        ->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableForecast->setHorizontalHeaderItem(
        0, new QTableWidgetItem(""));

    // Row height
    for (int i = 0; i < 3; i++)
        ui->tableForecast->setRowHeight(i, 36);

    // Row labels
    ui->tableForecast->setItem(0, 0, new QTableWidgetItem("Weather"));
    ui->tableForecast->setItem(1, 0, new QTableWidgetItem("High"));
    ui->tableForecast->setItem(2, 0, new QTableWidgetItem("Low"));
    ui->tableForecast->item(0, 0)->setForeground(QColor(255, 255, 255));
    ui->tableForecast->item(1, 0)->setForeground(QColor(255, 255, 255));
    ui->tableForecast->item(2, 0)->setForeground(QColor(255, 255, 255));

    QFont boldFont;
    boldFont.setBold(true);
    ui->tableForecast->item(0, 0)->setFont(boldFont);
    ui->tableForecast->item(1, 0)->setFont(boldFont);
    ui->tableForecast->item(2, 0)->setFont(boldFont);

    // -- Table QSS -------------------------------
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

    ui->tableForecast->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "background:transparent;"
        "border:none;"
        "font-size:18px;"
        "font-weight:bold;"
        "padding-top:4px;"
        "}");

    // -- Current weather label style -------------
    ui->labelLocation->setStyleSheet(
        "font-size:20px; font-weight:600; color:#8ecdf7;"
        "background:transparent; border:none;");
    ui->labelIcon->setStyleSheet(
        "font-size:68px; background:transparent; border:none;");
    ui->labelTemp->setStyleSheet(
        "font-size:52px; font-weight:bold; color:#ffffff;"
        "background:transparent; border:none;");
    ui->labelWeather->setStyleSheet(
        "font-size:26px; color:#cccccc; background:transparent; border:none;");
    ui->labelDetail->setStyleSheet(
        "font-size:16px; color:#999999; background:transparent; border:none;");

    // -- Forecast card shadow --------------------
    ui->forecastCard->setStyleSheet(
        "background:rgba(142,205,247,0.06);"
        "border-radius:18px; border:1px solid rgba(142,205,247,0.12);");

    QGraphicsDropShadowEffect *shadow =
        new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setOffset(0, 5);
    shadow->setColor(QColor(0, 0, 0, 80));
    ui->forecastCard->setGraphicsEffect(shadow);
}

WeatherPanel::~WeatherPanel()
{
    delete ui;
}

// ---------------------------------------------
// Request ultra-short-term weather (current temp/weather)
// ---------------------------------------------
void WeatherPanel::requestWeather()
{
    QString baseDate =
        QDate::currentDate().toString("yyyyMMdd");
    int hour = QTime::currentTime().hour();
    QString baseTime =
        QString("%1").arg(hour, 2, 10, QChar('0')) + "00";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "VilageFcstInfoService_2.0/"
            "getUltraSrtNcst?"
            "pageNo=1&numOfRows=1000&dataType=JSON"
            "&base_date=%1"
            "&base_time=%2"
            "&nx=%3"
            "&ny=%4"
            "&authKey=umH6GTnpT9-h-hk56Z_fKA")
            .arg(baseDate)
            .arg(baseTime)
            .arg(currentRegion.nx)
            .arg(currentRegion.ny);

    manager->get(QNetworkRequest(QUrl(url)));
}

// ---------------------------------------------
// Request mid-term sky status
// ---------------------------------------------
void WeatherPanel::requestMidLand()
{
    QString tmFc = QDate::currentDate().toString("yyyyMMdd") + "0600";
    //tmFc += (QTime::currentTime().hour() >= 18) ? "1800" : "0600";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "MidFcstInfoService/getMidLandFcst?"
            "pageNo=1&numOfRows=10&dataType=JSON"
            "&regId=%1"
            "&tmFc=%2"
            "&authKey=umH6GTnpT9-h-hk56Z_fKA")
            .arg(currentRegion.midLandRegId)
            .arg(tmFc);

    midLandManager->get(QNetworkRequest(QUrl(url)));
}

// ---------------------------------------------
// Request mid-term temperature
// ---------------------------------------------
void WeatherPanel::requestMidTemp()
{
    QString tmFc = QDate::currentDate().toString("yyyyMMdd") + "0600";
    //tmFc += (QTime::currentTime().hour() >= 18) ? "1800" : "0600";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "MidFcstInfoService/getMidTa?"
            "pageNo=1&numOfRows=10&dataType=JSON"
            "&regId=%1"
            "&tmFc=%2"
            "&authKey=umH6GTnpT9-h-hk56Z_fKA")
            .arg(currentRegion.midTempRegId)
            .arg(tmFc);

    midTempManager->get(QNetworkRequest(QUrl(url)));
}

// ---------------------------------------------
// Request short-term forecast (D+1 ~ D+3)
// regId=11B10101 (Seoul)
// ---------------------------------------------
void WeatherPanel::requestShortTerm()
{
    QString url =
        "https://apihub.kma.go.kr/api/typ01/url/fct_afs_dl.php"
        "?disp=1&help=0&authKey=umH6GTnpT9-h-hk56Z_fKA";

    shortTermManager->get(QNetworkRequest(QUrl(url)));
}

// Request today's high/low temperature
void WeatherPanel::requestDailyForecast()
{
    QString baseDate =
        QDate::currentDate().toString("yyyyMMdd");

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "VilageFcstInfoService_2.0/getVilageFcst?"
            "pageNo=1"
            "&numOfRows=1000"
            "&dataType=JSON"
            "&base_date=%1"
            "&base_time=0500"
            "&nx=%2"
            "&ny=%3"
            "&authKey=umH6GTnpT9-h-hk56Z_fKA")
            .arg(baseDate)
            .arg(currentRegion.nx)
            .arg(currentRegion.ny);

    dailyManager->get(QNetworkRequest(QUrl(url)));
}

// ---------------------------------------------
// Handle ultra-short-term response (current temp/weather panel)
// ---------------------------------------------
void WeatherPanel::onWeatherReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qDebug() << "Ultra-short-term JSON parse failed";
        reply->deleteLater();
        return;
    }

    QJsonArray itemArray =
        doc.object()["response"]
            .toObject()["body"]
            .toObject()["items"]
            .toObject()["item"]
            .toArray();

    double temp     = 0;
    double humidity = 0;
    double rain     = 0;
    double wind     = 0;
    int    pty      = 0;
    int    rn1      = 0;

    for (const QJsonValue &v : itemArray)
    {
        QJsonObject o = v.toObject();
        QString cat   = o["category"].toString();
        double  val   = o["obsrValue"].toString().toDouble();

        if (cat == "T1H") { temp     = val; currentTemp = val; }
        if (cat == "REH") { humidity = val; }
        if (cat == "RN1") { rain     = val; }
        if (cat == "WSD") { wind     = val; }
        if (cat == "PTY") { pty      = (int)val; }
    }

    QString weatherText, icon;
    switch (pty)
    {
    case 1: weatherText = "Rain";    icon = "R"; break;
    case 2: weatherText = "Rain/Snow"; icon = "RS"; break;
    case 3: weatherText = "Snow";    icon = "N";  break;
    case 5: weatherText = "Raindrop"; icon = "RD"; break;
    case 6: weatherText = "Rain/Snow"; icon = "RS"; break;
    case 7: weatherText = "Snow"; icon = "N"; break;
    default: weatherText = "Clear";  icon = "S";  break;
    }

    ui->labelIcon->setText(icon);
    ui->labelTemp->setText(
        QString::number(temp, 'f', 1) + "C");
    ui->labelWeather->setText(weatherText);
    ui->labelDetail->setText(
        QString("Humidity %1%   Rain %2mm   Wind %3m/s")
            .arg(humidity).arg(rain).arg(wind));
    ui->labelLocation->setText(
        "Location: " + currentRegion.name);

    reply->deleteLater();
}

// ---------------------------------------------
// Handle mid-term sky response
// ---------------------------------------------
void WeatherPanel::onMidLandReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qDebug() << "MidLand JSON parse failed";
        reply->deleteLater();
        return;
    }

    QJsonArray arr =
        doc.object()["response"]
            .toObject()["body"]
            .toObject()["items"]
            .toObject()["item"]
            .toArray();

    if (arr.isEmpty())
    {
        qDebug() << "MidLand no items";
        reply->deleteLater();
        return;
    }

    midLandData = arr.first().toObject();
    reply->deleteLater();
    tryFillMidForecast();
}

// ---------------------------------------------
// Handle mid-term temperature response
// ---------------------------------------------
void WeatherPanel::onMidTempReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        reply->deleteLater();
        return;
    }

    QJsonArray arr =
        doc.object()["response"]
            .toObject()["body"]
            .toObject()["items"]
            .toObject()["item"]
            .toArray();

    if (arr.isEmpty())
    {
        reply->deleteLater();
        return;
    }

    midTempData = arr.first().toObject();
    reply->deleteLater();
    tryFillMidForecast();
}

// ---------------------------------------------
// Handle short-term forecast response
//
// CSV column order:
//   REG_ID, TM_FC, TM_EF, MOD, NE, STN, C,
//   MAN_ID, MAN_FC, W1, T, W2, TA, ST, SKY, PREP, WF
//
// Seoul regId = 11B10101
// NE: 0=today_day(current), 1=today_night, 2=tomorrow_day, 3=tomorrow_night,
//     4=day_after_day, 5=day_after_night, 6=D+3_day, 7=D+3_night, 8=D+4_day
//
// Map D+1~D+3 dates using TM_EF
// TA=temperature, SKY=sky_code(DB01/02/03/04)
// ---------------------------------------------
void WeatherPanel::onShortTermReply(QNetworkReply *reply)
{
    // EUC-KR encoding, decode with QTextCodec
    QByteArray rawData = reply->readAll();
    reply->deleteLater();

    // EUC-KR to UTF-8 conversion
    QTextCodec *codec = QTextCodec::codecForName("EUC-KR");
    QString text = codec ? codec->toUnicode(rawData) : QString::fromUtf8(rawData);


    // Seoul region code
    const QString targetRegId =
        currentRegion.shortTermRegId;

    // Store daily max/min temp and SKY code per offset
    // key: offset(1=tomorrow, 2=day_after, 3=D+3)
    struct DayData {
        int    maxTemp  = -999;
        int    minTemp  = 999;
        QString skyIcon = "";
    };
    QMap<int, DayData> dayMap;

    QDate today = QDate::currentDate();

    QStringList lines = text.split('\n');
    for (const QString &line : lines)
    {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        // Remove trailing '='
        if (trimmed.endsWith(',') || trimmed.endsWith('='))
            trimmed.chop(1);

        QStringList cols = trimmed.split(',');
        if (cols.size() < 16)
            continue;

        QString regId = cols[0].trimmed();
        if (regId != targetRegId)
            continue;

        // TM_EF: yyyyMMddHHmm
        QString tmEf = cols[2].trimmed();
        if (tmEf.length() < 8)
            continue;

        QString dateStr = tmEf.left(8);      // yyyyMMdd
        QString hourStr = tmEf.mid(8, 2);    // HH

        QDate   targetDate = QDate::fromString(dateStr, "yyyyMMdd");
        int     hour       = hourStr.toInt();
        int     offset     = today.daysTo(targetDate); // 0=today, 1=tomorrow, ...

        if (offset < 0 || offset > 3)
            continue;  // Only process D+0 ~ D+3

        QString skyCode = cols[14].trimmed(); // SKY
        int     ta      = cols[12].trimmed().toInt(); // TA

        DayData &dd = dayMap[offset];

        if (ta != -99)
        {
            dd.maxTemp = qMax(dd.maxTemp, ta);
            dd.minTemp = qMin(dd.minTemp, ta);
        }

        if (dd.skyIcon.isEmpty())
        {
            dd.skyIcon = skyCodeToIcon(skyCode);
        }
    }

    // -- Fill table col 1~3 (D+1=col2, D+2=col3, D+3=col4) --
    // col 1 = today, col 2 = tomorrow(offset1), col 3 = day_after(offset2), col 4 = D+3(offset3)
    const QStringList weekNames = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

    for (int offset = 0; offset <= 3; offset++)
    {
        int col =1+offset; // col1=today, col2, col3, col4

        QDate targetDate = today.addDays(offset);
        QString dayName  = weekNames[targetDate.dayOfWeek() % 7];

        // Header day name
        ui->tableForecast->setHorizontalHeaderItem(
            col, new QTableWidgetItem("  " + dayName));

        if (!dayMap.contains(offset))
        {
            if (col != 0) { ui->tableForecast->setItem(0, col, new QTableWidgetItem("")); }
            if (col != 0) { ui->tableForecast->setItem(1, col, new QTableWidgetItem("-")); }
            if (col != 0) { ui->tableForecast->setItem(2, col, new QTableWidgetItem("-")); }
            continue;
        }

        const DayData &dd = dayMap[offset];

        QString icon    = dd.skyIcon.isEmpty() ? "C" : dd.skyIcon;
        QString maxStr;
        QString minStr;

        if(offset == 0 && todayMaxTemp != -999){
            maxStr=QString::number(todayMaxTemp)+"";
        }
        else{
            maxStr=(dd.maxTemp != -999) ? QString::number(dd.maxTemp) + "" : "-";
        }
        if(offset == 0 && todayMinTemp != -999)
        {
            minStr = QString::number(todayMinTemp) + "";
        }
        else
        {
            minStr =
                (dd.minTemp != 999)
                ? QString::number(dd.minTemp) + ""
                : "-";
        }

        auto setCell = [this](int row, int col, const QString &text) {
            QTableWidgetItem *it = new QTableWidgetItem(text);
            if (row == 0) it->setForeground(QColor(255, 255, 255));
            else if (row == 1) it->setForeground(QColor(220, 50, 50));
            else if (row == 2) it->setForeground(QColor(80, 160, 255));
            if (col != 0) { ui->tableForecast->setItem(row, col, it); }
        };
        setCell(0, col, icon);
        setCell(1, col, maxStr);
        setCell(2, col, minStr);
    }
    for(int c = 0; c < ui->tableForecast->columnCount(); c++)
    {
        auto h = ui->tableForecast->horizontalHeaderItem(c);
    }
}

// ---------------------------------------------
// Fill table D+4+ with mid-term forecast
// ---------------------------------------------
void WeatherPanel::tryFillMidForecast()
{
    if (midLandData.isEmpty() || midTempData.isEmpty())
        return;

    const QStringList weekNames = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    QDate today = QDate::currentDate();

    // Mid-term forecast provides D+4(offset=4) ~ D+10(offset=10)
    // col 1=today(D+0), col 2=D+1, ..., col 5=D+4, col 6=D+5, col 7=D+6
    for (int offset = 4; offset <= 7; offset++)
    {
        int col = 1 + offset; // col 5~8 but table only goes to col 7, range check
        if (col >= ui->tableForecast->columnCount())
            break;

        QDate target = today.addDays(offset);
        QString dayName = weekNames[target.dayOfWeek() % 7];

        ui->tableForecast->setHorizontalHeaderItem(
            col, new QTableWidgetItem("  " + dayName));

        // Key names: taMax4, taMax5 ... (no padding)
        int maxTemp = midTempData[QString("taMax%1").arg(offset)].toInt();
        int minTemp = midTempData[QString("taMin%1").arg(offset)].toInt();

        QString wfAm = midLandData[QString("wf%1Am").arg(offset)].toString();
        QString wfPm = midLandData[QString("wf%1Pm").arg(offset)].toString();
        // D+8+ uses wf8, wf9... without Am/Pm
        if (wfAm.isEmpty())
            wfAm = midLandData[QString("wf%1").arg(offset)].toString();
        if (wfPm.isEmpty())
            wfPm = wfAm;

        QString weather = wfPm;
        if (wfAm.contains("rain") || wfAm.contains("snow"))
            weather = wfAm;
        if (weather.isEmpty()) weather = wfAm;

        QString icon = weatherToIcon(weather);

        auto setCell = [this](int row, int col, const QString &text) {
            QTableWidgetItem *it = new QTableWidgetItem(text);
            if (row == 0)      it->setForeground(QColor(255, 255, 255));
            else if (row == 1) it->setForeground(QColor(220, 50, 50));
            else if (row == 2) it->setForeground(QColor(80, 160, 255));
            ui->tableForecast->setItem(row, col, it);
        };

        setCell(0, col, icon);
        setCell(1, col, QString::number(maxTemp) + "");
        setCell(2, col, QString::number(minTemp) + "");
    }
}

// Fill today's table
void WeatherPanel::onDailyForecastReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull())
    {
        reply->deleteLater();
        return;
    }

    QJsonArray items =
        doc.object()["response"]
           .toObject()["body"]
           .toObject()["items"]
           .toObject()["item"]
           .toArray();

    todayMinTemp = 999;
    todayMaxTemp = -999;

    QString today =
        QDate::currentDate().toString("yyyyMMdd");

    for (const QJsonValue &v : items)
    {
        QJsonObject obj = v.toObject();

        QString category =
            obj["category"].toString();

        QString fcstDate =
            obj["fcstDate"].toString();

        if (fcstDate != today)
            continue;

        if (category == "TMP")
        {
            int temp =
                obj["fcstValue"].toString().toInt();

            todayMinTemp =
                qMin(todayMinTemp, temp);

            todayMaxTemp =
                qMax(todayMaxTemp, temp);
        }
    }

    if (todayMinTemp == 999)
        todayMinTemp = -999;

    reply->deleteLater();

    requestShortTerm();
}


// ---------------------------------------------
// Apply brightness
// ---------------------------------------------
void WeatherPanel::setTextBrightness(int value)
{
    QString color = QString("rgb(%1,%1,%1)").arg(value);

    this->setStyleSheet(
        QString(
            "QWidget {"
            "background:rgba(15,18,25,230);"
            "border:1px solid rgba(142,205,247,0.25);"
            "border-radius:20px;"
            "color:%1;"
            "}").arg(color));

    QColor textColor(value, value, value);

    for (int row = 0; row < ui->tableForecast->rowCount(); row++)
    {
        for (int col = 0; col < ui->tableForecast->columnCount(); col++)
        {
            QTableWidgetItem *item =
                ui->tableForecast->item(row, col);
            if (!item) continue;

            if (row == 0)
                item->setForeground(QColor(value, value, value));
            else if (row == 1)
                item->setForeground(QColor(qMin(value + 80, 255), 80, 80));
            else if (row == 2)
                item->setForeground(QColor(80, 160, qMin(value + 80, 255)));
        }
    }

    for (int col = 0; col < ui->tableForecast->columnCount(); col++)
    {
        QTableWidgetItem *header =
            ui->tableForecast->horizontalHeaderItem(col);
        if (header)
            header->setForeground(textColor);
    }
}

void WeatherPanel::setRegion(int regionCode)
{
    switch(regionCode)
    {
    case 1: // Seoul
        currentRegion =
        {
            "Seoul",
            60,
            127,
            "11B10101",
            "11B00000",
            "11B10101"
        };
        break;

    case 2: // Busan
        currentRegion =
        {
            "Busan",
            98,
            76,
            "11H20201",
            "11H20000",
            "11H20201"
        };
        break;

    case 3: // Daegu
        currentRegion =
        {
            "Daegu",
            89,
            90,
            "11H10701",
            "11H10000",
            "11H10701"
        };
        break;

    case 4: // Incheon
        currentRegion =
        {
            "Incheon",
            55,
            124,
            "11B20201",
            "11B00000",
            "11B20201"
        };
        break;

    case 5: // Gwangju
        currentRegion =
        {
            "Gwangju",
            58,
            74,
            "11F20501",
            "11F20000",
            "11F20501"
        };
        break;

    case 6: // Daejeon
        currentRegion =
        {
            "Daejeon",
            67,
            100,
            "11C20401",
            "11C20000",
            "11C20401"
        };
        break;

    case 7: // Ulsan
        currentRegion =
        {
            "Ulsan",
            102,
            84,
            "11H20101",
            "11H10000",
            "11H20101"
        };
        break;

    case 8: // Jeju
        currentRegion =
        {
            "Jeju",
            52,
            38,
            "11G00201",
            "11G00000",
            "11G00201"
        };
        break;

    default:
        return;
    }

    ui->labelLocation->setText(
        "Location: " + currentRegion.name);

    requestWeather();
    requestDailyForecast();
    requestShortTerm();
    requestMidLand();
    requestMidTemp();
}
