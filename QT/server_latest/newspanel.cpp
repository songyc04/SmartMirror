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

    ui->newsContainer->setGeometry(0,80,740,300);

    manager = new QNetworkAccessManager(this);

    connect(manager,
            &QNetworkAccessManager::finished,
            this,
            &NewsPanel::onNewsReply);

    //--------------------------------
    // 전체 스타일
    //--------------------------------

    this->setStyleSheet(
        "QWidget {"
        "background:rgba(20,20,20,230);"
        "border-radius:25px;"
        "color:white;"
        "}"
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
        "?query=속보" //키워드 설정
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

    mainLayout->setSpacing(3);

    mainLayout->setContentsMargins(0,0,0,0);

    for(int i = 0; i < 3; i++)
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
        card->setFixedHeight(70);

        card->setStyleSheet(
            "background:rgba(255,255,255,0.05);"
            "border-radius:20px;"
        );

        QHBoxLayout *cardLayout =
            new QHBoxLayout(card);

        cardLayout->setContentsMargins(15,5,15,5);
        cardLayout->setSpacing(15);

        //--------------------------------
        // 번호
        //--------------------------------

        QLabel *num =
            new QLabel(
                QString("0%1").arg(i+1));

        num->setFixedWidth(55);

        num->setStyleSheet(
            "font-size:28px;"
            "font-weight:bold;"
            "background:transparent;"
        );

        //--------------------------------
        // 텍스트
        //--------------------------------

        QVBoxLayout *textLayout =
            new QVBoxLayout();

        textLayout->setContentsMargins(0,0,0,0);
        textLayout->setSpacing(3);

        QLabel *titleLabel =
            new QLabel(title);

        titleLabel->setMaximumWidth(560);

        titleLabel->setWordWrap(true);

        titleLabel->setStyleSheet(
            "font-size:16px;"
            "font-weight:bold;"
            "background:transparent;"
        );

        titleLabel->setFixedHeight(42);

        QLabel *descLabel =
            new QLabel(desc);

        descLabel->setWordWrap(false);

        descLabel->setMaximumWidth(560);

        descLabel->setFixedHeight(18);

        descLabel->setMaximumHeight(20);

        descLabel->setStyleSheet(
            "font-size:11px;"
            "background:transparent;"
        );

        textLayout->addWidget(titleLabel);
        textLayout->addWidget(descLabel);

        //--------------------------------
        // 배치
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
            "background:rgba(20,20,20,230);"
            "border-radius:25px;"
            "color:%1;"
            "}").arg(color));
}
