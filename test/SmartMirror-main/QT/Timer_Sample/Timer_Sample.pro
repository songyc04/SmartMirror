QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Timer_Sample
VERSION = 0.1
TEMPLATE = app

QT += widgets network

CONFIG += c++17


DEFINES += QT_DEPRECATED_WARNINGS


SOURCES += \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        mainwindow.h

FORMS += \
        mainwindow.ui
CONFIG += no_keywords
