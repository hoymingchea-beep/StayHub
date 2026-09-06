QT += widgets sql network multimedia

CONFIG += c++17

TARGET = StayHub
VERSION = 1.0.0
TEMPLATE = app

SOURCES += \
    main.cpp \
    splashscreen.cpp \
    database.cpp \
    roomdialog.cpp \
    logindialog.cpp \
    utilitydialog.cpp \
    telegramservice.cpp \
    clickablecard.cpp \
    incomechartwidget.cpp \
    tenantdetaildialog.cpp \
    mainwindow.cpp

HEADERS += \
    room.h \
    payment.h \
    user.h \
    utility.h \
    database.h \
    roomdialog.h \
    logindialog.h \
    utilitydialog.h \
    telegramservice.h \
    clickablecard.h \
    incomechartwidget.h \
    tenantdetaildialog.h \
    mainwindow.h \
    splashscreen.h

RESOURCES += \
    resources/app.qrc

# Sets the .exe file's own icon on Windows (taskbar, File Explorer, shortcuts).
# Ignored on other platforms.
win32: RC_ICONS = resources/icons/app_icon.ico
