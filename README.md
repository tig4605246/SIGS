#Simple Gateway Software

####Last edited by Kevin ---2017/03/01


###Purpose
-------------------------

* Wrap on some Open source libraries to make a simple IoT Gateway Software
			
-------------------------

###Include
-------------------------

* External Libraries
  * [libmodbus 3.0.6](https://github.com/stephane/libmodbus)
  * [libcurl](https://curl.haxx.se/)
  * [cJSON](https://github.com/DaveGamble/cJSON)
* Packages Needed
  * [sendmail](https://www.proofpoint.com/us/products/sendmail-sentrion) __Not in use right now__
  * [OpenSSH](https://www.openssh.com/)

	
-------------------------

###Compile
-------------------------

* simply run gcc
	

------------------------------ 
###Execute
------------------------------
* Run sgsMaster 
	


------------------------------ 
###Structure
------------------------------
* __sgsMaster__ (Parent Progress)
  * init the ipcs and exec the processes that the name is mentioned in the device.conf

* __sgsBacker__ (Log data maker)
  * record log every 30 seconds (store at ./datalogs/)
	
