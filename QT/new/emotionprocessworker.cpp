#include "emotionprocessworker.h"
#include <QDebug>
#include <QProcessEnvironment>

EmotionProcessWorker::EmotionProcessWorker(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
{
}

EmotionProcessWorker::~EmotionProcessWorker()
{
}

void EmotionProcessWorker::initialize()
{
    m_process = new QProcess(this);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &EmotionProcessWorker::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &EmotionProcessWorker::onReadyReadStandardError);
    connect(m_process, &QProcess::errorOccurred,
            this, &EmotionProcessWorker::onProcessError);
}

void EmotionProcessWorker::startProcess()
{
    if (!m_process)
        return;

    if (m_process->state() != QProcess::NotRunning)
    {
        qDebug() << "파이썬 감정 분석 스크립트가 이미 실행 중입니다.";
        return;
    }

    m_process->setWorkingDirectory("/home/jt-user/SmartMirror/PYTHON");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("DISPLAY", ":0");
    env.insert("XAUTHORITY", "/home/jt-user/.Xauthority");
    m_process->setProcessEnvironment(env);

    m_process->start("/home/jt-user/py310/bin/python",
                     QStringList() << "gesture_sr.py");

    qDebug() << "파이썬 감정 분석 스크립트를 시작합니다.";
    emit processOutput("감정 분석 스크립트 시작");
}

void EmotionProcessWorker::stopProcess()
{
    if (!m_process)
        return;

    if (m_process->state() != QProcess::NotRunning)
    {
        qDebug() << "파이썬 감정 분석 스크립트를 완전히 종료합니다.";
        m_process->kill();
        m_process->waitForFinished(1000);
        emit processOutput("감정 분석 스크립트 종료");
    }
}

void EmotionProcessWorker::shutdown()
{
    stopProcess();
    if (m_process)
    {
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void EmotionProcessWorker::onReadyReadStandardOutput()
{
    QByteArray output = m_process->readAllStandardOutput();
    QString msg = QString::fromUtf8(output).trimmed();
    qDebug() << "[Python Out]:" << msg;
    emit processOutput(msg);
}

void EmotionProcessWorker::onReadyReadStandardError()
{
    QByteArray errorOutput = m_process->readAllStandardError();
    if (!errorOutput.isEmpty())
    {
        QString msg = QString::fromUtf8(errorOutput).trimmed();
        qWarning() << "[Python Error]:" << msg;
        emit processError(msg);
    }
}

void EmotionProcessWorker::onProcessError(QProcess::ProcessError error)
{
    qWarning() << "파이썬 프로세스 오류 발생:" << error;
    emit processError(QString("프로세스 오류: %1").arg(error));
}
