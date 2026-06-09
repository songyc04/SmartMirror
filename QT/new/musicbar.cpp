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
      mTotalSec(672)
{
    ui->setupUi(this);

    // ── WaveWidget 삽입 ──────────────────────────────
    mWave = new WaveWidget(this);
    QVBoxLayout *waveLayout = new QVBoxLayout(ui->waveContainer);
    waveLayout->setContentsMargins(0, 0, 0, 0);
    waveLayout->setSpacing(0);
    waveLayout->addWidget(mWave);

    // ── 기본 트랙 정보 표시 ──────────────────────────
    setTotalSeconds(mTotalSec);
    updateTimeLabels();

    ui->progressSlider->setValue(0);

    // ── 진행 타이머 ──────────────────────────────────
    mProgressTimer = new QTimer(this);
    mProgressTimer->setInterval(1000);
    connect(mProgressTimer, &QTimer::timeout, this, &MusicBar::onTimerTick);

    // ── 시그널 연결 ──────────────────────────────────
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

// ── 외부 API ────────────────────────────────────────
void MusicBar::setTrackTitle(const QString &title)
{
    ui->lblTrackTitle->setText(title.toUpper());
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
        ui->btnPlay->setText("⏸");
        mProgressTimer->start();
    } else {
        ui->btnPlay->setText("▶");
        mProgressTimer->stop();
    }
}

// ── 슬롯 ────────────────────────────────────────────
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
        // ── 곡 종료: 정지 + 슬라이더/시간 처음으로 리셋 ──
        setPlaying(false);
        mCurrentSec = 0;
        updateTimeLabels();          // lblCurrentTime → "0:00", slider → 0
        ui->progressSlider->setValue(0);
    }
}

// ── 헬퍼 ────────────────────────────────────────────
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

// ── paintEvent: 둥근 카드 배경 ──────────────────────
void MusicBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 24, 24);

    painter.setPen(QPen(QColor(255, 255, 255, 22), 1));
    painter.setBrush(QColor(20, 20, 28, 210));
    painter.drawPath(path);
}

// ── 스타일 ───────────────────────────────────────────
void MusicBar::applyStyles()
{
    setAttribute(Qt::WA_StyledBackground, false);

    ui->lblMusicIcon->setStyleSheet(
        "color:#e8e8e8; font-size:20pt; background:transparent;"
    );

    ui->lblTrackTitle->setStyleSheet(
        "color:#ffffff; font-size:14pt; font-weight:600;"
        "letter-spacing:1px; background:transparent;"
        "font-family:'Futura','Trebuchet MS','Franklin Gothic Medium',sans-serif;"
    );

    ui->lblBarsIcon->setStyleSheet(
        "color:#777777; font-size:14pt; background:transparent;"
    );

    ui->waveContainer->setStyleSheet("background:transparent;");
    ui->waveContainer->setFixedHeight(100);

    QString timeStyle =
        "color:#aaaaaa; font-size:12pt; font-weight:500;"
        "background:transparent;"
        "font-family:'Courier New','Consolas',monospace;";
    ui->lblCurrentTime->setStyleSheet(timeStyle);
    ui->lblTotalTime->setStyleSheet(timeStyle);

    ui->progressSlider->setStyleSheet(R"(
        QSlider { height: 24px; }

        QSlider::groove:horizontal {
            height: 3px;
            background: rgba(255,255,255,0.12);
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: rgba(255,255,255,0.55);
            border-radius: 2px;
        }
        QSlider::add-page:horizontal {
            background: rgba(255,255,255,0.10);
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 13px;
            height: 13px;
            margin: -6px 0;
            border-radius: 7px;
            background: #ffffff;
            border: 2px solid rgba(255,255,255,0.20);
        }
        QSlider::handle:horizontal:hover {
            width: 16px;
            height: 16px;
            margin: -7px 0;
            border-radius: 8px;
            background: #ffffff;
            border: 2px solid rgba(255,255,255,0.35);
        }
        QSlider::handle:horizontal:pressed {
            background: #eeeeee;
            border: 2px solid rgba(255,255,255,0.25);
        }
    )");

    QString sideBtn = R"(
        QToolButton {
            border: none; font-size: 17pt; font-weight: 300;
            background: transparent; color: #bbbbbb;
            border-radius: 10px; padding: 3px 12px;
        }
        QToolButton:hover   { background: rgba(255,255,255,0.10); color: #ffffff; }
        QToolButton:pressed { background: rgba(255,255,255,0.16); }
    )";
    ui->btnPrev->setStyleSheet(sideBtn);
    ui->btnNext->setStyleSheet(sideBtn);

    ui->btnPlay->setStyleSheet(R"(
        QToolButton {
            border: none; font-size: 20pt;
            background: rgba(255,255,255,0.08); color: #ffffff;
            border-radius: 12px; padding: 3px 14px;
        }
        QToolButton:hover   { background: rgba(255,255,255,0.16); }
        QToolButton:pressed { background: rgba(255,255,255,0.24); }
    )");
}
