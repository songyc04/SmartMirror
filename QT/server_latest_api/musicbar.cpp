#include "musicbar.h"
#include "ui_musicbar.h"
#include <QDebug>

MusicBar::MusicBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MusicBar)
{
    ui->setupUi(this);

    mWave = new WaveWidget(this);

    QVBoxLayout *layout =
            new QVBoxLayout(ui->waveContainer);

    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(mWave);

    // MusicBar 전체 배경: 흰색, 둥근 모서리
    this->setStyleSheet(
        R"(
        MusicBar {
            background-color: #ffffff;
            border-radius: 20px;
        }
        )"
    );

    // 가운데 정렬
    this->setMaximumWidth(600);
    this->setMinimumWidth(400);
    this->setStyleSheet("background-color: white; border-radius: 20px;");

    //스타일
    QString style =
            R"(
            QToolButton{
                border:none;
                font-size:32px;
            }

            QToolButton:hover{
                background:#e0e0e0;
                border-radius:20px;
            }
            )";

    ui->btnPrev->setStyleSheet(style);
    ui->btnPlay->setStyleSheet(style);
    ui->btnNext->setStyleSheet(style);

    mIsPlaying = false;

    connect(
        ui->btnPlay,
        &QToolButton::clicked,
        this,
        &MusicBar::onPlayClicked
    );

    ui->btnPlay->setText("▶");
}

MusicBar::~MusicBar()
{
    delete ui;
}

void MusicBar::onPlayClicked()
{
    mIsPlaying = !mIsPlaying;

    mWave->setPlaying(mIsPlaying);

    if(mIsPlaying)
    {
        ui->btnPlay->setText("⏸");
    }
    else
    {
        ui->btnPlay->setText("▶");
    }
}
