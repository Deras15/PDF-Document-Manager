QT       += core gui widgets concurrent

TARGET = PDF_Reader
TEMPLATE = app

CONFIG += c++11 link_pkgconfig
PKGCONFIG += poppler-qt5

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        pdfsearchpanel.cpp \
    custom_widgets.cpp \
    librarysidebar.cpp \
    pdfviewport.cpp

HEADERS += \
        mainwindow.h \
        pdfsearchpanel.h \
    custom_widgets.h \
    librarysidebar.h \
    pdfviewport.h
