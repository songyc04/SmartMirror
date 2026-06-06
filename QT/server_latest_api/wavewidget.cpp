#include "wavewidget.h"

#include <QPainter>
#include <cstdlib>
#include <QDebug>

WaveWidget::WaveWidget(QWidget *parent)
    : QWidget(parent),
      mPlaying(false)
{
    //스타일
    setMinimumHeight(120);
    setMinimumHeight(120);
    mBars.resize(24);

    connect(&mTimer,
            &QTimer::timeout,
            this,
            [this]()
    {
        if(mPlaying)
        {
            for(int i=0;i<mBars.size();i++)
            {
                mBars[i] = 20 + (rand() % 80);
            }

            update();
        }
    });

    mTimer.start(100);
}

void WaveWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    int count = mBars.size();

    int barWidth = width() / count;

    for(int i=0;i<count;i++)
    {
        int h = mBars[i];

        QRect rect(
                    i * barWidth + 2,
                    height()/2 - h/2,
                    barWidth - 4,
                    h
                    );

        painter.drawRoundedRect(rect,4,4);
    }
}

void WaveWidget::setPlaying(bool playing)
{
    mPlaying = playing;

    if(!mPlaying)
    {
        for(int i=0;i<mBars.size();i++)
        {
            mBars[i] = 8;
        }
    }

    update();
}
