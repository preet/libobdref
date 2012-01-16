TEMPLATE = app
QT += core

SOURCES += main.cpp \
    parser.cpp \
    pugixml/pugixml.cpp

HEADERS += \
    parser.h \
    message.h \
    pugixml/pugixml.hpp \
    pugixml/pugiconfig.hpp

DEFINES += "OBDREF_DEBUG_ON"
