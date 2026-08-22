QT += core gui qml quick quickcontrols2 network

linux:!android {
    QT += dbus
    CONFIG += link_pkgconfig
}

macx {
    QMAKE_INFO_PLIST = macos/Info.plist
    CONFIG += app_bundle sdk_no_version_check
    TARGET = OMAStocks
}

win32 {
    TARGET = OMAStocks
}

android {
    TARGET = omastocks
    ANDROID_ABIS = arm64-v8a
    ANDROID_MIN_SDK_VERSION = 26
    ANDROID_TARGET_SDK_VERSION = 34
    ANDROID_VERSION_CODE = 1
    ANDROID_VERSION_NAME = 1.0.0
    QMAKE_TARGET_SDK_VERSION = $$ANDROID_TARGET_SDK_VERSION
    QMAKE_ANDROID_MIN_SDK_VERSION = $$ANDROID_MIN_SDK_VERSION
    QMAKE_ANDROID_TARGET_SDK_VERSION = $$ANDROID_TARGET_SDK_VERSION
    DISTFILES += \
        android/AndroidManifest.xml \
        android/build.gradle \
        android/gradle.properties
}

CONFIG += c++17 release
TEMPLATE = app

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
