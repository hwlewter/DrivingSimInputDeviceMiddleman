#ifndef DEVICETESTER_H
#define DEVICETESTER_H

#include <QMainWindow>
#include <Windows.h>
#include <strsafe.h>
#include <QDebug>
#include <QTimer>
#include <QString>
#include <QTcpSocket>
#include <QTcpServer>
#include <QHostAddress>
#include <QTime>
#include "tcpserverthread.h"
#include <QThread>

namespace Ui {
class DeviceTester;
}

class DeviceTester : public QMainWindow
{
    Q_OBJECT

public:
    explicit DeviceTester(QWidget *parent = 0);
    ~DeviceTester();
    // QString messageValue;
    static QString staticMsg;
    static QString byteData;
    static QString byteValue;
    static bool flipFlop; // used to signal a new message was received
    static uint8_t wheelValues[15];
    static uint8_t shifterValues[12];
    static short count;
    static LONG_PTR oldWindowProc;
    bool flipFlopChange; // used to see if change happened since last check
    QTcpSocket *tcpSocket;
    QTcpServer *tcpServer;
    TCPServerThread *serverThread;
    QThread *thread;
    void delay();

private:
    Ui::DeviceTester *ui;
    void registerControllers();
    //void errorExit(LPTSTR lpszFunction);
    static LRESULT CALLBACK WindowMessageProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    bool socketMade;
    QTimer *timer;
    double steeringEffectiveness;
    double brakeEffectiveness;
    double inputDelay;

private slots:
    void getClassInformation();
    void updateText();
    void createHostSocket();
    void sendCanbusPacketSteering();
    void sendCanbusPacketGas();
    void sendCanbusPacketBrake();
    void sendCanbusPacketClutch();
    void sendDPadPacket();
    void steeringEffectivenessUpdate();
    void brakeEffectivenessUpdate();
    void packetSender(QByteArray packet);
    void delayUpdate();
    void showMessageFromThread(QString message);
    void socketProvided();  // from signal letting us know a socket was created by TCP Server
    void updateFaults(QString message); // process message from remote app to update faults
    void sendJSONPacket();
};

#endif // DEVICETESTER_H
