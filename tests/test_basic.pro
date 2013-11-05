TEMPLATE    = app
TARGET      = test_basic
QT          += core

HEADERS += obdreftest.h
SOURCES += obdreftest.cpp test_basic.cpp

# obdref lib
PATH_OBDREF = ../libobdref

INCLUDEPATH += $${PATH_OBDREF}

HEADERS += \
    $${PATH_OBDREF}/pugixml/pugiconfig.hpp \
    $${PATH_OBDREF}/duktape/duktape.h \
    $${PATH_OBDREF}/pugixml/pugixml.hpp \
    $${PATH_OBDREF}/obdrefdebug.h \
    $${PATH_OBDREF}/datatypes.h \
    $${PATH_OBDREF}/parser.h

SOURCES += \
    $${PATH_OBDREF}/pugixml/pugixml.cpp \
    $${PATH_OBDREF}/duktape/duktape.c \
    $${PATH_OBDREF}/obdrefdebug.cpp \
    $${PATH_OBDREF}/parser.cpp

DEFINES += OBDREF_DEBUG_QDEBUG
