#-------------------------------------------------
#
# Project created by QtCreator 2016-10-05T17:09:23
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = svn-commit-tool
TEMPLATE = app

CONFIG -= app_bundle

SOURCES += main.cpp\
        commit_dialog.cpp

HEADERS  += commit_dialog.h

FORMS    += commit_dialog.ui

RESOURCES += \
    resources.qrc
