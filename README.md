# Simple Gateway Software

#### Last edited by Kevin ---2017/04/20
---------------------------------------

###Updates
  2017/04/20 : log system structure & functions


## Purpose

    Wrap on some Open source libraries to make a simple IoT Gateway Software
  The final goal is to provide SDK for others to develop their own data collector.

## Milestones

### First Stage

* Core functions : Things that make the system can start & stop and run properly
  * definitions : structs and MACROS
  * ipcs : how processes communicate with each other
  * controlling : start & stop Uploaders or Collectors

### Second Stage

* Event-Handler & logs : Dealing with real-time events. 
  * log : Save data files for a certain period of time. (Default is 7 days)
  * Event : 

* Wrapping up : API interface and documents and test programs

## Current Structure

              | Uploaders |
  ---------------------------------------
  | ipcs | controlling | protocol | log |
  ---------------------------------------
              | Collectors |

    We can change uploaders and Collectors without revising anything in the middle.
  It can ease the pain while developing new programs for new servers and sensors.

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

* __Event__
  * Definitions of errors, events and formatted output functions
	
