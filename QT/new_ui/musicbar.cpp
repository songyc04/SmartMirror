#include "musicbar.h"
#include "ui_musicbar.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QDebug>

MusicBar::MusicBar(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MusicBar),
      mIsPlaying(false),
      mCurrentSec(0),
      mTotalSec(0)
{
    ui->setupUi(this);

    // -- WaveWidget 삽입 ----------------------------
    mWave = new WaveWidget(this);
    QVBoxLayout *waveLayout = new QVBoxLayout(ui->waveContainer);
    waveLayout->setContentsMargins(0, 0, 0, 0);
    waveLayout->setSpacing(0);
    waveLayout->addWidget(mWave);

    // -- 기본 트랙 정보 표시 -------------------
    setTotalSeconds(mTotalSec);
    updateTimeLabels();

    ui->progressSlider->setValue(0);

    // -- 진행 타이머 -------------------------------
    mProgressTimer = new QTimer(this);
    mProgressTimer->setInterval(1000);
    connect(mProgressTimer, &QTimer::timeout, this, &MusicBar::onTimerTick);

    // -- 시그널 연결 ---------------------------
    connect(ui->btnPlay,        &QToolButton::clicked,  this, &MusicBar::onPlayClicked);
    connect(ui->btnPrev,        &QToolButton::clicked,  this, &MusicBar::onPrevClicked);
    connect(ui->btnNext,        &QToolButton::clicked,  this, &MusicBar::onNextClicked);
    connect(ui->progressSlider, &QSlider::sliderMoved,  this, &MusicBar::onSliderMoved);

    applyStyles();
}

MusicBar::~MusicBar()
{
    delete ui;
}

// -- 외부 API ------------------------------------
void MusicBar::setTrackTitle(const QString &title)
{
    ui->lblTrackTitle->setText(title);
}

void MusicBar::setTotalSeconds(int secs)
{
    mTotalSec = secs;
    ui->lblTotalTime->setText(formatTime(mTotalSec));
    ui->progressSlider->setMaximum(mTotalSec);
}

void MusicBar::setCurrentSeconds(int secs)
{
    mCurrentSec = secs;
    updateTimeLabels();
}

void MusicBar::setPlaying(bool playing)
{
    mIsPlaying = playing;
    mWave->setPlaying(mIsPlaying);

    if (mIsPlaying) {
        ui->btnPlay->setText("❚❚");
        mProgressTimer->start();
    } else {
        ui->btnPlay->setText("▷");
        mProgressTimer->stop();
    }
}

void MusicBar::resetPlay(){
    setPlaying(false);
    mCurrentSec=0;
    mTotalSec=0;
    ui->progressSlider->setMaximum(0);
    ui->progressSlider->setValue(0);
    ui->lblTrackTitle->setText("");
    ui->lblCurrentTime->setText("0:00");
    ui->lblTotalTime->setText("0:00");
}

// -- 슬롯 -------------------------------------------
void MusicBar::onPlayClicked()
{
    setPlaying(!mIsPlaying);
    emit playClicked();
}

void MusicBar::onPrevClicked() { emit prevClicked(); }
void MusicBar::onNextClicked() { emit nextClicked(); }

void MusicBar::onSliderMoved(int value)
{
    mCurrentSec = value;
    updateTimeLabels();
    emit seeked(value);
}

void MusicBar::onTimerTick()
{
    if (mCurrentSec < mTotalSec) {
        mCurrentSec++;
        updateTimeLabels();
    } else {
        setPlaying(false);
        mCurrentSec = 0;
        updateTimeLabels();
        ui->progressSlider->setValue(0);
    }
}

// -- 헬퍼 ------------------------------------------
void MusicBar::updateTimeLabels()
{
    ui->lblCurrentTime->setText(formatTime(mCurrentSec));
    if (mTotalSec > 0)
        ui->progressSlider->setValue(mCurrentSec);
}

QString MusicBar::formatTime(int secs) const
{
    int m = secs / 60;
    int s = secs % 60;
    return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

// -- paintEvent: 배경 없음 (투명) ------------
void MusicBar::paintEvent(QPaintEvent *)
{
    // 투명 배경
}

// -- 스타일 (골드/앰버 테마) ---------------------------------------------
void MusicBar::applyStyles()
{
    setAttribute(Qt::WA_StyledBackground, false);

    ui->lblMusicIcon->setStyleSheet(
        "color:#C9A96E; font-size:18pt; background:transparent;"
    );

    ui->lblTrackTitle->setStyleSheet(
        "color:#ffffff; font-size:13pt; font-weight:400;"
        "letter-spacing:0.5px; background:transparent;"
    );

    ui->lblBarsIcon->setStyleSheet(
        "color:#C9A96E; font-size:14pt; background:transparent;"
    );

    ui->waveContainer->setStyleSheet("background:transparent;");
    ui->waveContainer->setFixedHeight(50);

    QString timeStyle =
        "color:#888888; font-size:11pt; font-weight:400;"
        "background:transparent;";
    ui->lblCurrentTime->setStyleSheet(timeStyle);
    ui->lblTotalTime->setStyleSheet(timeStyle);

    ui->progressSlider->setStyleSheet(R"(
        QSlider { height: 16px; }

        QSlider::groove:horizontal {
            height: 2px;
            background: rgba(255,255,255,0.12);
            border-radius: 1px;
        }
        QSlider::sub-page:horizontal {
            height: 2px;
            background: #C9A96E;
            border-radius: 1px;
        }
        QSlider::add-page:horizontal {
            height: 2px;
            background: rgba(255,255,255,0.10);
            border-radius: 1px;
        }
        QSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            margin: -6px 0;
            border-radius: 6px;
            background: #C9A96E;
            border: none;
        }
        QSlider::handle:horizontal:hover {
            background: #dfc08a;
            border: none;
        }
        QSlider::handle:horizontal:pressed {
            background: #b8944f;
            border: none;
        }
    )");

    QString sideBtn = R"(
        QToolButton {
            border: none; font-size: 16pt; font-weight: 300;
            background: transparent; color: #cccccc;
            border-radius: 8px; padding: 2px 12px;
        }
        QToolButton:hover   { background: rgba(255,255,255,0.06); color: #ffffff; }
        QToolButton:pressed { background: rgba(255,255,255,0.10); }
    )";
    ui->btnPrev->setStyleSheet(sideBtn);
    ui->btnNext->setStyleSheet(sideBtn);

    ui->btnPlay->setStyleSheet(R"(
        QToolButton {
            border: 2px solid #C9A96E;
            font-size: 14pt;
            background: transparent;
            color: #C9A96E;
            border-radius: 22px;
            padding: 4px 16px;
        }
        QToolButton:hover   { background: rgba(201,169,110,0.12); color: #dfc08a; }
        QToolButton:pressed { background: rgba(201,169,110,0.22); }
    )");
}
