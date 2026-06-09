#ifndef GESTURESOCKETWORKER_H
#define GESTURESOCKETWORKER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

const quint16 GESTURE_PORT = 9001;

class GestureSocketWorker : public QObject
{
    Q_OBJECT

public:
    explicit GestureSocketWorker(QObject *parent = nullptr);
    ~GestureSocketWorker();

public slots:
    void initialize();
    void shutdown();

signals:
    void gestureReceived(const QString &gesture);
    void connectionStatus(const QString &status);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpServer *m_server;
    QTcpSocket *m_socket;
};

#endif // GESTURESOCKETWORKER_H
