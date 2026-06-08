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
    path.addRoundedRect(rect(), 28, 28);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#f5f5f3"));
    painter.drawPath(path);
}

// ── 스타일 ───────────────────────────────────────────
void MusicBar::applyStyles()
{
    setAttribute(Qt::WA_StyledBackground, false);

    ui->lblMusicIcon->setStyleSheet(
        "color:#1a1a1a; font-size:22pt; background:transparent;"
    );

    ui->lblTrackTitle->setStyleSheet(
        "color:#111111; font-size:15pt; font-weight:700;"
        "letter-spacing:2px; background:transparent;"
        "font-family:'Futura','Trebuchet MS','Franklin Gothic Medium',sans-serif;"
    );

    ui->lblBarsIcon->setStyleSheet(
        "color:#888888; font-size:16pt; background:transparent;"
    );

    // waveContainer는 반드시 배경 투명 + 고정 높이 110px
    ui->waveContainer->setStyleSheet("background:transparent;");
    ui->waveContainer->setFixedHeight(110);

    QString timeStyle =
        "color:#1a1a1a; font-size:13pt; font-weight:600;"
        "background:transparent;"
        "font-family:'Courier New','Consolas',monospace;";
    ui->lblCurrentTime->setStyleSheet(timeStyle);
    ui->lblTotalTime->setStyleSheet(timeStyle);

    ui->progressSlider->setStyleSheet(R"(
        QSlider { height: 28px; }

        QSlider::groove:horizontal {
            height: 3px;
            background: #d0d0ce;
            border-radius: 2px;
            margin: 0 1px;
        }
        QSlider::sub-page:horizontal {
            background: #2a2a2a;
            border-radius: 2px;
        }
        QSlider::add-page:horizontal {
            background: #d0d0ce;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -6px 0;
            border-radius: 7px;
            background: #ffffff;
            border: 2px solid #f5f5f3;
        }
        QSlider::handle:horizontal:hover {
            width: 18px;
            height: 18px;
            margin: -8px 0;
            border-radius: 9px;
            background: #ffffff;
            border: 2px solid #f5f5f3;
        }
        QSlider::handle:horizontal:pressed {
            background: #000000;
            border: 2px solid #e8e8e6;
        }
    )");

    QString sideBtn = R"(
        QToolButton {
            border: none; font-size: 18pt; font-weight: 300;
            background: transparent; color: #444444;
            border-radius: 12px; padding: 2px 12px; letter-spacing: -2px;
        }
        QToolButton:hover   { background: rgba(0,0,0,0.07); color: #111111; }
        QToolButton:pressed { background: rgba(0,0,0,0.14); }
    )";
    ui->btnPrev->setStyleSheet(sideBtn);
    ui->btnNext->setStyleSheet(sideBtn);

    ui->btnPlay->setStyleSheet(R"(
        QToolButton {
            border: none; font-size: 22pt;
            background: transparent; color: #111111;
            border-radius: 14px; padding: 2px 14px;
        }
        QToolButton:hover   { background: rgba(0,0,0,0.09); }
        QToolButton:pressed { background: rgba(0,0,0,0.18); }
    )");
}
