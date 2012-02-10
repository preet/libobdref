TEMPLATE = lib
TARGET = obdref
QT += core

SOURCES += \
    parser.cpp \
    pugixml/pugixml.cpp

HEADERS += \
    parser.h \
    message.hpp \
    pugixml/pugixml.hpp \
    pugixml/pugiconfig.hpp

LIBS += -lv8

headerfiles.path = /usr/local/include/obdref
headerfiles.files = parser.h \
                    message.hpp \
                    globals.js

pheaderfiles.path = /usr/local/include/obdref/pugixml
pheaderfiles.files = pugixml/pugixml.hpp \
                     pugixml/pugiconfig.hpp
							
target.path = /usr/local/lib
INSTALLS += headerfiles pheaderfiles target

OTHER_FILES += \
    globals.js
