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
// 헬퍼: 날씨 문자열을 아이콘으로 변환
// ---------------------------------------------
static QString weatherToIcon(const QString &text)
{
    if (text.contains("맑"))   return "☀️";
    if (text.contains("구름")) return "🌤";
    if (text.contains("흐"))   return "☁️";
    if (text.contains("비"))   return "🌧️";
    if (text.contains("눈"))   return "❄️";
    return "☁️";
}

// ---------------------------------------------
// 헬퍼: SKY 코드를 아이콘으로 변환
// ---------------------------------------------
static QString skyCodeToIcon(const QString &skyCode)
{
    if (skyCode == "DB01") return "☀️";   // 맑음
    if (skyCode == "DB02") return "🌤";  // 구름 조금
    if (skyCode == "DB03") return "🌥️";  // 구름 많음
    if (skyCode == "DB04") return "☁️";  // 흐림
    return "☁️";
}

WeatherPanel::WeatherPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WeatherPanel)
{
    ui->setupUi(this);

    // -- 네트워크 매니저 -------------------------
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
    // 지역 설정
    setRegion(1);

    // -- 패널 스타일 -----------------------------
    this->setStyleSheet(
        "QWidget {"
        "background:transparent;"
        "color:white;"
        "}");

    // -- 테이블 초기 설정 ---------------------
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

    // 열 너비 - 모든 열 균등 분할
    for (int i = 0; i < 8; i++)
    {
        ui->tableForecast->horizontalHeader()
            ->setSectionResizeMode(i, QHeaderView::Stretch);
    }

    // Header
    ui->tableForecast->horizontalHeader()->setFixedHeight(30);
    ui->tableForecast->horizontalHeader()
        ->setDefaultAlignment(Qt::AlignCenter | Qt::AlignVCenter);

    // 행 높이
    for (int i = 0; i < 3; i++)
        ui->tableForecast->setRowHeight(i, 40);

    // 행 라벨
    QTableWidgetItem *weatherLabel = new QTableWidgetItem("날씨");
    weatherLabel->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    weatherLabel->setForeground(QColor(255, 255, 255));
    ui->tableForecast->setItem(0, 0, weatherLabel);

    QTableWidgetItem *maxLabel = new QTableWidgetItem("최고");
    maxLabel->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    maxLabel->setForeground(QColor(220, 50, 50));
    ui->tableForecast->setItem(1, 0, maxLabel);

    QTableWidgetItem *minLabel = new QTableWidgetItem("최저");
    minLabel->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    minLabel->setForeground(QColor(80, 160, 255));
    ui->tableForecast->setItem(2, 0, minLabel);

    QFont labelFont;
    labelFont.setPointSize(13);
    ui->tableForecast->item(0, 0)->setFont(labelFont);
    ui->tableForecast->item(1, 0)->setFont(labelFont);
    ui->tableForecast->item(2, 0)->setFont(labelFont);

    // -- 테이블 QSS -------------------------------
    ui->tableForecast->setStyleSheet(
        "QTableWidget {"
        "background:transparent;"
        "border:none;"
        "font-size:16px;"
        "}"
        "QTableWidget::item {"
        "border:none;"
        "padding:2px;"
        "}");

    ui->tableForecast->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "background:transparent;"
        "border:none;"
        "font-size:15px;"
        "font-weight:500;"
        "color:#ffffff;"
        "padding:2px;"
        "}");

    // -- 현재 날씨 라벨 스타일 -------------
    ui->labelLocation->setStyleSheet(
        "font-size:18px; font-weight:500; color:#C9A96E;"
        "background:transparent; border:none;");
    ui->labelIcon->setStyleSheet(
        "font-size:48px; color:#ffffff; background:transparent; border:none;");
    ui->labelTemp->setStyleSheet(
        "font-size:42px; font-weight:300; color:#ffffff;"
        "background:transparent; border:none;");
    ui->labelWeather->setStyleSheet(
        "font-size:22px; color:#ffffff; background:transparent; border:none;");
    ui->labelDetail->setStyleSheet(
        "font-size:13px; color:#999999; background:transparent; border:none;");

    // -- 예보 카드 그림자 --------------------
    QGraphicsDropShadowEffect *shadow =
        new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 100));
    ui->forecastCard->setGraphicsEffect(shadow);
}

