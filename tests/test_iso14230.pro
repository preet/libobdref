TEMPLATE = app
TARGET = test_iso14230
QT += core

SOURCES += test_iso14230.cpp

# obdref lib
INCLUDEPATH += $${OUT_PWD}/..
LIBS += -L$${OUT_PWD}/.. -lobdref -lv8
