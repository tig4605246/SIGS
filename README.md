# Simple Gateway Software

#### Last edited by Kevin ---2017/03/12
---------------------------------------

## Purpose

  Wrap on some Open source libraries to make a simple IoT Gateway Software
  The final goal is to provide SDK for others to develop their own data collector.

## Include

* External Libraries
  * [libmodbus 3.0.6](https://github.com/stephane/libmodbus)
  * [libcurl](https://curl.haxx.se/)
  * [cJSON](https://github.com/DaveGamble/cJSON)
* Packages Needed
  * [sendmail](https://www.proofpoint.com/us/products/sendmail-sentrion) __Not in use right now__
  * [OpenSSH](https://www.openssh.com/)

## Compile

  use the __MAKEFILE__ in the source file

## Execute

  Run __sgsMaster__  to start everything
	 
## Structure

### Files

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

### Programs

the basic programs that runs the system

* __SGSmaster.c__ (Main Progress)
  * Initializes the ipcs and exec the processes that the name is mentioned in the device.conf.

* __SGSbacker.c__ (Log data maker)
  * Records log every 30 seconds. (store at ./datalogs/)
  asdfoajsfo;idasjf

	