WeatherPanel::~WeatherPanel()
{
    delete ui;
}

// ---------------------------------------------
// 초단기 날씨 요청 (현재 기온/날씨)
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
// 중기 하늘 상태 요청
// ---------------------------------------------
void WeatherPanel::requestMidLand()
{
    QString tmFc = QDate::currentDate().toString("yyyyMMdd") + "0600";

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
// 중기 기온 요청
// ---------------------------------------------
void WeatherPanel::requestMidTemp()
{
    QString tmFc = QDate::currentDate().toString("yyyyMMdd") + "0600";

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
// 단기 예보 요청 (D+1 ~ D+3)
// ---------------------------------------------
void WeatherPanel::requestShortTerm()
{
    QString url =
        "https://apihub.kma.go.kr/api/typ01/url/fct_afs_dl.php"
        "?disp=1&help=0&authKey=umH6GTnpT9-h-hk56Z_fKA";

    shortTermManager->get(QNetworkRequest(QUrl(url)));
}

// 오늘 최고/최저 기온 요청
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
// 초단기 응답 처리 (현재 기온/날씨 패널)
// ---------------------------------------------
void WeatherPanel::onWeatherReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qDebug() << "초단기 JSON 파싱 실패";
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
    case 1: weatherText = "비";    icon = "R"; break;
    case 2: weatherText = "비/눈"; icon = "RS"; break;
    case 3: weatherText = "눈";    icon = "N";  break;
    case 5: weatherText = "빗방울"; icon = "RD"; break;
    case 6: weatherText = "비/눈"; icon = "RS"; break;
    case 7: weatherText = "눈"; icon = "N"; break;
    default: weatherText = "맑음";  icon = "S";  break;
    }

    ui->labelIcon->setText(icon);
    ui->labelTemp->setText(
        QString::number(temp, 'f', 1) + "°C");
    ui->labelWeather->setText(weatherText);
    ui->labelDetail->setText(
        QString("습도 %1%   강수 %2mm   풍속 %3m/s")
            .arg(humidity).arg(rain).arg(wind));
    ui->labelLocation->setText(
        "위치: " + currentRegion.name);

    reply->deleteLater();
}

// ---------------------------------------------
// 중기 하늘 상태 응답 처리
// ---------------------------------------------
void WeatherPanel::onMidLandReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qDebug() << "중기 육상 JSON 파싱 실패";
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
        qDebug() << "중기 육상 항목 없음";
        reply->deleteLater();
        return;
    }

    midLandData = arr.first().toObject();
    reply->deleteLater();
    tryFillMidForecast();
}

