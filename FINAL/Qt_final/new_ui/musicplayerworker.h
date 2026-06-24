#ifndef MUSICPLAYERWORKER_H
#define MUSICPLAYERWORKER_H

#include <QObject>
#include <QProcess>

class MusicPlayerWorker : public QObject
{
    Q_OBJECT

public:
    explicit MusicPlayerWorker(QObject *parent = nullptr);
    ~MusicPlayerWorker();

public slots:
    void initialize();
    void searchAndPlay(const QString &keyword);
    void resume();
    void pause();
    void stop();
    void volumeUp();
    void volumeDown();
    void shutdown();

signals:
    void trackInfoReady(const QString &title, int durationSeconds);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void errorOccurred(const QString &message);
    void pythonOutput(const QString &output);

private slots:
    void onYtDlpFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onMpvReadyReadStandardOutput();
    void onMpvError(QProcess::ProcessError error);

private:
    void startMpvWithUrl(const QString &url);
    void sendMpvIpcCommand(const QString &jsonCommand);
    int  parseDuration(const QString &durationStr) const;

    QProcess *m_ytDlpProcess;
    QProcess *m_mpvProcess;

    QString m_keyword;
    int     m_searchCount;
    bool    m_isPlaying;
};

#endif // MUSICPLAYERWORKER_H
