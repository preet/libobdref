TEMPLATE = lib
CONFIG += staticlib
TARGET = obdref

HEADERS += \
    pugixml/pugiconfig.hpp \
    duktape/duktape.h \
    pugixml/pugixml.hpp \
    obdrefdebug.h \
    datatypes.h \
    parser.h

SOURCES += \
    pugixml/pugixml.cpp \
    duktape/duktape.c \
    obdrefdebug.cpp \
    parser.cpp

DEFINES += OBDREF_DEBUG_QDEBUG
