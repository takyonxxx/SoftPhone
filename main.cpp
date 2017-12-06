#include "SoftPhone.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SoftPhone w;
    w.show();

    return a.exec();
}
