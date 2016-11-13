QT       += core network

SOURCES +=\
    $$PWD/usrinfo.cpp \
    $$PWD/usrmanager.cpp

HEADERS  +=\
    $$PWD/usrinfo.h \
    $$PWD/usrmanager.h

INCLUDEPATH += $$PWD\

DISTFILES += \
    $$PWD/../HISTORY.txt \
    $$PWD/../README.txt

include($$PWD/../lib/saveLoadProcessor/src/loadsaveProcessor.pri)
