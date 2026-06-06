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
    ui->tableForecast->setColumnWidth(0, 90);
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

    // ── API 요청 시작 ────────────────────────
    requestWeather();
    requestMidLand();
    requestMidTemp();
    requestShortTerm();
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
            "&base_date=%1&base_time=%2"
            "&nx=60&ny=127"
            "&authKey=umH6GTnpT9-h-hk56Z_fKA")
            .arg(baseDate)
            .arg(baseTime);

    qDebug() << "초단기실황 URL:" << url;
    manager->get(QNetworkRequest(QUrl(url)));
}

// ─────────────────────────────────────────────
// 중기 하늘상태 요청
// ─────────────────────────────────────────────
void WeatherPanel::requestMidLand()
{
    QString tmFc = QDate::currentDate().toString("yyyyMMdd");
    tmFc += (QTime::currentTime().hour() >= 18) ? "1800" : "0600";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "MidFcstInfoService/getMidLandFcst?"
            "pageNo=1&numOfRows=10&dataType=JSON"
            "&regId=11B00000&tmFc=%1"
            "&authKey=umH6GTnpT9-h-hk56Z_fKA")
            .arg(tmFc);

    qDebug() << "MidLand URL:" << url;
    midLandManager->get(QNetworkRequest(QUrl(url)));
}

// ─────────────────────────────────────────────
// 중기 기온 요청
// ─────────────────────────────────────────────
void WeatherPanel::requestMidTemp()
{
    QString tmFc = QDate::currentDate().toString("yyyyMMdd");
    tmFc += (QTime::currentTime().hour() >= 18) ? "1800" : "0600";

    QString url =
        QString(
            "https://apihub.kma.go.kr/api/typ02/openApi/"
            "MidFcstInfoService/getMidTa?"
            "pageNo=1&numOfRows=10&dataType=JSON"
            "&regId=11B10101&tmFc=%1"
            "&authKey=umH6GTnpT9-h-hk56Z_fKA")
            .arg(tmFc);

    qDebug() << "MidTemp URL:" << url;
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

    qDebug() << "ShortTerm URL:" << url;
    shortTermManager->get(QNetworkRequest(QUrl(url)));
}

// ─────────────────────────────────────────────
// 초단기실황 응답 처리 (현재 기온/날씨 패널)
// ─────────────────────────────────────────────
void WeatherPanel::onWeatherReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    qDebug() << "초단기실황 응답:" << data;

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
        QString("습도 %1%%   강수 %2mm   풍속 %3m/s")
            .arg(humidity).arg(rain).arg(wind));
    ui->labelLocation->setText("현재 위치 : 서울");

    reply->deleteLater();
}

