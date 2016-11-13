#-------------------------------------------------
#
# Project created by QtCreator 2016-11-11T16:31:24
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = userManagerExample
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui

include($$PWD/../../src/usrmanager.pri)
