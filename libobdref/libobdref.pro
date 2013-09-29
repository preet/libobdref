TEMPLATE = lib
CONFIG += staticlib
TARGET = obdref
DESTDIR = $${OUT_PWD}/lib
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

headerfiles.path = $${OUT_PWD}/include
headerfiles.files = parser.h \
                    message.h

pheaderfiles.path = $${OUT_PWD}/include/pugixml
pheaderfiles.files = pugixml/pugixml.hpp \
                     pugixml/pugiconfig.hpp
							
INSTALLS += headerfiles pheaderfiles
