[What is obdref?]

obdref is a small library that is meant to act as a layer between 
on-board diagnostics ("OBD") hardware drivers and higher level 
software applications. It helps a user build and parse messages 
required to communicate with a vehicle. An XML/JS hybrid definitions 
file is used to define vehicle parameters, which can be easily 
extended to add support for additional parameters and 
specifications unique to vehicle OEMs. obdref is provided 
under the terms of the MIT license.

Please note that it's very difficult to test a program like obdref 
thoroughly in the real world. Since obdref uses a definitions file, 
there's always the chance that there are a few typos, and you might 
send or receive erroneous data to/from your vehicle as a result. 
Use this library at your own risk! 
(on a related note, definitions file contributions and 
corrections are always welcome)

[Getting Help]

The wiki should contain the most up to date information on
using obdref. Check it out at:

https://github.com/canurabus/obdref/wiki

You can also email me at:
prismatic.project <at> gmail.com


[Building/Installation]

* Platforms

With the right configuration, obdref should work on any major platform, 
but I've only worked with it on Linux. I'd appreciate any 
help/feedback from those who get it running on Windows, OS/X, etc.


* Building

obdref has three external dependencies: 
- STL
- Qt 4.7+
- Google's V8 Javascript Engine

Most modern Linux distros should have precompiled packages for Qt 
and V8 (and STL should come standard). By default, obdref is 
set up to be used as a shared lib to be built by qmake. 
You can compile it as a static lib as well (though you'll 
need to do this on your own) or include obdref directly 
in another project.


* Building: Including obdref directly

Just add the following files to your project, and make 
sure to link against v8:

- message.hpp
- parser.h
- pugixml/pugixml.hpp
- pugixml/pugiconfig.hpp
- parser.cpp
- pugixml/pugixml.cpp

Note: pugixml is the library obdref uses to parse XML documents. 
Like obdref, pugixml is distributed under the MIT License. 
See http://pugixml.org/ for more information.


* Building: Installing a shared lib

There are *.pro files included with obdref, and qmake is 
used to build the Makefiles for the project. 

Here's one way to set obdref up:

canurabus@woop:~$ git clone git@github.com:canurabus/obdref.git
canurabus@woop:~$ cd obdref

// setup everything (library and test apps)
canurabus@woop:~/obdref$ qmake obdref.pro
canurabus@woop:~/obdref$ make
canurabus@woop:~/obdref$ sudo make install

// install the library only
canurabus@woop:~/obdref$ qmake obdref.pro
canurabus@woop:~/obdref$ make
canurabus@woop:~/obdref$ sudo make install

// setup the tests only
canurabus@woop:~/obdref$ cd tests
canurabus@woop:~/obdref/tests$ qmake tests.pro
canurabus@woop:~/obdref/tests$ make

// run ldconfig so the system 'knows' about obdref
canurabus@woop:~/obdref$ sudo ldconfig


* Linking obdref in your project

If you set up obdref as a shared lib, the binaries will be installed 
to /usr/local/lib and the header files will be installed 
to /usr/local/include/obdref. 
How you link against obdref is compiler specific, 
with GCC for instance, you'd pass a library flag like: '-lobdref'.
