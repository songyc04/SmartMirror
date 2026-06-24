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

// ─────────────────────────────────────────────
// 헬퍼: 날씨 문자열 → 이모지 아이콘
// ─────────────────────────────────────────────
static QString weatherToIcon(const QString &text)
{
    if (text.contains("맑"))   return "☀";
    if (text.contains("구름")) return "⛅";
    if (text.contains("흐"))   return "☁";
    if (text.contains("비"))   return "🌧";
    if (text.contains("눈"))   return "❄";
    return "☁";
}

// ─────────────────────────────────────────────
// 헬퍼: SKY 코드 → 이모지 아이콘
// ─────────────────────────────────────────────
static QString skyCodeToIcon(const QString &skyCode)
{
    if (skyCode == "DB01") return "☀";   // 맑음
    if (skyCode == "DB02") return "⛅";  // 구름조금
    if (skyCode == "DB03") return "⛅";  // 구름많음
    if (skyCode == "DB04") return "☁";  // 흐림
    return "☁";
}

WeatherPanel::WeatherPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WeatherPanel)
{
    ui->setupUi(this);

    // ── 네트워크 매니저 ──────────────────────
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
    //지역설정
    setRegion(1);

    // ── 패널 스타일 ──────────────────────────
    this->setStyleSheet(
        "QWidget {"
        "background:rgba(30,30,30,220);"
        "border:2px solid white;"
        "border-radius:20px;"
        "color:white;"
        "}");

    // ── 테이블 초기 설정 ─────────────────────
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

    // 열 너비
    ui->tableForecast->setColumnWidth(0, 80);
    ui->tableForecast->horizontalHeader()
        ->setSectionResizeMode(0, QHeaderView::Interactive);
    for (int i = 1; i < 8; i++)
    {
        ui->tableForecast->horizontalHeader()
            ->setSectionResizeMode(i, QHeaderView::Stretch);
    }

    // 헤더
    ui->tableForecast->horizontalHeader()->setFixedHeight(32);
    ui->tableForecast->horizontalHeader()
        ->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->tableForecast->setHorizontalHeaderItem(
        0, new QTableWidgetItem(""));

    // 행 높이
    for (int i = 0; i < 3; i++)
        ui->tableForecast->setRowHeight(i, 36);

    // 행 레이블
    ui->tableForecast->setItem(0, 0, new QTableWidgetItem("날씨"));
    ui->tableForecast->setItem(1, 0, new QTableWidgetItem("최고"));
    ui->tableForecast->setItem(2, 0, new QTableWidgetItem("최저"));
    ui->tableForecast->item(0, 0)->setForeground(QColor(255, 255, 255));
    ui->tableForecast->item(1, 0)->setForeground(QColor(255, 255, 255));
    ui->tableForecast->item(2, 0)->setForeground(QColor(255, 255, 255));

    QFont boldFont;
    boldFont.setBold(true);
    ui->tableForecast->item(0, 0)->setFont(boldFont);
    ui->tableForecast->item(1, 0)->setFont(boldFont);
    ui->tableForecast->item(2, 0)->setFont(boldFont);

    // ── 테이블 QSS ───────────────────────────
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

    // ── 현재날씨 레이블 스타일 ───────────────
    ui->labelLocation->setStyleSheet(
        "font-size:24px; font-weight:bold;"
        "background:transparent; border:none;");
    ui->labelIcon->setStyleSheet(
        "font-size:78px; background:transparent; border:none;");
    ui->labelTemp->setStyleSheet(
        "font-size:58px; font-weight:bold;"
        "background:transparent; border:none;");
    ui->labelWeather->setStyleSheet(
        "font-size:32px; background:transparent; border:none;");
    ui->labelDetail->setStyleSheet(
        "font-size:18px; background:transparent; border:none;");

    // ── Forecast 카드 그림자 ─────────────────
    ui->forecastCard->setStyleSheet(
        "background:rgba(255,255,255,0.08);"
        "border-radius:25px; border:none;");

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

// ─────────────────────────────────────────────
// 초단기실황 요청 (현재 기온/날씨 표시용)
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// 중기 하늘상태 요청
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// 중기 기온 요청
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// fct_afs_dl 단기예보 요청 (D+1 ~ D+3 채우기)
// regId=11B10101 (서울)
// ─────────────────────────────────────────────
void WeatherPanel::requestShortTerm()
{
    QString url =
        "https://apihub.kma.go.kr/api/typ01/url/fct_afs_dl.php"
        "?disp=1&help=0&authKey=umH6GTnpT9-h-hk56Z_fKA";

    shortTermManager->get(QNetworkRequest(QUrl(url)));
}

//오늘의 날씨 최고,최저 온도 요청
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

// ─────────────────────────────────────────────
// 초단기실황 응답 처리 (현재 기온/날씨 패널)
// ─────────────────────────────────────────────
void WeatherPanel::onWeatherReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qDebug() << "초단기실황 JSON 파싱 실패";
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
    case 1: weatherText = "비";    icon = "🌧"; break;
    case 2: weatherText = "비/눈"; icon = "🌨"; break;
    case 3: weatherText = "눈";    icon = "❄";  break;
    case 5: weatherText = "빗방울"; icon = "🌦"; break;
    case 6: weatherText = "빗방울/눈날림"; icon = "🌨"; break;
    case 7: weatherText = "눈날림"; icon = "❄"; break;
    default: weatherText = "맑음";  icon = "☀";  break;
    }

    ui->labelIcon->setText(icon);
    ui->labelTemp->setText(
        QString::number(temp, 'f', 1) + "°C");
    ui->labelWeather->setText(weatherText);
    ui->labelDetail->setText(
        QString("습도 %1%   강수 %2mm   풍속 %3m/s")
            .arg(humidity).arg(rain).arg(wind));
    ui->labelLocation->setText(
        "현재 위치 : " + currentRegion.name);

    reply->deleteLater();
}

