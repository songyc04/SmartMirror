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

    // 유휴 상태: 작은 기본 높이
    for (int i = 0; i < BAR_COUNT; i++) {
        mBars[i]    = 0.12;
        mTargets[i] = 0.12;
    }

    // 애니메이션 타이머: 40ms마다 갱신 + 부드러운 보간
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        if (mPlaying) {
            // 일정 간격으로 새 타겟 생성
            static int tickCount = 0;
            if (++tickCount % 2 == 0)
                randomizeTargets();
        }
        animateBars();
        update();
    });
    mTimer.start(40);   // 25fps
}

// -- 랜덤 타겟 높이 생성 ---------------------
void WaveWidget::randomizeTargets()
{
    for (int i = 0; i < BAR_COUNT; i++) {
        qreal t = 0.15 + (qreal)(rand() % 85) / 100.0;
        mTargets[i] = t;
    }
}

// -- 현재 높이를 타겟으로 선형 보간 ---
void WaveWidget::animateBars()
{
    const qreal speed = mPlaying ? 0.25 : 0.15;
    for (int i = 0; i < BAR_COUNT; i++) {
        mBars[i] += (mTargets[i] - mBars[i]) * speed;
    }
}

// -- 재생/정지 상태 변경 ----------------------------
void WaveWidget::setPlaying(bool playing)
{
    mPlaying = playing;

    if (!mPlaying) {
        // 정지 시 모든 바를 작은 높이 타겟으로 설정
        for (int i = 0; i < BAR_COUNT; i++)
            mTargets[i] = 0.10 + (qreal)(i % 3) * 0.02;
    } else {
        randomizeTargets();
    }
}

// -- 그리기 ------------------------------------------
void WaveWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int W = width();
    int H = height();

    // 바 영역: 상단 58%, 간격: 4%, 반사: 하단 30%
    int barAreaH  = (int)(H * 0.58);
    int gapH      = (int)(H * 0.04);
    int reflAreaH = (int)(H * 0.30);
    int barTop    = 0;                         // 바 시작 Y (상단부터)
    int reflStart = barAreaH + gapH;           // 반사 시작 Y

    qreal barW    = (qreal)W / BAR_COUNT;
    qreal barGap  = qMax(1.0, barW * 0.18);
    qreal bw      = barW - barGap;

    for (int i = 0; i < BAR_COUNT; i++) {
        qreal cx = i * barW + barW * 0.5;
        qreal x  = cx - bw * 0.5;

        // -- 메인 바 --------------------------------------
        int bh = qMax(4, (int)(mBars[i] * barAreaH));
        int by = barTop + barAreaH - bh;

        QRectF barRect(x, by, bw, bh);
        QPainterPath barPath;
        barPath.addRoundedRect(barRect, 3, 3);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#d4d4d4"));
        p.drawPath(barPath);

        // -- 반사 바 (페이드 아웃) -------------------
        int rh = qMax(2, (int)(mBars[i] * reflAreaH * 0.5));
        QRectF reflRect(x, reflStart, bw, rh);

        // 위에서 아래로 페이드 그라데이션
        QLinearGradient grad(0, reflStart, 0, reflStart + rh);
        grad.setColorAt(0.0, QColor(212, 212, 212, 60));
        grad.setColorAt(1.0, QColor(212, 212, 212, 0));
        p.setBrush(grad);

        QPainterPath reflPath;
        reflPath.addRoundedRect(reflRect, 2, 2);
        p.drawPath(reflPath);
    }
}
