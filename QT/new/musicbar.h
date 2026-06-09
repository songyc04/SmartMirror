#ifndef MUSICBAR_H
#define MUSICBAR_H

#include <QWidget>
#include <QTimer>
#include "wavewidget.h"

namespace Ui {
class MusicBar;
}

class MusicBar : public QWidget
{
    Q_OBJECT

public:
    explicit MusicBar(QWidget *parent = nullptr);
    ~MusicBar();

    // Set track info from outside
    void setTrackTitle(const QString &title);
    void setTotalSeconds(int secs);
    void setCurrentSeconds(int secs);

    // Playback control
    void setPlaying(bool playing);
    bool isPlaying() const { return mIsPlaying; }

    void onTimerTick();

signals:
    void playClicked();
    void prevClicked();
    void nextClicked();
    void seeked(int seconds);   // When slider is moved

private slots:
    void onPlayClicked();
    void onPrevClicked();
    void onNextClicked();
    void onSliderMoved(int value);


protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Ui::MusicBar    *ui;
    WaveWidget      *mWave;
    QTimer          *mProgressTimer;

    bool    mIsPlaying;
    int     mCurrentSec;
    int     mTotalSec;

    void    updateTimeLabels();
    QString formatTime(int secs) const;
    void    applyStyles();
};

#endif // MUSICBAR_H
