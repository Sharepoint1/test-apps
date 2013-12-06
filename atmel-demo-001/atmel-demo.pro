TEMPLATE = app
TARGET = atmel-demo

CONFIG += qt
QT = core gui

QMAKE_CXXFLAGS_RELEASE += -Wall -Wextra

HEADERS += \
	mainwindow.h \
	videoworker.h \


SOURCES += \
	main.cpp \
	mainwindow.cpp \
	videoworker.cpp \

RESOURCES = atmel-demo.qrc

QMAKE_RESOURCE_FLAGS += -compress 0
