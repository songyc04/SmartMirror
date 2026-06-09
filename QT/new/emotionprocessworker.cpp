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
        qDebug() << "Python emotion analysis script is already running.";
        return;
    }

    m_process->setWorkingDirectory("/home/jt-user/SmartMirror/PYTHON");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("DISPLAY", ":0");
    env.insert("XAUTHORITY", "/home/jt-user/.Xauthority");
    m_process->setProcessEnvironment(env);

    m_process->start("/home/jt-user/py310/bin/python",
                     QStringList() << "gesture_sr.py");

    qDebug() << "Starting Python emotion analysis script.";
    emit processOutput("Emotion analysis script started");
}

void EmotionProcessWorker::stopProcess()
{
    if (!m_process)
        return;

    if (m_process->state() != QProcess::NotRunning)
    {
        qDebug() << "Fully terminating Python emotion analysis script.";
        m_process->kill();
        m_process->waitForFinished(1000);
        emit processOutput("Emotion analysis script stopped");
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
    qWarning() << "Python process error:" << error;
    emit processError(QString("Process error: %1").arg(error));
}
