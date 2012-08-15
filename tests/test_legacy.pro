TEMPLATE = app
TARGET = test_legacy
QT += core

SOURCES += test_legacy.cpp

# obdref lib
INCLUDEPATH += $${OUT_PWD}/..
LIBS += -L$${OUT_PWD}/.. -lobdref -lv8
