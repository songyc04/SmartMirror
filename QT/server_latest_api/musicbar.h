#ifndef MUSICBAR_H
#define MUSICBAR_H

#include <QWidget>
#include "wavewidget.h"

namespace Ui {
class MusicBar;
}

class MusicBar : public QWidget
{
    Q_OBJECT

public:
    explicit MusicBar(QWidget *parent = 0);
    ~MusicBar();

private slots:
    void onPlayClicked();

private:
    Ui::MusicBar *ui;
    WaveWidget *mWave;

    bool mIsPlaying;
};

#endif // MUSICBAR_H
