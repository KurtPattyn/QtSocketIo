QT       += core network
QT       += websockets
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle
CONFIG	 += no_keywords
CONFIG	 += c++11

SOURCES += \
    socketioclient.cpp

HEADERS += \
    socketioclient.h
