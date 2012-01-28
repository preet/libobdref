TEMPLATE = lib
CONFIG += staticlib
TARGET = obdref
QT += core

SOURCES += \
    parser.cpp \
    pugixml/pugixml.cpp \
    muparser/muParserTokenReader.cpp \
    muparser/muParserError.cpp \
    muparser/muParserCallback.cpp \
    muparser/muParserBytecode.cpp \
    muparser/muParserBase.cpp \
    muparser/muParser.cpp \

HEADERS += \
    parser.h \
    message.hpp \
    pugixml/pugixml.hpp \
    pugixml/pugiconfig.hpp \
    muparser/muParserTokenReader.h \
    muparser/muParserToken.h \
    muparser/muParserTemplateMagic.h \
    muparser/muParserStack.h \
    muparser/muParserFixes.h \
    muparser/muParserError.h \
    muparser/muParserDef.h \
    muparser/muParserCallback.h \
    muparser/muParserBytecode.h \
    muparser/muParserBase.h \
    muparser/muParser.h

headerfiles.path = /usr/local/include/obdref
headerfiles.files = parser.h \
                    message.hpp

mheaderfiles.path = /usr/local/include/obdref/muparser
mheaderfiles.files = muparser/muParserTokenReader.h \
                     muparser/muParserToken.h \
                     muparser/muParserTemplateMagic.h \
                     muparser/muParserStack.h \
                     muparser/muParserFixes.h \
                     muparser/muParserError.h \
                     muparser/muParserDef.h \
                     muparser/muParserCallback.h \
                     muparser/muParserBytecode.h \
                     muparser/muParserBase.h \
                     muparser/muParser.h

pheaderfiles.path = /usr/local/include/obdref/pugixml
pheaderfiles.files = pugixml/pugixml.hpp \
                     pugixml/pugiconfig.hpp
							
target.path = /usr/local/lib/obdref
INSTALLS += headerfiles pheaderfiles mheaderfiles target
