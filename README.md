# Simple Gateway Software

#### Last edited by tig4605246 ---2017/04/20
---------------------------------------


## Purpose

  Wrap on some Open source libraries to make a simple IoT Gateway Software.
  The goal is to ease the pain during the development in the future.

<<<<<<< HEAD

## Current Structure

                 |-- Uploaders  : Upload data to target places
                 |
    SGSmaster--- |--   Logger   : Create formatted logs with sqlite3 db
                 |
                 |-- Collectors : Collect and parse data

  We can change uploaders and Collectors without revising anything in the middle.
  It can ease the pain while developing new programs for new servers and sensors.
=======
>>>>>>> 8d91a7117c2dfc7c0e2d8c39c243151133d8093d

## Include

* External Libraries
  * [libmodbus 3.0.6](https://github.com/stephane/libmodbus)
  * [libcurl](https://curl.haxx.se/)
  * [cJSON](https://github.com/DaveGamble/cJSON)
  * [sqlite3](https://www.sqlite.org/download.html)
* Packages Needed
  * [sendmail](https://www.proofpoint.com/us/products/sendmail-sentrion) __Not in use right now__
  * [OpenSSH](https://www.openssh.com/)

## Compile

  use the __MAKEFILE__ in the source file

## Execute

  Run __sgsMaster__ in the bin folder to start everything

## Files

* __source__

  * __definition__
    * Defining parameters and structs.

  * __controlling__
    * Functions for controlling the sub processes

  * __ipcs__
    * Wrap up funtions for handling shared memory and message queue

  * __protocol__
    * Build-in libraries that support certain protocols

  * __thirdparty__
    * All external resources are located at here

  * __log__
    * Functions do data records and managing them

  * __events__
    * Definitions of errors, events and formatted output functions

* __conf__

  * The configurations of SGS are here
