#include "devicetester.h"
#include "ui_devicetester.h"

QString DeviceTester::staticMsg = "Beginning Message";
QString DeviceTester::byteData = "\tBlank Data";
QString DeviceTester::byteValue = "\tInitial Value";
bool DeviceTester::flipFlop = false;  // will change when a new message was received
uint8_t DeviceTester::wheelValues[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t DeviceTester::shifterValues[] = {0,0,0,0,0,0,0,0,0,0,0,0};
short DeviceTester::count = 0;
LONG_PTR DeviceTester::oldWindowProc = (LONGLONG)&DeviceTester::WindowMessageProc;

typedef LRESULT (DeviceTester::*functionPointer)(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


DeviceTester::DeviceTester(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DeviceTester)
{
    ui->setupUi(this);

    //QPushButton* sendButton = ui->sendButton;


    // initialize fault modifiers
    steeringEffectiveness = 1.0;
    brakeEffectiveness = 1.0;
    inputDelay = 0;

    socketMade = false;  // for resource cleanup
    registerControllers();  // listens to controllers via raw input API

    flipFlopChange = false;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateText()));
    timer->start(10);

    connect(ui->pushButtonShowClassInfo, &QPushButton::clicked, this, &DeviceTester::getClassInformation);
    connect(ui->pushButtonCreateHost, &QPushButton::clicked, this, &DeviceTester::createHostSocket);
    connect(ui->verticalSliderBrake, &QSlider::valueChanged, this, &DeviceTester::brakeEffectivenessUpdate);
    connect(ui->verticalSliderSteer, &QSlider::valueChanged, this, &DeviceTester::steeringEffectivenessUpdate);
    connect(ui->dialDelay, &QDial::valueChanged, this, &DeviceTester::delayUpdate);

    ui->inputDelayLabel->setText("0.0 sec");
}

DeviceTester::~DeviceTester()
{
    if (socketMade) {
        //tcpSocket->close();
        thread->deleteLater();
    }
    delete ui;
}

