TEMPLATE = app
QT += core

SOURCES += main.cpp \
    parser.cpp \
    pugixml/pugixml.cpp \
    muparser/muParserTokenReader.cpp \
    muparser/muParserError.cpp \
    muparser/muParserCallback.cpp \
    muparser/muParserBytecode.cpp \
    muparser/muParserBase.cpp \
    muparser/muParser.cpp

HEADERS += \
    parser.h \
    message.h \
    pugixml/pugixml.hpp \
    pugixml/pugiconfig.hpp \
    muparser/muParserTokenReader.h \
    muparser/muParserToken.h \
    muparser/muParserStack.h \
    muparser/muParserFixes.h \
    muparser/muParserError.h \
    muparser/muParserDef.h \
    muparser/muParserCallback.h \
    muparser/muParserBytecode.h \
    muparser/muParserBase.h \
    muparser/muParser.h

DEFINES += "OBDREF_DEBUG_ON"
