#Simple Gateway Software

####Last edited by Kevin ---2017/03/08
---------------------------------------

##Purpose

  Wrap on some Open source libraries to make a simple IoT Gateway Software

##Include

* External Libraries
  * [libmodbus 3.0.6](https://github.com/stephane/libmodbus)
  * [libcurl](https://curl.haxx.se/)
  * [cJSON](https://github.com/DaveGamble/cJSON)
* Packages Needed
  * [sendmail](https://www.proofpoint.com/us/products/sendmail-sentrion) __Not in use right now__
  * [OpenSSH](https://www.openssh.com/)

##Compile

  simply run __gcc -o target target.c cJSON.c `-lm` `-pthread` `-lcurl`__

##Execute

  Run __sgsMaster `-a`__ in auto mode, or simply __SGSmaster__ in manual mode 
	 
##Structure

###Headers
* __SGSdefinitions.h__
  * Defining parameters and structs.

* __SGScontrol.h__
  * Functions for controlling the sub processes

* __SGSipcs.h__
  * Wrap up funtions for handling shared memory and message queue at here

###Daemons

* __SGSmaster.c__ (Parent Progress)
  * Initializes the ipcs and exec the processes that the name is mentioned in the device.conf.

* __SGSbacker.c__ (Log data maker)
  * Records log every 30 seconds. (store at ./datalogs/)

* __SGScollector.c__ (system infomation)
  * Gets system info and stores to the shared memory

* __SGSuploader.c__ (upload data to mongo server )
  * Gets data from shared memory and uploads to server
	
