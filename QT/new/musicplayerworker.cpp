#include "musicplayerworker.h"
#include <QDebug>
#include <QProcessEnvironment>
#include <QDateTime>

MusicPlayerWorker::MusicPlayerWorker(QObject *parent)
    : QObject(parent)
    , m_ytDlpProcess(nullptr)
    , m_mpvProcess(nullptr)
    , m_searchCount(5)
    , m_isPlaying(false)
{
}

MusicPlayerWorker::~MusicPlayerWorker()
{
}

void MusicPlayerWorker::initialize()
{
    m_ytDlpProcess = new QProcess(this);
    m_mpvProcess    = new QProcess(this);

    connect(m_mpvProcess, &QProcess::readyReadStandardOutput,
            this, &MusicPlayerWorker::onMpvReadyReadStandardOutput);
    connect(m_mpvProcess, &QProcess::errorOccurred,
            this, &MusicPlayerWorker::onMpvError);
}

void MusicPlayerWorker::searchAndPlay(const QString &keyword)
{
    if (!m_ytDlpProcess || !m_mpvProcess)
        return;

    m_keyword = keyword.isEmpty() ? QString("calm") : keyword;

    if (m_ytDlpProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "yt-dlp force killing previous process.";
        m_ytDlpProcess->kill();
        m_ytDlpProcess->waitForFinished(1000);
    }

    qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));

    int targetRank = (qrand() % 10) + 1;
    qDebug() << "Search keyword:" << m_keyword;
    qDebug() << "View count sort:" << targetRank << "th song extracted";

    QString cmd = QString("/home/jt-user/py310/bin/yt-dlp \"ytsearch%1:%2\" ")
                       .arg(m_searchCount)
                       .arg(m_keyword)
                   + "--match-filter \"duration <= 600\" "
                   + "--flat-playlist "
                   + "--print \"%(view_count)012d|%(url)s|%(title)s|%(duration)s\" "
                   + "| sort -t'|' -k1 -rn "
                   + "| head -n "
                   + QString::number(targetRank)
                   + " | tail -1";

    m_ytDlpProcess->disconnect();

    connect(m_ytDlpProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &MusicPlayerWorker::onYtDlpFinished);

    m_ytDlpProcess->start("sh", QStringList() << "-c" << cmd);
}

void MusicPlayerWorker::onYtDlpFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    disconnect(m_ytDlpProcess,
               QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
               this,
               &MusicPlayerWorker::onYtDlpFinished);

    if (exitCode != 0)
    {
        qWarning() << "yt-dlp URL extraction failed (exitCode:" << exitCode << ")";
        emit errorOccurred("URL extraction failed");
        return;
    }

    m_ytDlpProcess->readAllStandardError();

    QString rawOutput = QString::fromUtf8(m_ytDlpProcess->readAllStandardOutput()).trimmed();

    if (rawOutput.isEmpty())
    {
        qWarning() << "yt-dlp output is empty.";
        emit errorOccurred("Invalid URL");
        return;
    }

    QString outputLine = rawOutput.contains('\n')
        ? rawOutput.section('\n', -1, -1).trimmed()
        : rawOutput;

    QStringList fields = outputLine.split('|');

    QString audioUrl;
    QString videoTitle = "Unknown title";
    QString videoDuration = "0";
    int urlIdx = -1;

    for (int i = 1; i < fields.size(); i++)
    {
        if (fields[i].startsWith("https://"))
        {
            urlIdx = i;
            break;
        }
    }

    if (urlIdx > 0)
    {
        audioUrl = fields[urlIdx];
    }
    else
    {
        qWarning() << "Failed to obtain valid YouTube URL. Received data:" << outputLine;
        emit errorOccurred("Invalid URL");
        return;
    }

    if (urlIdx + 1 < fields.size() - 1)
    {
        QStringList titleParts;
        for (int i = urlIdx + 1; i < fields.size() - 1; i++)
            titleParts << fields[i];
        videoTitle = titleParts.join('|');
    }

    if (fields.size() > urlIdx + 1)
        videoDuration = fields.last();

    int durationInSeconds = parseDuration(videoDuration);

    qDebug() << "=========================";
    qDebug() << "URL:" << audioUrl;
    qDebug() << "Title:" << videoTitle;
    qDebug() << "Duration(raw):" << videoDuration << "->" << durationInSeconds << "sec";
    qDebug() << "=========================";

    qDebug() << "Top viewed URL extracted from" << m_searchCount << "results:" << audioUrl;

    emit trackInfoReady(videoTitle, durationInSeconds);

    startMpvWithUrl(audioUrl);
}

