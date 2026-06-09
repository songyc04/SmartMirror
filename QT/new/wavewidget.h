#ifndef WAVEWIDGET_H
#define WAVEWIDGET_H

#include <QWidget>
#include <QVector>
#include <QTimer>

class WaveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveWidget(QWidget *parent = nullptr);

    void setPlaying(bool playing);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<qreal> mBars;      // 각 바의 높이 (0.0 ~ 1.0)
    QVector<qreal> mTargets;   // 애니메이션 타겟 높이
    QTimer         mTimer;
    bool           mPlaying;

    static const int BAR_COUNT = 52;

    void  randomizeTargets();
    void  animateBars();
};

#endif // WAVEWIDGET_H
