# Self-Cared Gateway Software

#### Last edited by XXP ---2017/11/01
---------------------------------------

SIGS Alpha build v1.0 

## Features

* Flexible with different protocol agents
* Sqlite3 database implemented
* Error reporting via mails; Local error & log files  

## Requirements 

* Required dependencies
  * [libmodbus 3.0.6](https://github.com/stephane/libmodbus)
  * [libcurl](https://curl.haxx.se/)
  * [cJSON](https://github.com/DaveGamble/cJSON)
  * [sqlite3](https://www.sqlite.org/download.html)
  * libssl-dev
  * [libssh2](https://www.libssh2.org/)

After installing the dependencies, cd to ~/source/main, run Makefile to compile into binary files.

## Known Issues

* Http post might stops working in some situation.
* DB locks up for some reasons.


