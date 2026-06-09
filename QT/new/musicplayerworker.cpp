#include "musicplayerworker.h"
#include <QDebug>
#include <QProcessEnvironment>
#include <QRegularExpression>
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

    m_keyword = keyword.isEmpty() ? QString("잔잔한") : keyword;

    if (m_ytDlpProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "yt-dlp 이전 프로세스를 강제 종료합니다.";
        m_ytDlpProcess->kill();
        m_ytDlpProcess->waitForFinished(1000);
    }

    qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));

    int targetRank = (qrand() % 10) + 1;
    qDebug() << "검색 키워드:" << m_keyword;
    qDebug() << "조회수 정렬:" << targetRank << "등 노래 추출 완료";

    QString cmd = QString("/home/jt-user/py310/bin/yt-dlp \"ytsearch%1:%2\" ")
                      .arg(m_searchCount)
                      .arg(m_keyword)
                  + "--match-filter \"duration <= 600\" "
                  + "--flat-playlist --print \"%(view_count)012d %(url)s\" "
                  + "| sort -r | head -n "
                  + QString::number(targetRank)
                  + " | awk '{print $2}'";

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
        qWarning() << "yt-dlp URL 추출 실패 (exitCode:" << exitCode << ")";
        emit errorOccurred("URL 추출 실패");
        return;
    }

    m_ytDlpProcess->readAllStandardError();

    QString audioUrl = QString::fromUtf8(m_ytDlpProcess->readAllStandardOutput()).trimmed();

    if (audioUrl.contains('\n'))
    {
        audioUrl = audioUrl.section('\n', -1, -1).trimmed();
    }

    if (audioUrl.isEmpty() || !audioUrl.startsWith("http"))
    {
        qWarning() << "유효한 유튜브 주소를 획득하지 못했습니다. 수신 데이터:" << audioUrl;
        emit errorOccurred("유효하지 않은 URL");
        return;
    }

    qDebug() << "URL:" << audioUrl;

    QProcess infoProcess;
    infoProcess.start("/home/jt-user/py310/bin/yt-dlp",
                      QStringList() << "--skip-download"
                                    << "--get-duration"
                                    << "--print" << "title"
                                    << audioUrl);

    if (!infoProcess.waitForFinished(30000))
    {
        qDebug() << "yt-dlp 정보 요청 시간 초과";
        emit errorOccurred("yt-dlp 제목/길이 요청 시간 초과");
        return;
    }

    QString output = QString::fromUtf8(infoProcess.readAllStandardOutput()).trimmed();
    QString errorOutput = QString::fromUtf8(infoProcess.readAllStandardError()).trimmed();

    qDebug() << "OUTPUT:" << output;
    if (output.isEmpty())
    {
        qDebug() << "Error: 출력된 데이터가 없습니다.";
    }

    QStringList outputList = output.split(QRegularExpression("[\r\n]+"), QString::SkipEmptyParts);

    QString videoTitle = "알 수 없는 제목";
    QString videoDuration = "00:00";

    if (outputList.size() >= 2)
    {
        videoTitle = outputList.at(0);
        videoDuration = outputList.at(1);
    }
    else if (outputList.size() == 1)
    {
        videoTitle = outputList.at(0);
    }
    else
    {
        qWarning() << "제목과 길이 데이터를 파싱할 수 없습니다.";
    }

    int durationInSeconds = parseDuration(videoDuration);

    qDebug() << "=========================";
    qDebug() << "제목:" << videoTitle;
    qDebug() << "길이:" << videoDuration;
    qDebug() << "=========================";

    qDebug() << m_searchCount << "개 중 조회수가 가장 높은 영상 URL 추출 성공:" << audioUrl;

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

    qDebug() << "mpv 구동 명령어: /usr/bin/mpv" << mpvArgs.join(" ");

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
        qDebug() << "mpv 자체 로그 검증 완료 - 실제 노래 재생이 시작되었습니다!";
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
        qDebug() << "음악 일시정지";
        sendMpvIpcCommand("{\"command\":[\"set_property\",\"pause\",true]}");
        m_isPlaying = false;
        emit playbackPaused();
    }
}

void MusicPlayerWorker::stop()
{
    if (m_mpvProcess && m_mpvProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "모든 오디오 재생 프로세스를 완전 종료합니다.";
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
        qDebug() << "mpv 볼륨 5% 증가";
        sendMpvIpcCommand("{\"command\":[\"cycle\",\"volume\",\"up\"]}");
    }
}

void MusicPlayerWorker::volumeDown()
{
    if (m_mpvProcess && m_mpvProcess->state() == QProcess::Running)
    {
        qDebug() << "mpv 볼륨 5% 감소";
        sendMpvIpcCommand("{\"command\":[\"cycle\",\"volume\",\"down\"]}");
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
    QStringList timeParts = durationStr.trimmed().split(':');
    int durationInSeconds = 0;

    if (timeParts.size() == 2)
    {
        durationInSeconds = (timeParts.at(0).toInt() * 60) + timeParts.at(1).toInt();
    }
    else if (timeParts.size() == 3)
    {
        durationInSeconds = (timeParts.at(0).toInt() * 3600)
                          + (timeParts.at(1).toInt() * 60)
                          + timeParts.at(2).toInt();
    }
    else if (!timeParts.isEmpty())
    {
        durationInSeconds = timeParts.at(0).toInt();
    }

    return durationInSeconds;
}
