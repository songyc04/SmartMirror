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

    if (keyword.isEmpty())
    {
        qDebug() << "keyword가 비어있어 노래 재생을 건너뜁니다.";
        return;
    }

    m_keyword = keyword;

    if (m_ytDlpProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "yt-dlp 이전 프로세스 강제 종료.";
        m_ytDlpProcess->kill();
        m_ytDlpProcess->waitForFinished(1000);
    }

    qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));

    int targetRank = (qrand() % 10) + 1;
    qDebug() << "검색 키워드:" << m_keyword;
    qDebug() << "조회수 정렬:" << targetRank << "번째 곡 추출";

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
        qWarning() << "yt-dlp URL 추출 실패 (종료코드:" << exitCode << ")";
        emit errorOccurred("URL 추출 실패");
        return;
    }

    m_ytDlpProcess->readAllStandardError();

    QString rawOutput = QString::fromUtf8(m_ytDlpProcess->readAllStandardOutput()).trimmed();

    if (rawOutput.isEmpty())
    {
        qWarning() << "yt-dlp 출력이 비어있습니다.";
        emit errorOccurred("잘못된 URL");
        return;
    }

    QString outputLine = rawOutput.contains('\n')
        ? rawOutput.section('\n', -1, -1).trimmed()
        : rawOutput;

    QStringList fields = outputLine.split('|');

    QString audioUrl;
    QString videoTitle = "제목 없음";
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
        qWarning() << "유효한 YouTube URL을 얻지 못했습니다. 수신 데이터:" << outputLine;
        emit errorOccurred("잘못된 URL");
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
    qDebug() << "제목:" << videoTitle;
    qDebug() << "재생시간(원본):" << videoDuration << "->" << durationInSeconds << "초";
    qDebug() << "=========================";

    qDebug() << m_searchCount << "개 결과 중 최고 조회수 URL:" << audioUrl;

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
            << "--demuxer-readahead-secs=5"
            << "--cache=yes"
            << "--network-timeout=10"
            << url;

    qDebug() << "mpv 명령: /usr/bin/mpv" << mpvArgs.join(" ");

    m_mpvProcess->start("/usr/bin/mpv", mpvArgs);
    m_isPlaying = true;
//    emit playbackStarted();
}

void MusicPlayerWorker::onMpvReadyReadStandardOutput()
{
    if (!m_mpvProcess)
        return;

    QString mpvLog = QString::fromUtf8(m_mpvProcess->readAllStandardOutput()).trimmed();

    if (mpvLog.contains("Starting playback...") || mpvLog.contains("AO: [alsa]"))
    {
        qDebug() << "mpv 로그 확인 - 실제 재생 시작!";
        m_isPlaying = true;
        emit playbackStarted();
    }
}

void MusicPlayerWorker::onMpvError(QProcess::ProcessError error)
{
    qWarning() << "mpv 실행 실패:" << error;
    emit errorOccurred(QString("mpv 실행 실패: %1").arg(error));
}

void MusicPlayerWorker::resume()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "음악 재생 재개";
        sendMpvIpcCommand("{\"command\":[\"set_property\",\"pause\",false]}");
        m_isPlaying = true;
        emit playbackStarted();
    }
}

void MusicPlayerWorker::pause()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "음악 재생 일시정지";
        sendMpvIpcCommand("{\"command\":[\"set_property\",\"pause\",true]}");
        m_isPlaying = false;
        emit playbackPaused();
    }
}

void MusicPlayerWorker::stop()
{
    if (m_mpvProcess && m_mpvProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "모든 오디오 재생 프로세스 완전 종료.";
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
        qDebug() << "볼륨 5% 증가";
        QProcess::execute("pactl", QStringList() << "set-sink-volume" << "@DEFAULT_SINK@" << "+5%");

    }
}

void MusicPlayerWorker::volumeDown()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "볼륨 5% 감소";
        QProcess::execute("pactl", QStringList() << "set-sink-volume" << "@DEFAULT_SINK@" << "-5%");
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