void DeviceTester::delay(){
    QTime targetTime = QTime::currentTime().addMSecs(inputDelay * 1000);
    while (QTime::currentTime() < targetTime){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
}

void DeviceTester::delayUpdate(){
    double knobValue = ui->dialDelay->value();
    inputDelay = knobValue / 10; // range of 0 - 2 second delay
    ui->inputDelayLabel->setText(QString::number(inputDelay, 'f', 1)+" sec");
}

void DeviceTester::packetSender(QByteArray packet){
    if (inputDelay > 0) delay();
    serverThread->tcpSocket->write(packet);
    //emit xmlMessage(packet);
}

void DeviceTester::brakeEffectivenessUpdate(){
    brakeEffectiveness = ui->verticalSliderBrake->value();
    brakeEffectiveness /= 100;// should range between 0 and 1
}

void DeviceTester::steeringEffectivenessUpdate(){
    steeringEffectiveness = ui->verticalSliderSteer->value();
    steeringEffectiveness /= 100;
}

void DeviceTester::updateFaults(QString message){  // triggered when a socket emits remote fault message
    QStringList splitLine;
    splitLine = message.split(','); // first elment is source, 2nd is value
    short source = splitLine[0].toShort();
    short value = splitLine[1].toShort();
    switch (source){
    case 0: // invert steering  0 or 1
        if (value == 1) ui->checkBoxSteerReverse->setChecked(true);
        else ui->checkBoxSteerReverse->setChecked(false);
        break;
    case 1: // disable brakes
        if (value == 1) ui->checkBoxBrakeDisable->setChecked(true);
        else ui->checkBoxBrakeDisable->setChecked(false);
        break;
    case 2: // accelerator stuck
        if (value == 1) ui->checkBoxGasMax->setChecked(true);
        else ui->checkBoxGasMax->setChecked(false);
        break;
    case 3: // steering efficiency  0-100
        ui->verticalSliderSteer->setValue(value);
        break;
    case 4: // brake efficiency  0-100
        ui->verticalSliderBrake->setValue(value);
        break;
    case 5: // input delay  0-20
        ui->dialDelay->setValue(value);
        break;
    default:
        ui->textBrowser->append("Remote message not handled: "+message);
        break;
    }
}

void DeviceTester::createHostSocket(){
    if (ui->radioButtonOpenDS->isChecked()){
        ui->textBrowser->append("Host Socket for OpenDS To be created");
        if (socketMade){
            //tcpSocket->close();
            //serverThread->~TCPServerThread();
            //tcpServer->close();
            //thread->quit();
            //thread->deleteLater();
            serverThread->startOver();
            return;
        }

        socketMade = true;  // now meaning the thread for the socket server was started
        thread = new QThread;
        serverThread = new TCPServerThread();
        serverThread->moveToThread(thread);
        connect(serverThread, SIGNAL(postMessage(QString)), this, SLOT(showMessageFromThread(QString)));
        connect(thread, SIGNAL(started()), serverThread, SLOT(startUp()));  // once thread is started, begin server process
        connect(serverThread, SIGNAL(newConnection()), this, SLOT(socketProvided()));
        thread->start();
        ui->textBrowser->append("Thread should have started...");
    }
    else if (ui->radioButtonUdacity->isChecked()){
        //ui->textBrowser->append("Udacity support not finished yet");
        ui->textBrowser->append("Web Socket for Unity to be created");
   //     if (socketMade){
            //tcpSocket->close();
   //         thread->deleteLater();
   //     }

     //   socketMade = true;  // now meaning the thread for the socket server was started
     //   thread = new QThread;
     //   serverThread = new TCPServerThread();
     //   serverThread->moveToThread(thread);
        webSocketServer = new QWebSocketServer(QStringLiteral("WebServer"),
                                   QWebSocketServer::NonSecureMode,
                                   this);
        if(webSocketServer->listen(QHostAddress::Any, 4712))
        {
            ui->textBrowser->append("Web Socket Server listening on port 4712");
            connect(webSocketServer, &QWebSocketServer::newConnection, this, &DeviceTester::onNewConnection);
        }
        //connect(ui->sendButton, &QPushButton::clicked, this, &DeviceTester::sendJSONPacket);
        connect(ui->lineEditWheelDec,&QLineEdit::textChanged, this, &DeviceTester::sendJSONPacket);
        connect(ui->lineEditGas,&QLineEdit::textChanged, this, &DeviceTester::sendJSONPacket);
       // connect(serverThread, SIGNAL(postMessage(QString)), this, SLOT(showMessageFromThread(QString)));
       // connect(thread, SIGNAL(started()), serverThread, SLOT(startUp()));  // once thread is started, begin server process
       // connect(serverThread, SIGNAL(newConnection()), this, SLOT(socketProvided()));
      //  thread->start();
        ui->textBrowser->append("Thread should have started...");
    }
}

void DeviceTester::socketProvided(){  // when told a socket was made
    if (ui->radioButtonOpenDS->isChecked()){
        ui->textBrowser->append("XML formatted Messages selected for OpenDS");
        //socketMade = true;
        connect(ui->lineEditWheelDec, &QLineEdit::textChanged, this, &DeviceTester::sendCanbusPacketSteering);
        connect(ui->lineEditGas, &QLineEdit::textChanged, this, &DeviceTester::sendCanbusPacketGas);
        connect(ui->lineEditBrake, &QLineEdit::textChanged, this, &DeviceTester::sendCanbusPacketBrake);
        connect(ui->lineEditClutch, &QLineEdit::textChanged, this, &DeviceTester::sendCanbusPacketClutch);
        connect(ui->lineEditWheelByte15, &QLineEdit::textChanged, this, &DeviceTester::sendDPadPacket);
        connect(ui->lineEditShiftByte2, &QLineEdit::textChanged, this, &DeviceTester::sendGearPacket);
        connect(ui->lineEditWheelByte12, &QLineEdit::textChanged, this, &DeviceTester::sendWheelButtonsPacket);

        // for remote app socket
        connect(serverThread, SIGNAL(remoteFaultMessage(QString)), this, SLOT(updateFaults(QString)));
    }
    else if (ui->radioButtonUdacity->isChecked()){
        ui->textBrowser->append("JSON formatted messages selected for Udacity Sim");

        // TODO: connect to JSON based slots
        connect(ui->lineEditGas, &QLineEdit::textChanged, this, &DeviceTester::sendJSONPacket);
        connect(ui->lineEditWheelDec, &QLineEdit::textChanged, this, &DeviceTester::sendJSONPacket);
    }
}

void DeviceTester::sendJSONPacket()
{
    //Set Thrustmaster values
    double steerValue = ui->lineEditWheelDec->text().toDouble();
    double steerValueConverted = (steerValue-32767)/21844;
    double throttleValue = ui -> lineEditGas->text().toDouble();
    double throttleValueConverted = (1023-throttleValue)/1023;
    ui->textBrowser->append("Message attempt to send.");
    if(client != nullptr)//.pro->QMAKE_CXXFLAGS += -std=c++0x
    {
        //Set user input values to test car funtionality before using Thrustmaster
        //double steerAngle = ui->lineEditWheelDec->text().toDouble();
        //double gas = ui->lineEditGas->text().toDouble();

        //Create Json Dictionaries
        QJsonObject eventData;
        eventData.insert("steering_angle", QJsonValue::fromVariant(steerValueConverted));
        eventData.insert("throttle", QJsonValue::fromVariant(throttleValueConverted));
        //eventData.insert("");

        QJsonObject data;
        //event.insert("name", "steer");
        data.insert("data", eventData);
        QJsonObject event;
        event.insert("name", "steer");

        QJsonDocument doc(data);//Send name
        QJsonDocument doc2(event);
        //client->sendTextMessage("42" + doc.toJson(QJsonDocument::Compact));//Send data
        //client->sendTextMessage("42{\"data\":{\"steering_angle\":0,\"throttle\":0},\"name\":\"steer\"}");
        client->sendTextMessage("42" + doc2.toJson(QJsonDocument::Compact) + doc.toJson(QJsonDocument::Compact));
        //client->sendTextMessage("42" + event);//Send data

    }
}

void DeviceTester::onNewConnection()
{
    ui->textBrowser->append("Client connected.");
    client = webSocketServer->nextPendingConnection();
}

void DeviceTester::showMessageFromThread(QString message){
    ui->textBrowser->append(message);
}

void DeviceTester::sendWheelButtonsPacket(){ // from byte 12 listing
    bool ok = false;
    short wheelButtonValue = ui->lineEditWheelByte12->text().toShort(&ok, 2);
    QString str;
    QByteArray packet;
    switch (wheelButtonValue){
    case 8:  // purple button
        str = "<message><action name=\"keyPress\">KEY_END</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    default:
        break;
    }
}

void DeviceTester::sendGearPacket(){
    bool ok = false;
    short shifterValue = ui->lineEditShiftByte2->text().toShort(&ok,2); // read Byte 2 as a binary value
    QString str;
    QByteArray packet;
    switch (shifterValue){
    case 0:  // neutral
        str = "<message><action name=\"keyPress\">NEUTRAL</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    case 1:  // 1st gear
        str = "<message><action name=\"keyPress\">BUTTON_8</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    case 2:  // 2nd gear
        str = "<message><action name=\"keyPress\">BUTTON_9</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    case 4:  // 3rd gear
        str = "<message><action name=\"keyPress\">BUTTON_10</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    case 8:  // 4th gear
        str = "<message><action name=\"keyPress\">BUTTON_11</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    case 16:  // 5th gear
        str = "<message><action name=\"keyPress\">BUTTON_12</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    case 32:  // 6th gear
        str = "<message><action name=\"keyPress\">BUTTON_13</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    case 128:  // Reverse gear
        str = "<message><action name=\"keyPress\">BUTTON_14</action></message>";
        ui->textBrowser->append("XML Message sent: "+ str);
        packet.append(str);
        packetSender(packet);
        break;
    default:
        break;
    }
}


void DeviceTester::sendDPadPacket(){
    bool ok = false;
    short dPadValue = ui->lineEditWheelByte15->text().toShort(&ok,2);
    QString str;
    QByteArray packet;
    switch (dPadValue){
    case 2:  // reset the car
        str = "<message><action name=\"button\">return</action></message>";
        ui->textBrowser->append("XML Message sent: "+str);
        packet.append(str);
        //tcpSocket->write(packet);
        packetSender(packet);
        break;
    case 4:  // reverse needs an int value over 0
        str = "<message><action name=\"MFLminus_State\">1</action></message>";
        ui->textBrowser->append("XML Message sent: "+str);
        packet.append(str);
        packetSender(packet);
        break;
    case 6:  // change the view
        str = "<message><action name=\"button\">cs</action></message>";
        ui->textBrowser->append("XML Message sent: "+str);
        packet.append(str);
        packetSender(packet);
        break;
    default:
        break;
    }
}

void DeviceTester::sendCanbusPacketGas(){
    double gasValue = ui->lineEditGas->text().toDouble();
    double gasValueConverted = gasValue/1023;  // for a value between 0.0 and 1.0
    gasValueConverted = 1 - gasValueConverted; // to invert values where 1 is pedal to the metal
    if (ui->checkBoxGasMax->isChecked()) gasValueConverted = 1; // fault triggered
    QString gasStr = "<message><action name=\"acceleration\">"+QString::number(gasValueConverted,'f',1)+"</action></message>";
    ui->textBrowser->append("XML Message sent: "+gasStr);
    QByteArray gasPacket;
    gasPacket.append(gasStr);
    //tcpSocket->write(gasPacket);
    packetSender(gasPacket);
}

void DeviceTester::sendCanbusPacketBrake(){
    double brakeValue = ui->lineEditBrake->text().toDouble();
    double brakeValueConverted = brakeValue/1023;  // for a value between 0.0 and 1.0
    brakeValueConverted = 1 - brakeValueConverted; // to invert values where 1 is pedal to the metal
    brakeValueConverted *= brakeEffectiveness; // fault modifier
    if (ui->checkBoxBrakeDisable->isChecked()) brakeValueConverted = 0; // fault triggered
    QString brakeStr = "<message><action name=\"brake\">"+QString::number(brakeValueConverted,'f',1)+"</action></message>";
    ui->textBrowser->append("XML Message sent: "+brakeStr);
    QByteArray brakePacket;
    brakePacket.append(brakeStr);
    //tcpSocket->write(brakePacket);
    packetSender(brakePacket);
}

void DeviceTester::sendCanbusPacketClutch(){
    double clutchValue = ui->lineEditClutch->text().toDouble();
    double clutchValueConverted = clutchValue/1023;  // for a value between 0.0 and 1.0
    clutchValueConverted = 1 - clutchValueConverted; // to invert values where 1 is pedal to the metal
    QString clutchStr = "<message><action name=\"clutch\">"+QString::number(clutchValueConverted,'f',1)+"</action></message>";
    ui->textBrowser->append("XML Message sent: "+clutchStr);
    QByteArray clutchPacket;
    clutchPacket.append(clutchStr);
    //tcpSocket->write(clutchPacket);
    packetSender(clutchPacket);
}

void DeviceTester::sendCanbusPacketSteering(){
    //ui->textBrowser->append("Let's send a packet to OpenDS about the wheel");
    double steeringValue = ui->lineEditWheelDec->text().toDouble();
    steeringValue -= 32767; // offset the value to get range of -32,767 to 32,767
    steeringValue /=60; // divide by step size of 60 adjust range to steering angle of -540 to 540
    if (steeringValue < -540) steeringValue = -540;
    else if (steeringValue > 540) steeringValue = 540;
    double steeringModifier = -1;
    if (ui->checkBoxSteerReverse->isChecked()) steeringModifier = 1;
    steeringValue *= steeringModifier;  // inversion
    steeringValue *= steeringEffectiveness; // adjust for fault
    QString steeringStr = "<message><action name=\"steering\">"+QString::number(steeringValue, 'f', 0)+"</action></message>";
    ui->textBrowser->append("XML Message sent: "+steeringStr);
    QByteArray steeringPacket;
    steeringPacket.append(steeringStr);
    //tcpSocket->write(steeringPacket);
    packetSender(steeringPacket);
}


void DeviceTester::getClassInformation(){
    ui->textBrowser->append("Class Information: ");
    WNDCLASSEX windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEX);
    LPTSTR className = new TCHAR[50];
    for (int c = 0; c< 50; c++){
        className[c] = '&';
    }
    HWND windowHandle = GetActiveWindow();
    int getClassNameReturnValue = GetClassName(windowHandle, className, 40);
    ui->textBrowser->append("\tThe number of characters copied to class name buffer: "+QString::number(getClassNameReturnValue));
    QString name;
    for (int c = 0; c< 50; c++){
        if (className[c] != '&')name+= className[c];
    }
    int strLength = name.length();
    LPTSTR moreAccurateClassName = new TCHAR[strLength];
    for (int i = 0; i < strLength; i++){
        moreAccurateClassName[i] = className[i];
    }
    ui->textBrowser->append("The class name found: " + name);

    HINSTANCE hInstance = (HINSTANCE)::GetModuleHandle(NULL);

    BOOL returnValue = GetClassInfoEx(hInstance, moreAccurateClassName, &windowClass);
    ui->textBrowser->append("\tThe return value for the GetClassInfo call: " + QString::number(returnValue));
    delete[] className;

    if (returnValue == 0){
        //this->errorExit((LPTSTR) TEXT("GetClassInfoEx"));
        ui->textBrowser->append("GetClassInfoEx failed");
        return;
    }
    /*
      typedef struct tagWNDCLASSEX {
        UINT      cbSize;
        UINT      style;
        WNDPROC   lpfnWndProc;
        int       cbClsExtra;
        int       cbWndExtra;
        HINSTANCE hInstance;
        HICON     hIcon;
        HCURSOR   hCursor;
        HBRUSH    hbrBackground;
        LPCTSTR   lpszMenuName;
        LPCTSTR   lpszClassName;
        HICON     hIconSm;
        } WNDCLASSEX, *PWNDCLASSEX;
    */

    ui->textBrowser->append("The Window Class started with WNDPROC: "+ QString::number((LONGLONG)windowClass.lpfnWndProc,16));
    DeviceTester::oldWindowProc = GetWindowLongPtr(windowHandle, GWLP_WNDPROC);
    ui->textBrowser->append("GetWindowLongPtr says it was WNDPROC: "+ QString::number(DeviceTester::oldWindowProc,16));
    DeviceTester::oldWindowProc = SetWindowLongPtr(windowHandle, GWLP_WNDPROC, (LONGLONG)&WindowMessageProc); // set new value for window procedure default
    ui->textBrowser->append("Old Window Proc Value: "+QString::number(DeviceTester::oldWindowProc,16));

    returnValue = GetClassInfoEx(hInstance, moreAccurateClassName, &windowClass);
    ui->textBrowser->append("\tThe return value for the GetClassInfo call: " + QString::number(returnValue));
    ui->textBrowser->append("The Window Class now has a WNDPROC: "+ QString::number((LONGLONG)windowClass.lpfnWndProc,16));
    LONG_PTR newestWindowProc = GetWindowLongPtr(windowHandle, GWLP_WNDPROC);
    ui->textBrowser->append("GetWindowLongPtr says it was WNDPROC: "+ QString::number(newestWindowProc,16));
}

