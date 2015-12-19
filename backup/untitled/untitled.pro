TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp
QMAKE_CXXFLAGS += -std=c++11
QMAKE_CXXFLAGS += -pthread
LIBS += -pthread
include(deployment.pri)
qtcAddDeployment()




