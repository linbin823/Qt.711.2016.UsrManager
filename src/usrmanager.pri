QT       += core network

SOURCES +=\
    $$PWD/usrinfo.cpp \
    $$PWD/usrmanager.cpp

HEADERS  +=\
    $$PWD/usrinfo.h \
    $$PWD/usrmanager.h \
    $$PWD/iusrmanage.h \
    $$PWD/iusrauthorize.h

INCLUDEPATH += $$PWD\

DISTFILES += \
    $$PWD/../HISTORY.txt \
    $$PWD/../README.txt

include($$PWD/../lib/loadSaveProcessor/src/loadsaveProcessor.pri)
