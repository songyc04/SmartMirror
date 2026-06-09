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
    QVector<qreal> mBars;      // Height of each bar (0.0 ~ 1.0)
    QVector<qreal> mTargets;   // Animation target height
    QTimer         mTimer;
    bool           mPlaying;

    static const int BAR_COUNT = 52;

    void  randomizeTargets();
    void  animateBars();
};

#endif // WAVEWIDGET_H