void MusicPlayerWorker::startMpvWithUrl(const QString &url)
{
    if (m_mpvProcess->state() != QProcess::NotRunning)
    {
        m_mpvProcess->kill();
        m_mpvProcess->waitForFinished(1000);
    }

    QProcess::execute("rm", QStringList() << "-f" << "/tmp/mpv-socket");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", "/home/jt-user/py310/bin:" + env.value("PATH"));
    m_mpvProcess->setProcessEnvironment(env);

    QStringList mpvArgs;
    mpvArgs << "--no-video"
            << "--input-ipc-server=/tmp/mpv-socket"
            << "--gapless-audio=yes"
            << "--ao=alsa"
            << "--terminal=yes"
            << "--msg-level=all=status"
            << "--demuxer-max-bytes=50"
            << "--demuxer-readahead-secs=30"
            << "--network-timeout=10"
            << url;

    qDebug() << "mpv command: /usr/bin/mpv" << mpvArgs.join(" ");

    m_mpvProcess->start("/usr/bin/mpv", mpvArgs);
    m_isPlaying = true;
    emit playbackStarted();
}

void MusicPlayerWorker::onMpvReadyReadStandardOutput()
{
    if (!m_mpvProcess)
        return;

    QString mpvLog = QString::fromUtf8(m_mpvProcess->readAllStandardOutput()).trimmed();

    if (mpvLog.contains("Starting playback...") || mpvLog.contains("AO: [alsa]"))
    {
        qDebug() << "mpv log verified - actual playback has started!";
        m_isPlaying = true;
        emit playbackStarted();
    }
}

void MusicPlayerWorker::onMpvError(QProcess::ProcessError error)
{
    qWarning() << "mpv execution failed:" << error;
    emit errorOccurred(QString("mpv execution failed: %1").arg(error));
}

void MusicPlayerWorker::resume()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "Resuming music playback";
        sendMpvIpcCommand("{\"command\":[\"set_property\",\"pause\",false]}");
        m_isPlaying = true;
        emit playbackStarted();
    }
}

void MusicPlayerWorker::pause()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "Pausing music playback";
        sendMpvIpcCommand("{\"command\":[\"set_property\",\"pause\",true]}");
        m_isPlaying = false;
        emit playbackPaused();
    }
}

void MusicPlayerWorker::stop()
{
    if (m_mpvProcess && m_mpvProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "Fully terminating all audio playback processes.";
        m_mpvProcess->kill();
        m_mpvProcess->waitForFinished(1000);
        m_isPlaying = false;
        emit playbackStopped();
    }
}

void MusicPlayerWorker::volumeUp()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "mpv volume up 5%";
        sendMpvIpcCommand("{\"command\":[\"add\",\"volume\",5]}");
    }
}

void MusicPlayerWorker::volumeDown()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "mpv volume down 5%";
        sendMpvIpcCommand("{\"command\":[\"add\",\"volume\",-5]}");
    }
}

void MusicPlayerWorker::shutdown()
{
    if (m_ytDlpProcess && m_ytDlpProcess->state() != QProcess::NotRunning)
    {
        m_ytDlpProcess->kill();
        m_ytDlpProcess->waitForFinished(1000);
    }

    if (m_mpvProcess && m_mpvProcess->state() != QProcess::NotRunning)
    {
        m_mpvProcess->kill();
        m_mpvProcess->waitForFinished(1000);
    }
}

void MusicPlayerWorker::sendMpvIpcCommand(const QString &jsonCommand)
{
    QProcess::execute("sh", QStringList() << "-c"
                      << QString("echo '%1' | socat - /tmp/mpv-socket").arg(jsonCommand));
}

int MusicPlayerWorker::parseDuration(const QString &durationStr) const
{
    QString trimmed = durationStr.trimmed();

    if (trimmed.contains(':'))
    {
        QStringList timeParts = trimmed.split(':');
        int durationInSeconds = 0;

        if (timeParts.size() == 2)
        {
            durationInSeconds = (timeParts.at(0).toInt() * 60)
                              + qRound(timeParts.at(1).toDouble());
        }
        else if (timeParts.size() == 3)
        {
            durationInSeconds = (timeParts.at(0).toInt() * 3600)
                              + (timeParts.at(1).toInt() * 60)
                              + qRound(timeParts.at(2).toDouble());
        }

        return durationInSeconds;
    }

    return qRound(trimmed.toDouble());
}