LRESULT CALLBACK DeviceTester::WindowMessageProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    UINT getRawReturnValue = 0;
    UINT cbSizeHeader = 0;

    switch (uMsg){
        case WM_INPUT:{
            DeviceTester::count = 0;
            DeviceTester::flipFlop = !DeviceTester::flipFlop; // invert the flipFlop

            UINT pcbSize;
            cbSizeHeader  = sizeof(RAWINPUTHEADER);
            getRawReturnValue = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &pcbSize, cbSizeHeader);  // will place required size of buffer in pcbSize

            //DeviceTester::staticMsg = "Get Raw Return Value: " + QString::number(getRawReturnValue) +
            //        "\n\tlparam for button pressed: "+QString::number(lParam);

            LPBYTE rawBytePointer = new BYTE[pcbSize];
            if (rawBytePointer == NULL){  // if no data was passed
                delete[] rawBytePointer;
                return 0;
            }
            if ((getRawReturnValue = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, rawBytePointer, &pcbSize, cbSizeHeader)) != pcbSize){
                qDebug() << "Incorrect size returned from GetRawInputData call";
            }
            RAWINPUT* raw = (RAWINPUT*)rawBytePointer;
            DWORD numberOfHIDInputs = 0;
            DWORD sizeOfEachInputInBytes = 0;
            DWORD totalSizeOfRawDataInput = 0;

            //DeviceTester::byteValue = "";
            if (raw->header.dwType == RIM_TYPEHID){
                numberOfHIDInputs = raw->data.hid.dwCount;
                sizeOfEachInputInBytes = raw->data.hid.dwSizeHid;
                totalSizeOfRawDataInput = numberOfHIDInputs * sizeOfEachInputInBytes;

                if (totalSizeOfRawDataInput == 15){  // then the input is from the wheel
                    for ( ; DeviceTester::count<15; DeviceTester::count++){
                        DeviceTester::wheelValues[DeviceTester::count] = raw->data.hid.bRawData[DeviceTester::count];
                    }
                }
                else if (totalSizeOfRawDataInput == 12){  // then the input is from the shifter
                    for ( ; DeviceTester::count<12; DeviceTester::count++){
                        DeviceTester::shifterValues[DeviceTester::count] = raw->data.hid.bRawData[DeviceTester::count];
                    }
                }
                else {
                    qDebug() << "Not supposed to be here";
                }
                /*
                DeviceTester::byteData = "Total size of raw input byte array: "+QString::number(totalSizeOfRawDataInput) +
                        "\n\tBased on number of HID inputs being: "+QString::number(numberOfHIDInputs)+
                        "\n\tAnd the size of each input in bytes: "+QString::number(sizeOfEachInputInBytes);
                */
            }
            else {
                qDebug() << "Input not detected as HID Type";
            }
            //DefWindowProc(hwnd, uMsg, wParam, lParam);
            delete[] rawBytePointer;
            return 0;
        }
        case WM_DESTROY:
            qDebug() << "Destroyed";
            PostQuitMessage(42);  // ends the system created message loop by posting a WM_QUIT message to the thread
            return 0;

       default:
            return CallWindowProc((WNDPROC)DeviceTester::oldWindowProc, hwnd, uMsg, wParam, lParam); //DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void DeviceTester::updateText(){
    if (flipFlopChange != DeviceTester::flipFlop){  // happens when a message changes the static boolean
        flipFlopChange = DeviceTester::flipFlop;
        // update wheel values
        ui->lineEditWheelByte1->setText(QString::number(DeviceTester::wheelValues[0],2).rightJustified(8, '0'));
        ui->lineEditWheelByte2->setText(QString::number(DeviceTester::wheelValues[1],2).rightJustified(8, '0')); // steering LSB
        ui->lineEditWheelByte3->setText(QString::number(DeviceTester::wheelValues[2],2).rightJustified(8, '0')); // steering MSB
        ui->lineEditWheelHexLSB->setText(QString::number(DeviceTester::wheelValues[1], 16));
        ui->lineEditWheelHexMSB->setText(QString::number(DeviceTester::wheelValues[2], 16));
        uint16_t wheelDecValue = (DeviceTester::wheelValues[2] * 256) + DeviceTester::wheelValues[1];
        ui->lineEditWheelDec->setText(QString::number(wheelDecValue));
        ui->horizontalSlider->setValue(wheelDecValue);
        ui->lineEditWheelByte4->setText(QString::number(DeviceTester::wheelValues[3],2).rightJustified(8, '0')); // brake LSB
        ui->lineEditWheelByte5->setText(QString::number(DeviceTester::wheelValues[4],2).rightJustified(8, '0')); // brake MSB
        uint16_t brakeDecValue = (DeviceTester::wheelValues[4] * 256) + DeviceTester::wheelValues[3];
        ui->lineEditBrake->setText(QString::number(brakeDecValue));
        ui->lineEditWheelByte6->setText(QString::number(DeviceTester::wheelValues[5],2).rightJustified(8, '0')); // gas LSB
        ui->lineEditWheelByte7->setText(QString::number(DeviceTester::wheelValues[6],2).rightJustified(8, '0')); // gas MSB
        uint16_t gasDecValue = (DeviceTester::wheelValues[6] * 256) + DeviceTester::wheelValues[5];
        ui->lineEditGas->setText(QString::number(gasDecValue));
        ui->lineEditWheelByte8->setText(QString::number(DeviceTester::wheelValues[7],2).rightJustified(8, '0')); // clutch LSB
        ui->lineEditWheelByte9->setText(QString::number(DeviceTester::wheelValues[8],2).rightJustified(8, '0')); // clutch MSB
        uint16_t clutchDecValue = (DeviceTester::wheelValues[8] * 256) + DeviceTester::wheelValues[7];
        ui->lineEditClutch->setText(QString::number(clutchDecValue));
        ui->lineEditWheelByte10->setText(QString::number(DeviceTester::wheelValues[9],2).rightJustified(8, '0'));
        ui->lineEditWheelByte11->setText(QString::number(DeviceTester::wheelValues[10],2).rightJustified(8, '0'));
        ui->lineEditWheelByte12->setText(QString::number(DeviceTester::wheelValues[11],2).rightJustified(8, '0'));
        ui->lineEditWheelByte13->setText(QString::number(DeviceTester::wheelValues[12],2).rightJustified(8, '0'));
        ui->lineEditWheelByte14->setText(QString::number(DeviceTester::wheelValues[13],2).rightJustified(8, '0'));
        ui->lineEditWheelByte15->setText(QString::number(DeviceTester::wheelValues[14],2).rightJustified(8, '0'));

        // update shifter values
        ui->lineEditShiftByte1->setText(QString::number(DeviceTester::shifterValues[0],2).rightJustified(8, '0'));
        ui->lineEditShiftByte2->setText(QString::number(DeviceTester::shifterValues[1],2).rightJustified(8, '0'));
        ui->lineEditShiftByte3->setText(QString::number(DeviceTester::shifterValues[2],2).rightJustified(8, '0'));
        ui->lineEditShiftByte4->setText(QString::number(DeviceTester::shifterValues[3],2).rightJustified(8, '0'));
        ui->lineEditShiftByte5->setText(QString::number(DeviceTester::shifterValues[4],2).rightJustified(8, '0'));
        ui->lineEditShiftByte6->setText(QString::number(DeviceTester::shifterValues[5],2).rightJustified(8, '0'));
        ui->lineEditShiftByte7->setText(QString::number(DeviceTester::shifterValues[6],2).rightJustified(8, '0'));
        ui->lineEditShiftByte8->setText(QString::number(DeviceTester::shifterValues[7],2).rightJustified(8, '0'));
        ui->lineEditShiftByte9->setText(QString::number(DeviceTester::shifterValues[8],2).rightJustified(8, '0'));
        ui->lineEditShiftByte10->setText(QString::number(DeviceTester::shifterValues[9],2).rightJustified(8, '0'));
        ui->lineEditShiftByte11->setText(QString::number(DeviceTester::shifterValues[10],2).rightJustified(8, '0'));
        ui->lineEditShiftByte12->setText(QString::number(DeviceTester::shifterValues[11],2).rightJustified(8, '0'));
        switch (DeviceTester::shifterValues[1]){
        case 1:
            ui->radioButton_1->setChecked(true);
            break;
        case 2:
            ui->radioButton_2->setChecked(true);
            break;
        case 4:
            ui->radioButton_3->setChecked(true);
            break;
        case 8:
            ui->radioButton_4->setChecked(true);
            break;
        case 16:
            ui->radioButton_5->setChecked(true);
            break;
        case 32:
            ui->radioButton_6->setChecked(true);
            break;
        case 64:
            ui->radioButton_7->setChecked(true);
            break;
        case 128:
            ui->radioButton_R->setChecked(true);
            break;
        case 0:
            ui->radioButton_N->setChecked(true); ;
            break;
        default:
            break;
        }

        /*
        ui->textBrowser->append(DeviceTester::staticMsg);
        ui->textBrowser->append(DeviceTester::byteData);
        ui->textBrowser->append(DeviceTester::byteValue);
        */
    }
}

