TEMPLATE = lib
CONFIG += staticlib
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

H_INSTALLPATH = /home/preet/Dev/build/obdref/obdref
L_INSTALLPATH = /home/preet/Dev/build/obdref
DEFINES += H_INSTALLPATH=\\\"/home/preet/Dev/build/obdref/obdref\\\"

headerfiles.path = $${H_INSTALLPATH}
headerfiles.files = parser.h \
                    message.hpp \
                    globals.js

pheaderfiles.path = $${H_INSTALLPATH}/pugixml
pheaderfiles.files = pugixml/pugixml.hpp \
                     pugixml/pugiconfig.hpp
							
target.path = $${L_INSTALLPATH}
INSTALLS += headerfiles pheaderfiles target
