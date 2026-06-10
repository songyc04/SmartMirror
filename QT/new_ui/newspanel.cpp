#include "newspanel.h"
#include "ui_newspanel.h"



#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

NewsPanel::NewsPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::NewsPanel)
{
    ui->setupUi(this);

    ui->newsContainer->setGeometry(0,80,860,480);

    manager = new QNetworkAccessManager(this);

    connect(manager,
            &QNetworkAccessManager::finished,
            this,
            &NewsPanel::onNewsReply);

    //--------------------------------
    // 전역 타일
    //--------------------------------

    this->setStyleSheet(
        "QWidget {"
        "background:transparent;"
        "color:white;"
        "}"
    );

    ui->labelTitle->setStyleSheet(
        "font-size:22px;"
        "font-weight:400;"
        "color:#ffffff;"
        "padding:0px;"
        "background:transparent;"
    );

    requestNews();
}

NewsPanel::~NewsPanel()
{
    delete ui;
}

void NewsPanel::requestNews()
{
    QString url =
        "https://openapi.naver.com/v1/search/news.json"
        "?query=뉴스"
        "&display=3"
        "&sort=date";

    QNetworkRequest request(url);

    request.setRawHeader(
        "X-Naver-Client-Id",
        "fHy8UMkEXRt92hUtj2gi");

    request.setRawHeader(
        "X-Naver-Client-Secret",
        "90QOjgV2bC");

    manager->get(request);
}

void NewsPanel::onNewsReply(
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

    QJsonArray items =
        obj["items"].toArray();

    //--------------------------------
    // 뉴스 카드 생성
    //--------------------------------

    QVBoxLayout *mainLayout =
        new QVBoxLayout(ui->newsContainer);

    mainLayout->setSpacing(10);

    mainLayout->setContentsMargins(0,0,0,0);
    int count = qMin(3, items.size());
    for(int i = 0; i < count; i++)
    {
        QJsonObject item =
            items[i].toObject();

        QString title =
            item["title"].toString();

        QString desc =
            item["description"].toString();

        //--------------------------------
        // HTML 태그 제거
        //--------------------------------

        title.remove("<b>");
        title.remove("</b>");

        desc.remove("<b>");
        desc.remove("</b>");

        //--------------------------------
        // 카드
        //--------------------------------

        QFrame *card =
            new QFrame();
        card->setFixedHeight(90);

        card->setStyleSheet(
            "background:rgba(255,255,255,0.03);"
            "border:1px solid rgba(201,169,110,0.15);"
            "border-radius:12px;"
        );

        QHBoxLayout *cardLayout =
            new QHBoxLayout(card);

        cardLayout->setContentsMargins(16,10,16,10);
        cardLayout->setSpacing(16);

        //--------------------------------
        // 번호 (원형 지)
        //--------------------------------

        QLabel *num =
            new QLabel(
                QString("%1").arg(i+1, 2, 10, QChar('0')));

        num->setFixedSize(44, 44);

        num->setStyleSheet(
            "font-size:18px;"
            "font-weight:500;"
            "color:#C9A96E;"
            "border:1.5px solid #C9A96E;"
            "border-radius:22px;"
            "background:transparent;"
        );
        num->setAlignment(Qt::AlignCenter);

        //--------------------------------
        // 텍스트
        //--------------------------------

        QVBoxLayout *textLayout =
            new QVBoxLayout();

        textLayout->setContentsMargins(0,0,0,0);
        textLayout->setSpacing(4);

        QLabel *titleLabel =
            new QLabel(title);

        titleLabel->setMaximumWidth(680);

        titleLabel->setWordWrap(true);

        titleLabel->setStyleSheet(
            "font-size:15px;"
            "font-weight:500;"
            "color:#ffffff;"
            "background:transparent;"
        );

        titleLabel->setFixedHeight(40);

        QLabel *descLabel =
            new QLabel(desc);

        descLabel->setWordWrap(true);

        descLabel->setMaximumWidth(680);

        descLabel->setFixedHeight(32);

        descLabel->setStyleSheet(
            "font-size:12px;"
            "color:#999999;"
            "background:transparent;"
        );

        textLayout->addWidget(titleLabel);
        textLayout->addWidget(descLabel);

        //--------------------------------
        // 레이아웃
        //--------------------------------

        cardLayout->addWidget(num);

        cardLayout->addLayout(textLayout,1);

        mainLayout->addWidget(card);
    }

    reply->deleteLater();
}

void NewsPanel::setTextBrightness(int value)
{
    QString color =
        QString("rgb(%1,%1,%1)")
            .arg(value);

    this->setStyleSheet(
        QString(
            "QWidget {"
            "background:transparent;"
            "color:%1;"
            "}").arg(color));
}
