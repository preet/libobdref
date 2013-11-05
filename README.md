### What is libobdref?
libobdref is a small library that is meant to act as a layer between 
on-board diagnostics ("OBD") interface software and higher level 
applications. It helps a developer build and parse messages 
required to communicate with a vehicle. 

An XML+JavaScript definitions file is used to define vehicle parameters, 
and can be easily extended to add support for additional parameters 
unique to different vehicles.

libobdref can parse many kinds of OBD messages but it is not necessarily fully compliant with any OBD standards.

It also hasn't been rigorously tested in the real world and there's always the chance you might send/receive erroneous data to/from your vehicle as a result.

**Use this library at your own risk!**
***
### License
libobdref is licensed under the Apache License, version 2.0. For more information see the LICENSE file.
***
### Building
The only dependencies required are Qt and STL. Two third party packages are also included with libobdref: pugixml (an xml parser) and duktape (a javascript engine); both are made available under the MIT license. See http://pugixml.org and http://duktape.org for more information.

To build the library:

    cd libobdref
    qmake libobdref.pro && make
    
To build tests:

    cd tests
    qmake tests.pro && make

Alternatively, libobdref can also be directly added to a project:

    headers:
    pugixml/pugiconfig.hpp
    pugixml/pugixml.hpp
    duktape/duktape.h
    obdrefdebug.h
    datatypes.h
    parser.h
    
    sources:
    pugixml/pugixml.cpp
    duktape/duktape.c
    obdrefdebug.cpp
    parser.cpp    

***
### Help
Check the docs folder for documentation and examples. 

You can also email me at prismatic.project__at__gmail.com

