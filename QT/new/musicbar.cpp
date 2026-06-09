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
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 18, 18);

    painter.setPen(QPen(QColor(142, 205, 247, 30), 1));
    painter.setBrush(QColor(15, 18, 25, 220));
    painter.drawPath(path);
}

// ── 스타일 ───────────────────────────────────────────
void MusicBar::applyStyles()
{
    setAttribute(Qt::WA_StyledBackground, false);

    ui->lblMusicIcon->setStyleSheet(
        "color:#8ecdf7; font-size:16pt; background:transparent;"
    );

    ui->lblTrackTitle->setStyleSheet(
        "color:#ffffff; font-size:12pt; font-weight:600;"
        "letter-spacing:1px; background:transparent;"
        "font-family:'Futura','Trebuchet MS','Franklin Gothic Medium',sans-serif;"
    );

    ui->lblBarsIcon->setStyleSheet(
        "color:#555555; font-size:11pt; background:transparent;"
    );

    ui->waveContainer->setStyleSheet("background:transparent;");
    ui->waveContainer->setFixedHeight(50);

    QString timeStyle =
        "color:#888888; font-size:10pt; font-weight:500;"
        "background:transparent;"
        "font-family:'Courier New','Consolas',monospace;";
    ui->lblCurrentTime->setStyleSheet(timeStyle);
    ui->lblTotalTime->setStyleSheet(timeStyle);

    ui->progressSlider->setStyleSheet(R"(
        QSlider { height: 16px; }

        QSlider::groove:horizontal {
            height: 2px;
            background: rgba(255,255,255,0.10);
            border-radius: 1px;
        }
        QSlider::sub-page:horizontal {
            height: 2px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #5b9bd5, stop:1 #8ecdf7);
            border-radius: 1px;
        }
        QSlider::add-page:horizontal {
            height: 2px;
            background: rgba(255,255,255,0.08);
            border-radius: 1px;
        }
        QSlider::handle:horizontal {
            width: 10px;
            height: 10px;
            margin: -5px 0;
            border-radius: 5px;
            background: #8ecdf7;
            border: none;
        }
        QSlider::handle:horizontal:hover {
            width: 14px;
            height: 14px;
            margin: -7px 0;
            border-radius: 7px;
            background: #ffffff;
            border: none;
        }
        QSlider::handle:horizontal:pressed {
            background: #5b9bd5;
            border: none;
        }
    )");

    QString sideBtn = R"(
        QToolButton {
            border: none; font-size: 13pt; font-weight: 300;
            background: transparent; color: #999999;
            border-radius: 8px; padding: 2px 8px;
        }
        QToolButton:hover   { background: rgba(255,255,255,0.08); color: #ffffff; }
        QToolButton:pressed { background: rgba(255,255,255,0.14); }
    )";
    ui->btnPrev->setStyleSheet(sideBtn);
    ui->btnNext->setStyleSheet(sideBtn);

    ui->btnPlay->setStyleSheet(R"(
        QToolButton {
            border: none; font-size: 16pt;
            background: rgba(142,205,247,0.12); color: #8ecdf7;
            border-radius: 10px; padding: 2px 12px;
        }
        QToolButton:hover   { background: rgba(142,205,247,0.22); color: #ffffff; }
        QToolButton:pressed { background: rgba(142,205,247,0.32); }
    )");
}
