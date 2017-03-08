#Simple Gateway Software

####Last edited by Kevin ---2017/03/01

-------------------------
###Purpose
-------------------------
* Wrap on some Open source libraries to make a simple IoT Gateway Software
			
-------------------------
###Include
-------------------------
* External Libraries
  * libmodbus 3.0.6 [Link to original wirter](https://github.com/stephane/libmodbus)
  * libcurl [Link to original writer](https://curl.haxx.se/)
  * cJSON [Link to original writer](https://github.com/DaveGamble/cJSON)
* Packages Needed
  * sendmail 
  * ssh (reverse tunnel)

	
-------------------------
###compile
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
	
