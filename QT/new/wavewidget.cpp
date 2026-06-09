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

    // Idle state: small base height
    for (int i = 0; i < BAR_COUNT; i++) {
        mBars[i]    = 0.12;
        mTargets[i] = 0.12;
    }

    // Animation timer: update target every 80ms + smooth interpolation
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        if (mPlaying) {
            // Generate new targets at regular intervals
            static int tickCount = 0;
            if (++tickCount % 2 == 0)
                randomizeTargets();
        }
        animateBars();
        update();
    });
    mTimer.start(40);   // 25fps
}

// -- Generate random target heights ---------------------
void WaveWidget::randomizeTargets()
{
    for (int i = 0; i < BAR_COUNT; i++) {
        qreal t = 0.15 + (qreal)(rand() % 85) / 100.0;
        mTargets[i] = t;
    }
}

// -- Interpolate current height to target with Lerp ---
void WaveWidget::animateBars()
{
    const qreal speed = mPlaying ? 0.25 : 0.15;
    for (int i = 0; i < BAR_COUNT; i++) {
        mBars[i] += (mTargets[i] - mBars[i]) * speed;
    }
}

// -- Change play/stop state ----------------------------
void WaveWidget::setPlaying(bool playing)
{
    mPlaying = playing;

    if (!mPlaying) {
        // When stopped, set all bars to small height target
        for (int i = 0; i < BAR_COUNT; i++)
            mTargets[i] = 0.10 + (qreal)(i % 3) * 0.02;
    } else {
        randomizeTargets();
    }
}

// -- Drawing ------------------------------------------
void WaveWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int W = width();
    int H = height();

    // Bar area: top 60%, reflection: bottom 30%, center gap: 10%
    int barAreaH  = (int)(H * 0.58);
    int gapH      = (int)(H * 0.04);
    int reflAreaH = (int)(H * 0.30);
    int barTop    = 0;                         // Bar start Y (from top)
    int reflStart = barAreaH + gapH;           // Reflection start Y

    qreal barW    = (qreal)W / BAR_COUNT;
    qreal barGap  = qMax(1.0, barW * 0.18);
    qreal bw      = barW - barGap;

    for (int i = 0; i < BAR_COUNT; i++) {
        qreal cx = i * barW + barW * 0.5;
        qreal x  = cx - bw * 0.5;

        // -- Main bar --------------------------------------
        int bh = qMax(4, (int)(mBars[i] * barAreaH));
        int by = barTop + barAreaH - bh;

        QRectF barRect(x, by, bw, bh);
        QPainterPath barPath;
        barPath.addRoundedRect(barRect, 3, 3);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#e0e0e0"));
        p.drawPath(barPath);

        // -- Reflection bar (fade out) -------------------
        int rh = qMax(2, (int)(mBars[i] * reflAreaH * 0.5));
        QRectF reflRect(x, reflStart, bw, rh);

        // Gradient fading from top to bottom
        QLinearGradient grad(0, reflStart, 0, reflStart + rh);
        grad.setColorAt(0.0, QColor(220, 220, 220, 70));
        grad.setColorAt(1.0, QColor(220, 220, 220, 0));
        p.setBrush(grad);

        QPainterPath reflPath;
        reflPath.addRoundedRect(reflRect, 2, 2);
        p.drawPath(reflPath);
    }
}
