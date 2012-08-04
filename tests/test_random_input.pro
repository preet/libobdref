TEMPLATE = app
TARGET = test_random_input
QT += core

SOURCES += test_random_input.cpp

# obdref lib
INCLUDEPATH += /home/preet/Documents/obdref
LIBS += -L/home/preet/Documents/obdref -lobdref -lv8
