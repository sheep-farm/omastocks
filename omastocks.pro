QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release link_pkgconfig
TARGET = omastocks
TEMPLATE = app

PKGCONFIG += libcurl

HEADERS += \
    src/Stock.h \
    src/StockListModel.h \
    src/SystemTheme.h \
    src/YahooFinance.h \
    src/Backend.h

SOURCES += \
    src/main.cpp \
    src/StockListModel.cpp \
    src/SystemTheme.cpp \
    src/YahooFinance.cpp \
    src/Backend.cpp

RESOURCES += src/resources.qrc