/*
void DeviceTester::errorExit(LPTSTR lpszFunction){
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &lpMsgBuf, 0, NULL );

    lpDisplayBuf = (LPVOID) LocalAlloc(LMEM_ZEROINIT,
                   (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));

    StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                    TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf);

    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(dw);
}
*/

void DeviceTester::registerControllers(){

    UINT nDevices;
    UINT cbSize; // size of a rawInputDevice structure
    int returnValue;
    // PRAWINPUTDEVICE pRawInputDevices;

    cbSize = sizeof(RAWINPUTDEVICE);

    returnValue = GetRegisteredRawInputDevices(NULL, &nDevices, cbSize);

    if (returnValue >= 0) {  //if successful
        ui->textBrowser->append("Registered Devices Found: " + QString::number(nDevices) );
    }

    // trying to register devices only knowing the usage page information
    PRAWINPUTDEVICE testDevice = (PRAWINPUTDEVICE) malloc(sizeof(RAWINPUTDEVICE));
    testDevice->dwFlags=0;
    testDevice->hwndTarget = NULL;
    testDevice->usUsage = 4;
    testDevice->usUsagePage = 1;
    RegisterRawInputDevices(testDevice, 1, sizeof(RAWINPUTDEVICE));
    returnValue = GetRegisteredRawInputDevices(NULL, &nDevices, cbSize);
    if (returnValue >= 0) {  //if successful
        ui->textBrowser->append("Registered Devices Found After Test: " + QString::number(nDevices) );
    }


}
