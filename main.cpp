#include "devicetester.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    DeviceTester w;
    w.show();

    return a.exec();
}
