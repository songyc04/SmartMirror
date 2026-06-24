#ifndef TCPSOCKETWORKER_H
#define TCPSOCKETWORKER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

const quint16 ARDUINO_PORT = 9000;

class TcpSocketWorker : public QObject
{
    Q_OBJECT

public:
    explicit TcpSocketWorker(QObject *parent = nullptr);
    ~TcpSocketWorker();

public slots:
    void initialize();
    void shutdown();

signals:
    void dataReceived(const QString &data);
    void connectionStatus(const QString &status);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpServer *m_server;
    QTcpSocket *m_socket;
};

#endif // TCPSOCKETWORKER_H