// ---------------------------------------------
// 중기 기온 응답 처리
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
// 단기 예보 응답 처리
// ---------------------------------------------
void WeatherPanel::onShortTermReply(QNetworkReply *reply)
{
    QByteArray rawData = reply->readAll();
    reply->deleteLater();

    QTextCodec *codec = QTextCodec::codecForName("EUC-KR");
    QString text = codec ? codec->toUnicode(rawData) : QString::fromUtf8(rawData);

    const QString targetRegId =
        currentRegion.shortTermRegId;

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

        if (trimmed.endsWith(',') || trimmed.endsWith('='))
            trimmed.chop(1);

        QStringList cols = trimmed.split(',');
        if (cols.size() < 16)
            continue;

        QString regId = cols[0].trimmed();
        if (regId != targetRegId)
            continue;

        QString tmEf = cols[2].trimmed();
        if (tmEf.length() < 8)
            continue;

        QString dateStr = tmEf.left(8);
        QString hourStr = tmEf.mid(8, 2);

        QDate   targetDate = QDate::fromString(dateStr, "yyyyMMdd");
        int     hour       = hourStr.toInt();
        int     offset     = today.daysTo(targetDate);

        if (offset < 0 || offset > 6)
            continue;

        QString skyCode = cols[14].trimmed();
        int     ta      = cols[12].trimmed().toInt();

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

    // 테이블 채우기: col 0=행레이블, col 1~7=D+0~D+6
    const QStringList weekNames = {"일","월","화","수","목","금","토"};

    for (int offset = 0; offset <= 6; offset++)
    {
        int col = 1 + offset;

        QDate targetDate = today.addDays(offset);
        QString dayName  = weekNames[targetDate.dayOfWeek() % 7];

        ui->tableForecast->setHorizontalHeaderItem(
            col, new QTableWidgetItem(dayName));

        if (!dayMap.contains(offset))
        {
            ui->tableForecast->setItem(0, col, new QTableWidgetItem(""));
            ui->tableForecast->setItem(1, col, new QTableWidgetItem("-"));
            ui->tableForecast->setItem(2, col, new QTableWidgetItem("-"));
            continue;
        }

        const DayData &dd = dayMap[offset];

        QString icon    = dd.skyIcon.isEmpty() ? "C" : dd.skyIcon;
        QString maxStr;
        QString minStr;

        if(offset == 0 && todayMaxTemp != -999){
            maxStr=QString::number(todayMaxTemp);
        }
        else{
            maxStr=(dd.maxTemp != -999) ? QString::number(dd.maxTemp) : "-";
        }
        if(offset == 0 && todayMinTemp != -999)
        {
            minStr = QString::number(todayMinTemp);
        }
        else
        {
            minStr =
                (dd.minTemp != 999)
                ? QString::number(dd.minTemp)
                : "-";
        }

        auto setCell = [this](int row, int col, const QString &text) {
            QTableWidgetItem *it = new QTableWidgetItem(text);
            it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
            if (row == 0) it->setForeground(QColor(255, 255, 255));
            else if (row == 1) it->setForeground(QColor(220, 50, 50));
            else if (row == 2) it->setForeground(QColor(80, 160, 255));
            ui->tableForecast->setItem(row, col, it);
        };
        setCell(0, col, icon);
        setCell(1, col, maxStr);
        setCell(2, col, minStr);
    }

    // 단기 예보 처리 후 중기 예보 데이터가 있으면 함께 채움
    tryFillMidForecast();
}

// ---------------------------------------------
// 중기 예보로 D+4 이상 테이블 채우기
// ---------------------------------------------
void WeatherPanel::tryFillMidForecast()
{
    if (midLandData.isEmpty() || midTempData.isEmpty())
        return;

    const QStringList weekNames = {"일","월","화","수","목","금","토"};
    QDate today = QDate::currentDate();

    for (int offset = 4; offset <= 6; offset++)
    {
        int col = 1 + offset;
        if (col >= ui->tableForecast->columnCount())
            break;

        QDate target = today.addDays(offset);
        QString dayName = weekNames[target.dayOfWeek() % 7];

        ui->tableForecast->setHorizontalHeaderItem(
            col, new QTableWidgetItem(dayName));

        int maxTemp = midTempData[QString("taMax%1").arg(offset)].toInt();
        int minTemp = midTempData[QString("taMin%1").arg(offset)].toInt();

        QString wfAm = midLandData[QString("wf%1Am").arg(offset)].toString();
        QString wfPm = midLandData[QString("wf%1Pm").arg(offset)].toString();
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
            it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
            if (row == 0)      it->setForeground(QColor(255, 255, 255));
            else if (row == 1) it->setForeground(QColor(220, 50, 50));
            else if (row == 2) it->setForeground(QColor(80, 160, 255));
            ui->tableForecast->setItem(row, col, it);
        };

        setCell(0, col, icon);
        setCell(1, col, QString::number(maxTemp));
        setCell(2, col, QString::number(minTemp));
    }
}

// 오늘 테이블 채우기
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
// 밝기 적용
// ---------------------------------------------
void WeatherPanel::setTextBrightness(int value)
{
    QString color = QString("rgb(%1,%1,%1)").arg(value);

    this->setStyleSheet(
        QString(
            "QWidget {"
            "background:transparent;"
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
    case 1: // 서울
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

    case 2: // 부산
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

    case 3: // 대구
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

    case 4: // 인천
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

    case 5: // 광주
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

    case 6: // 대전
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

    case 7: // 울산
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

    case 8: // 제주
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
        "위치: " + currentRegion.name);

    requestWeather();
    requestDailyForecast();
    requestShortTerm();
    requestMidLand();
    requestMidTemp();
}
