#include "tcpserverthread.h"

TCPServerThread::TCPServerThread()
{

}

TCPServerThread::~TCPServerThread(){
    if (socketMade) tcpServer->close();  //should clean up any sockets created from this server too
}

void TCPServerThread::startOver(){
    socketMade =  false;

}


void TCPServerThread::startUp(){  // runs when the object is created and moved to a new thread
    emit postMessage("Thread Started");
    socketMade = false;

//ws://127.0.0.1:4712/socket.io/?EIO=4&transport=websocket
    tcpServer = new QTcpServer();

    //QWebSocket webSocket;
   // QUrl url("ws://127.0.0.1:4712/socket.io/?EIO=4&transport=websocket");
    //QWebSocket webSocket;
   //QString currentIP = "127.0.0.1";  // 146.229.161.0 is address of carsim desktop
    currentIP = "146.229.161.0";
    //connect(&m_webSocket, &QWebSocket::connected, this, &EchoClient::onConnected);
    //connect(&m_webSocket, &QWebSocket::disconnected, this, &EchoClient::closed);
   // connect(&webSocket, &QWebSocket::connected, this, &TCPServerThread::processNewConnection);
    //connect(&webSocket, &QWebSocket::disconnected, this, &TCPServerThread::closed);
    //webSocket.open(QUrl(url));
    //
    //QHostAddress hostAddress;
    hostAddress.setAddress(currentIP);
    tcpServer->listen(hostAddress, 4712);
    //emit postMessage("Server Address listening is : " + url.toString());
    emit postMessage("Server Address listening is : "+tcpServer->serverAddress().toString()+" on Port: "+QString::number(tcpServer->serverPort()));

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(processNewConnection()));


}

void TCPServerThread::processNewConnection(){  // runs when a new connection is detected

    if (!socketMade){  // if no connections have been made yet
        tcpSocket = new QTcpSocket();
        if (tcpServer->hasPendingConnections()){
            emit postMessage("TCP Server has pending Connections");
            tcpSocket = tcpServer->nextPendingConnection();
            socketMade = true;
        }
        if (tcpSocket->isOpen()){
            emit postMessage("Socket successfully bound");
            socketMade = true;
            emit newConnection();

        }
    }

    // for remote socket
    else {
        tcpSocket2 = new QTcpSocket();
        if (tcpServer->hasPendingConnections()){
            emit postMessage("TCP Server has another pending Connection");
            tcpSocket2 = tcpServer->nextPendingConnection();
            connect(tcpSocket2, SIGNAL(readyRead()), this, SLOT(readRemoteFaultMsg()));
        }
        if (tcpSocket2->isOpen()){
            emit postMessage("Secondary socket successfully bound");
        }
    }

}

void TCPServerThread::readRemoteFaultMsg(){
    QByteArray incomingPacket = tcpSocket2->readAll();
    QString msg = incomingPacket;
    emit postMessage("Remote Message Recieved: " +msg);
    emit remoteFaultMessage(msg);
}
