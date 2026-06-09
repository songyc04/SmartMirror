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
        qWarning() << "9001 TCP server start failed:" << m_server->errorString();
        emit connectionStatus("9001 server start failed");
    }
    else
    {
        qDebug() << "9001 server waiting - port:" << GESTURE_PORT;
        emit connectionStatus("9001 server waiting");
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

    qDebug() << "9001 connected:" << m_socket->peerAddress().toString();
    emit connectionStatus("9001 connected");
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

        qDebug() << "9001 received:" << data;
        emit gestureReceived(data);
    }
}

void GestureSocketWorker::onDisconnected()
{
    qDebug() << "9001 disconnected";
    qDebug() << "9001 waiting - port:" << GESTURE_PORT;
    emit connectionStatus("9001 disconnected");

    if (m_socket)
    {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}
