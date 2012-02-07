TEMPLATE = app
TARGET = test_random_input
QT += core
INCLUDEPATH += ../

SOURCES += test_random_input.cpp
LIBS += -lobdref -L../
LIBS += -lv8

