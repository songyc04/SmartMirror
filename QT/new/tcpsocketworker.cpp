#include "tcpsocketworker.h"
#include <QDebug>

TcpSocketWorker::TcpSocketWorker(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_socket(nullptr)
{
}

TcpSocketWorker::~TcpSocketWorker()
{
}

void TcpSocketWorker::initialize()
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &TcpSocketWorker::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, ARDUINO_PORT))
    {
        qWarning() << "TCP 9000 server start failed:" << m_server->errorString();
        emit connectionStatus("9000 server start failed");
    }
    else
    {
        qDebug() << "Arduino waiting - port:" << ARDUINO_PORT;
        emit connectionStatus("9000 server waiting");
    }
}

void TcpSocketWorker::shutdown()
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

void TcpSocketWorker::onNewConnection()
{
    if (m_socket)
    {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }

    m_socket = m_server->nextPendingConnection();
    connect(m_socket, &QTcpSocket::readyRead,
            this, &TcpSocketWorker::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &TcpSocketWorker::onDisconnected);

    qDebug() << "Arduino connected:" << m_socket->peerAddress().toString();
    emit connectionStatus("Arduino connected");
}

void TcpSocketWorker::onReadyRead()
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

        qDebug() << "Received:" << data;
        emit dataReceived(data);
    }
}

void TcpSocketWorker::onDisconnected()
{
    qDebug() << "Arduino disconnected";
    qDebug() << "Arduino waiting - port:" << ARDUINO_PORT;
    emit connectionStatus("Arduino disconnected");

    if (m_socket)
    {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}
