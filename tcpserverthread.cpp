#include "tcpserverthread.h"

TCPServerThread::TCPServerThread()
{

}

TCPServerThread::~TCPServerThread(){
    if (socketMade) tcpServer->close();  //should clean up any sockets created from this server too

}

void TCPServerThread::startUp(){  // runs when the object is created and moved to a new thread
    emit postMessage("Thread Started");
    socketMade = false;


    tcpServer = new QTcpServer();
    QString currentIP = "146.229.161.0";  // address of carsim desktop
    QHostAddress hostAddress;
    hostAddress.setAddress(currentIP);
    tcpServer->listen(hostAddress, 4712);
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
