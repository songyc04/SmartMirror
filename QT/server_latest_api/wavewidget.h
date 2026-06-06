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
    QVector<int> mBars;
    QTimer mTimer;

    bool mPlaying;
};

#endif
