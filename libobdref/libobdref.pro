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

H_INSTALLPATH = $${OUT_PWD}/obdref
L_INSTALLPATH = $${OUT_PWD}/
DEFINES += H_INSTALLPATH=\\\"$${H_INSTALLPATH}\\\"

headerfiles.path = $${H_INSTALLPATH}
headerfiles.files = parser.h \
                    message.hpp \
                    globals.js

pheaderfiles.path = $${H_INSTALLPATH}/pugixml
pheaderfiles.files = pugixml/pugixml.hpp \
                     pugixml/pugiconfig.hpp
							
INSTALLS += headerfiles pheaderfiles
