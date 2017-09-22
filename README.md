Terravox
========

[![Build Status](https://travis-ci.org/yvt/terravox.svg?branch=master)](https://travis-ci.org/yvt/terravox)

Terravox is a heightmap Editor which can be used to create a Voxlap5 VXL data quickly.

![](https://openspadesmedia.yvt.jp/misc/terravox/ss1.png)
![](https://openspadesmedia.yvt.jp/misc/terravox/shot0624.jpg)

Installation
------------

1. Make sure following development packages are installed.
  * C++ compiler. GCC 4.8 / Clang 3.2 or later is recommended.
  * Qt 5.4 or later.
  * (Optionally, LuaJIT for scripting support)
2. `qmake Terravox.pro`
3. `make`
  

Organization
------------

* (root) - contains the main source code written in C++/Qt, Qt form design, and 
           translation files.
* res - mostly image resources.
* lua - built-in Lua code providing an interface between third-party plugin 
        scripts and the application program.
