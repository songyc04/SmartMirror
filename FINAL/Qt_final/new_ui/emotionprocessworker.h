#ifndef EMOTIONPROCESSWORKER_H
#define EMOTIONPROCESSWORKER_H

#include <QObject>
#include <QProcess>

class EmotionProcessWorker : public QObject
{
    Q_OBJECT

public:
    explicit EmotionProcessWorker(QObject *parent = nullptr);
    ~EmotionProcessWorker();

public slots:
    void initialize();
    void startProcess();
    void stopProcess();
    void shutdown();

signals:
    void processOutput(const QString &output);
    void processError(const QString &error);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
};

#endif // EMOTIONPROCESSWORKER_H
