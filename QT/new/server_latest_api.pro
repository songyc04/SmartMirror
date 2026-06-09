#-------------------------------------------------
#
# Project created by QtCreator 2026-05-29T11:22:11
#
#-------------------------------------------------

QT       += core gui network widgets

CONFIG   += c++11

TARGET = new
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    weatherpanel.cpp \
    newspanel.cpp \
    musicbar.cpp \
    wavewidget.cpp \
    tcpsocketworker.cpp \
    gesturesocketworker.cpp \
    musicplayerworker.cpp \
    emotionprocessworker.cpp

HEADERS += \
        mainwindow.h \
    weatherpanel.h \
    newspanel.h \
    musicbar.h \
    wavewidget.h \
    tcpsocketworker.h \
    gesturesocketworker.h \
    musicplayerworker.h \
    emotionprocessworker.h

FORMS += \
        mainwindow.ui \
    weatherpanel.ui \
    newspanel.ui \
    musicbar.ui
