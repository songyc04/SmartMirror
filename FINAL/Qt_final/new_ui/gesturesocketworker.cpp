#include "gesturesocketworker.h"
#include <QDebug>

GestureSocketWorker::GestureSocketWorker(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_socket(nullptr)
{
}

GestureSocketWorker::~GestureSocketWorker()
{
}

void GestureSocketWorker::initialize()
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &GestureSocketWorker::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, GESTURE_PORT))
    {
        qWarning() << "9001 TCP 서버 시작 실패:" << m_server->errorString();
        emit connectionStatus("9001 서버 시작 실패");
    }
    else
    {
        qDebug() << "9001 서버 대기 중 - 포트:" << GESTURE_PORT;
        emit connectionStatus("9001 서버 대기 중");
    }
}

void GestureSocketWorker::shutdown()
{
    if (m_socket)
    {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_server)
    {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void GestureSocketWorker::onNewConnection()
{
    if (m_socket)
    {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }

    m_socket = m_server->nextPendingConnection();
    connect(m_socket, &QTcpSocket::readyRead,
            this, &GestureSocketWorker::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &GestureSocketWorker::onDisconnected);

    qDebug() << "9001 연결:" << m_socket->peerAddress().toString();
    emit connectionStatus("9001 연결됨");
}

void GestureSocketWorker::onReadyRead()
{
    if (!m_socket)
        return;

    while (m_socket->canReadLine())
    {
        QByteArray raw = m_socket->readLine();
        QString data = QString::fromUtf8(raw).trimmed();

        if (data.startsWith("\"") && data.endsWith("\""))
        {
            data = data.mid(1, data.length() - 2);
        }
        data = data.trimmed();

        qDebug() << "9001 수신:" << data;
        emit gestureReceived(data);
    }
}

void GestureSocketWorker::onDisconnected()
{
    qDebug() << "9001 연결 해제";
    qDebug() << "9001 대기 중 - 포트:" << GESTURE_PORT;
    emit connectionStatus("9001 연결 해제됨");

    if (m_socket)
    {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}
