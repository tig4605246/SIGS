#Simple Gateway Software

####Last edited by Kevin ---2017/03/01

-------------------------
###Purpose
-------------------------
* Clean Code, easy to update.
			
-------------------------
###Include
-------------------------
* libmodbus 3.0.6 (sensor agent)
* libcurl (mongo agent)
* sendmail (mailing script)
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
	
