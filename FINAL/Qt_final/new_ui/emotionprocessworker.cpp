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
        qDebug() << "Python 스크립트가 이미 실행 중입니다.";
        return;
    }

    m_process->setWorkingDirectory("/home/jt-user/SmartMirror/PYTHON");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("DISPLAY", ":0");
    env.insert("XAUTHORITY", "/home/jt-user/.Xauthority");
    m_process->setProcessEnvironment(env);

    m_process->start("/home/jt-user/py310/bin/python",
                     QStringList() << "final.py");

    qDebug() << "Python 스크립트 시작.";
    emit processOutput("Python 스크립트 시작됨");
}

void EmotionProcessWorker::stopProcess()
{
    if (!m_process)
        return;

    if (m_process->state() != QProcess::NotRunning)
    {
        qDebug() << "Python 스크립트 완전 종료.";
        m_process->kill();
        m_process->waitForFinished(1000);
        emit processOutput("스크립트 중지됨");
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
    qWarning() << "Python 프로세스 오류:" << error;
    emit processError(QString("프로세스 오류: %1").arg(error));
}
