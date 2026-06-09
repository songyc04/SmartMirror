#include "wavewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <cstdlib>
#include <cmath>

WaveWidget::WaveWidget(QWidget *parent)
    : QWidget(parent),
      mPlaying(false)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumHeight(50);
    setMaximumHeight(50);

    mBars.resize(BAR_COUNT);
    mTargets.resize(BAR_COUNT);

    // 정지 상태: 작은 기본 높이
    for (int i = 0; i < BAR_COUNT; i++) {
        mBars[i]    = 0.12;
        mTargets[i] = 0.12;
    }

    // 애니메이션 타이머: 80ms 마다 목표 갱신 + 부드러운 보간
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        if (mPlaying) {
            // 일정 간격으로 새 목표 생성
            static int tickCount = 0;
            if (++tickCount % 2 == 0)
                randomizeTargets();
        }
        animateBars();
        update();
    });
    mTimer.start(40);   // 25fps
}

// ── 목표 높이 랜덤 생성 ──────────────────────────────
void WaveWidget::randomizeTargets()
{
    for (int i = 0; i < BAR_COUNT; i++) {
        qreal t = 0.15 + (qreal)(rand() % 85) / 100.0;
        mTargets[i] = t;
    }
}

// ── Lerp으로 현재 높이를 목표로 보간 ────────────────
void WaveWidget::animateBars()
{
    const qreal speed = mPlaying ? 0.25 : 0.15;
    for (int i = 0; i < BAR_COUNT; i++) {
        mBars[i] += (mTargets[i] - mBars[i]) * speed;
    }
}

// ── 재생/정지 상태 변경 ──────────────────────────────
void WaveWidget::setPlaying(bool playing)
{
    mPlaying = playing;

    if (!mPlaying) {
        // 정지 시 모든 바를 작은 높이로 목표 설정
        for (int i = 0; i < BAR_COUNT; i++)
            mTargets[i] = 0.10 + (qreal)(i % 3) * 0.02;
    } else {
        randomizeTargets();
    }
}

// ── 그리기 ──────────────────────────────────────────
void WaveWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int W = width();
    int H = height();

    // 바 영역: 상단 60%, 반사: 하단 30%, 중앙 갭: 10%
    int barAreaH  = (int)(H * 0.58);
    int gapH      = (int)(H * 0.04);
    int reflAreaH = (int)(H * 0.30);
    int barTop    = 0;                         // 바 시작 Y (상단 기준)
    int reflStart = barAreaH + gapH;           // 반사 시작 Y

    qreal barW    = (qreal)W / BAR_COUNT;
    qreal barGap  = qMax(1.0, barW * 0.18);
    qreal bw      = barW - barGap;

    for (int i = 0; i < BAR_COUNT; i++) {
        qreal cx = i * barW + barW * 0.5;
        qreal x  = cx - bw * 0.5;

        // ── 메인 바 ──────────────────────────────────
        int bh = qMax(4, (int)(mBars[i] * barAreaH));
        int by = barTop + barAreaH - bh;

        QRectF barRect(x, by, bw, bh);
        QPainterPath barPath;
        barPath.addRoundedRect(barRect, 3, 3);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#e0e0e0"));
        p.drawPath(barPath);

        // ── 반사 바 (페이드 아웃) ────────────────────
        int rh = qMax(2, (int)(mBars[i] * reflAreaH * 0.5));
        QRectF reflRect(x, reflStart, bw, rh);

        // 위에서 아래로 투명해지는 그라디언트
        QLinearGradient grad(0, reflStart, 0, reflStart + rh);
        grad.setColorAt(0.0, QColor(220, 220, 220, 70));
        grad.setColorAt(1.0, QColor(220, 220, 220, 0));
        p.setBrush(grad);

        QPainterPath reflPath;
        reflPath.addRoundedRect(reflRect, 2, 2);
        p.drawPath(reflPath);
    }
}
