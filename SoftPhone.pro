QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += network

TARGET = SoftPhone
TEMPLATE = app

SOURCES += \
        main.cpp \
        SoftPhone.cpp \
        FileFactory.cpp \
        FileItem.cpp \
        Functions.cpp \
        IniFileIterator.cpp \
        IniFileLoader.cpp \
        PttAdapter.cpp \
        PttManager.cpp \
        SnmpStack.cpp \
        TCPCommunicator.cpp \
        TransportAdapter.cpp \
        UDPCommunicator.cpp \
        Utility.cpp \
        WavWriter.cpp \
        Plotter.cpp

HEADERS += \
        SoftPhone.h \
        Codecs.h \
        ConfigException.h \
        ed137_rtp.h \
        FileFactory.h \
        FileItem.h \
        FileIteratorInterface.h \
        FileLoaderInterface.h \
        IniFileIterator.h \
        IniFileLoader.h \
        IPttListener.h \
        PttAdapter.h \
        PttManager.h \
        SnmpStack.h \
        TCPCommunicator.h \
        TransportAdapter.h \
        UDPCommunicator.h \
        Utility.h \
        WavWriter.h \
        StaticVariables.h \
        Plotter.h

FORMS += \
        SoftPhone.ui

INCLUDEPATH +=usr/lib
INCLUDEPATH +=usr/local/lib

LIBS += `pkg-config --libs libpjproject`
LIBS += -lboost_system -lboost_chrono -lboost_thread -ludev
LIBS += -lnetsnmp -lasound
LIBS += -lfftw3
