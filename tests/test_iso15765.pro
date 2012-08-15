TEMPLATE = app
TARGET = test_iso15765
QT += core

SOURCES += test_iso15765.cpp

# obdref lib
INCLUDEPATH += $${OUT_PWD}/..
LIBS += -L$${OUT_PWD}/.. -lobdref -lv8