// ─────────────────────────────────────────────
// 중기 하늘상태 응답 처리
// ─────────────────────────────────────────────
void WeatherPanel::onMidLandReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qDebug() << "MidLand JSON 파싱 실패";
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
        qDebug() << "MidLand 아이템 없음";
        reply->deleteLater();
        return;
    }

    midLandData = arr.first().toObject();
    reply->deleteLater();
    tryFillMidForecast();
}

// ─────────────────────────────────────────────
// 중기 기온 응답 처리
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// fct_afs_dl 단기예보 응답 처리
//
// CSV 컬럼 순서:
//   REG_ID, TM_FC, TM_EF, MOD, NE, STN, C,
//   MAN_ID, MAN_FC, W1, T, W2, TA, ST, SKY, PREP, WF
//
// 서울 regId = 11B10101
// NE: 0=오늘낮(현재), 1=오늘밤, 2=내일낮, 3=내일밤,
//     4=모레낮, 5=모레밤, 6=D+3낮, 7=D+3밤, 8=D+4낮
//
// TM_EF 날짜로 D+1~D+3 날짜를 정확히 매핑
// TA=기온, SKY=하늘코드(DB01/02/03/04)
// ─────────────────────────────────────────────
void WeatherPanel::onShortTermReply(QNetworkReply *reply)
{
    // EUC-KR 인코딩이므로 QTextCodec으로 디코딩
    QByteArray rawData = reply->readAll();
    reply->deleteLater();

    // EUC-KR → UTF-8 변환
    QTextCodec *codec = QTextCodec::codecForName("EUC-KR");
    QString text = codec ? codec->toUnicode(rawData) : QString::fromUtf8(rawData);


    // 서울 지역 코드
    const QString targetRegId =
        currentRegion.shortTermRegId;

    // offset별 낮 최고기온, 밤 최저기온, SKY 코드 저장
    // key: offset(1=내일, 2=모레, 3=D+3)
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

        // 끝에 '=' 제거
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
        int     offset     = today.daysTo(targetDate); // 0=오늘, 1=내일, ...

        if (offset < 0 || offset > 3)
            continue;  // D+0 ~ D+3 만 처리

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

    // ── 테이블 col 1~3 채우기 (D+1=col2, D+2=col3, D+3=col4) ──
    // col 1 = 오늘, col 2 = 내일(offset1), col 3 = 모레(offset2), col 4 = D+3(offset3)
    const QStringList weekNames = {"일","월","화","수","목","금","토"};

    for (int offset = 0; offset <= 3; offset++)
    {
        int col =1+offset; // col1=오늘, col2, col3, col4

        QDate targetDate = today.addDays(offset);
        QString dayName  = weekNames[targetDate.dayOfWeek() % 7];

        // 헤더 요일
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

        QString icon    = dd.skyIcon.isEmpty() ? "☁" : dd.skyIcon;
        QString maxStr;
        QString minStr;

        if(offset == 0 && todayMaxTemp != -999){
            maxStr=QString::number(todayMaxTemp)+"°";
        }
        else{
            maxStr=(dd.maxTemp != -999) ? QString::number(dd.maxTemp) + "°" : "-";
        }
        if(offset == 0 && todayMinTemp != -999)
        {
            minStr = QString::number(todayMinTemp) + "°";
        }
        else
        {
            minStr =
                (dd.minTemp != 999)
                ? QString::number(dd.minTemp) + "°"
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

// ─────────────────────────────────────────────
// 중기예보로 테이블 D+4 이후 채우기
// ─────────────────────────────────────────────
void WeatherPanel::tryFillMidForecast()
{
    if (midLandData.isEmpty() || midTempData.isEmpty())
        return;

    const QStringList weekNames = {"일","월","화","수","목","금","토"};
    QDate today = QDate::currentDate();

    // 중기예보는 D+4(offset=4) ~ D+10(offset=10) 제공
    // col 1=오늘(D+0), col 2=D+1, ..., col 5=D+4, col 6=D+5, col 7=D+6
    for (int offset = 4; offset <= 7; offset++)
    {
        int col = 1 + offset; // col 5~8 인데 테이블은 col 7까지라 범위 체크
        if (col >= ui->tableForecast->columnCount())
            break;

        QDate target = today.addDays(offset);
        QString dayName = weekNames[target.dayOfWeek() % 7];

        ui->tableForecast->setHorizontalHeaderItem(
            col, new QTableWidgetItem("  " + dayName));

        // 키 이름: taMax4, taMax5 ... (패딩 없음)
        int maxTemp = midTempData[QString("taMax%1").arg(offset)].toInt();
        int minTemp = midTempData[QString("taMin%1").arg(offset)].toInt();

        QString wfAm = midLandData[QString("wf%1Am").arg(offset)].toString();
        QString wfPm = midLandData[QString("wf%1Pm").arg(offset)].toString();
        // D+8 이상은 Am/Pm 없이 wf8, wf9... 형태
        if (wfAm.isEmpty())
            wfAm = midLandData[QString("wf%1").arg(offset)].toString();
        if (wfPm.isEmpty())
            wfPm = wfAm;

        QString weather = wfPm;
        if (wfAm.contains("비") || wfAm.contains("눈"))
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
        setCell(1, col, QString::number(maxTemp) + "°");
        setCell(2, col, QString::number(minTemp) + "°");
    }
}

//오늘의 테이블 체우기
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


// ─────────────────────────────────────────────
// 밝기 적용
// ─────────────────────────────────────────────
void WeatherPanel::setTextBrightness(int value)
{
    QString color = QString("rgb(%1,%1,%1)").arg(value);

    this->setStyleSheet(
        QString(
            "QWidget {"
            "background:rgba(30,30,30,220);"
            "border:2px solid %1;"
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
    case 1: // 서울
        currentRegion =
        {
            "서울",
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
            "부산",
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
            "대구",
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
            "인천",
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
            "광주",
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
            "대전",
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
            "울산",
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
            "제주",
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
        "현재 위치 : " + currentRegion.name);

    requestWeather();
    requestDailyForecast();
    requestShortTerm();
    requestMidLand();
    requestMidTemp();
}
