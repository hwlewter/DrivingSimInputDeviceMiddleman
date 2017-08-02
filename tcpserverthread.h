#ifndef TCPSERVERTHREAD_H
#define TCPSERVERTHREAD_H
#include <QTcpSocket>
#include <QTcpServer>

#include <QtWebSockets/QWebSocket>
#include <QUrl>
#include <QtCore/QObject>
/**
 * Actually just an object intended to be moved to a thread
 */

class TCPServerThread : public QObject
{
    Q_OBJECT

public:
    TCPServerThread();
    ~TCPServerThread();
    QTcpServer *tcpServer;
    QTcpSocket *tcpSocket;  // socket for openDS
    QTcpSocket *tcpSocket2; // socket for remote app
    bool socketMade;  // first socket for openDS
    QString currentIP;
    QHostAddress hostAddress;
    void startOver();

signals:
    void newConnection();  // to tell GUI a socket is ready for use
    void postMessage(QString message);
    void remoteFaultMessage(QString message);

public slots:
    void startUp();  // start the TCP server to listen
    void processNewConnection();  // triggers when server receives new connection
    void readRemoteFaultMsg();  // reads incoming messages from remote app

};

#endif // TCPSERVERTHREAD_H