// ─────────────────────────────────────────────
// 중기 하늘상태 응답 처리
// ─────────────────────────────────────────────
void WeatherPanel::onMidLandReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    qDebug() << "MidLand 응답:" << data;

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
    qDebug() << "MidTemp 응답:" << data;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        qDebug() << "MidTemp JSON 파싱 실패";
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
        qDebug() << "MidTemp 아이템 없음";
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

    qDebug() << "ShortTerm 응답 (첫 500자):" << text.left(500);

    // 서울 지역 코드
    const QString targetRegId = "11B10101";

    // offset별 낮 최고기온, 밤 최저기온, SKY 코드 저장
    // key: offset(1=내일, 2=모레, 3=D+3)
    struct DayData {
        int    maxTemp  = -999;
        int    minTemp  = -999;
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

        if (offset < 1 || offset > 3)
            continue;  // D+1 ~ D+3 만 처리

        QString skyCode = cols[14].trimmed(); // SKY
        int     ta      = cols[12].trimmed().toInt(); // TA

        DayData &dd = dayMap[offset];

        // 낮(12시) → 최고기온 + 아이콘
        if (hour == 12)
        {
            if (ta != -99)
                dd.maxTemp = ta;
            if (dd.skyIcon.isEmpty())
                dd.skyIcon = skyCodeToIcon(skyCode);
        }
        // 자정(00시) → 최저기온
        else if (hour == 0)
        {
            if (ta != -99)
                dd.minTemp = ta;
            // 아이콘이 아직 없으면 밤 SKY로 대체
            if (dd.skyIcon.isEmpty())
                dd.skyIcon = skyCodeToIcon(skyCode);
        }
    }

    // ── 테이블 col 1~3 채우기 (D+1=col2, D+2=col3, D+3=col4) ──
    // col 1 = 오늘, col 2 = 내일(offset1), col 3 = 모레(offset2), col 4 = D+3(offset3)
    const QStringList weekNames = {"일","월","화","수","목","금","토"};

    for (int offset = 1; offset <= 3; offset++)
    {
        int col = 1 + offset; // col2, col3, col4

        QDate targetDate = today.addDays(offset);
        QString dayName  = weekNames[targetDate.dayOfWeek() % 7];

        // 헤더 요일
        ui->tableForecast->setHorizontalHeaderItem(
            col, new QTableWidgetItem("  " + dayName));

        if (!dayMap.contains(offset))
        {
            ui->tableForecast->setItem(0, col, new QTableWidgetItem(""));
            ui->tableForecast->setItem(1, col, new QTableWidgetItem("-"));
            ui->tableForecast->setItem(2, col, new QTableWidgetItem("-"));
            continue;
        }

        const DayData &dd = dayMap[offset];

        QString icon    = dd.skyIcon.isEmpty() ? "☁" : dd.skyIcon;
        QString maxStr  = (dd.maxTemp != -999) ? QString::number(dd.maxTemp) + "°" : "-";
        QString minStr  = (dd.minTemp != -999) ? QString::number(dd.minTemp) + "°" : "-";

        ui->tableForecast->setItem(0, col, new QTableWidgetItem(icon));
        ui->tableForecast->setItem(1, col, new QTableWidgetItem(maxStr));
        ui->tableForecast->setItem(2, col, new QTableWidgetItem(minStr));

        qDebug() << "ShortTerm col" << col << "D+" << offset
                 << targetDate.toString("MM/dd") << dayName
                 << "max:" << maxStr << "min:" << minStr << "icon:" << icon;
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

    // ── 다음주 수요일 계산 ───────────────────
    int todayDow       = today.dayOfWeek();
    int daysToThisWed  = (3 - todayDow + 7) % 7;
    int daysToNextWed  = (daysToThisWed == 0) ? 7 : (daysToThisWed + 7);

    // col 계산: 오늘=col1, 내일=col2 ... 오늘로부터 N일 후=col(N+1)
    // D+1~D+3 은 onShortTermReply 에서 채우므로 D+4(col5)부터 처리
    for (int offset = 4; offset <= daysToNextWed && offset < 7; offset++)
    {
        QDate target = today.addDays(offset);
        int   col    = 1 + offset;

        QString dayName = weekNames[target.dayOfWeek() % 7];

        ui->tableForecast->setHorizontalHeaderItem(
            col, new QTableWidgetItem("  " + dayName));

        int maxTemp = midTempData[QString("taMax%1").arg(offset)].toInt();
        int minTemp = midTempData[QString("taMin%1").arg(offset)].toInt();

        if (maxTemp == 0 && minTemp == 0)
        {
            ui->tableForecast->setItem(0, col, new QTableWidgetItem(""));
            ui->tableForecast->setItem(1, col, new QTableWidgetItem("-"));
            ui->tableForecast->setItem(2, col, new QTableWidgetItem("-"));
            continue;
        }

        QString wfAm = midLandData[QString("wf%1Am").arg(offset)].toString();
        QString wfPm = midLandData[QString("wf%1Pm").arg(offset)].toString();
        if (wfAm.isEmpty())
            wfAm = midLandData[QString("wf%1").arg(offset)].toString();
        if (wfPm.isEmpty())
            wfPm = wfAm;

        QString weather = wfPm;
        if (wfAm.contains("비") || wfAm.contains("눈"))
            weather = wfAm;
        if (weather.isEmpty()) weather = wfAm;

        QString icon = weatherToIcon(weather);

        ui->tableForecast->setItem(0, col, new QTableWidgetItem(icon));
        ui->tableForecast->setItem(1, col, new QTableWidgetItem(
            QString::number(maxTemp) + "°"));
        ui->tableForecast->setItem(2, col, new QTableWidgetItem(
            QString::number(minTemp) + "°"));

        qDebug() << "MidForecast col" << col << "D+" << offset
                 << target.toString("MM/dd") << dayName
                 << "max:" << maxTemp << "min:" << minTemp;
    }

    // 오늘(col1) 헤더 설정
    QString todayName = weekNames[today.dayOfWeek() % 7];
    ui->tableForecast->setHorizontalHeaderItem(
        1, new QTableWidgetItem("  " + todayName));
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
