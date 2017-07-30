# Self-Cared Gateway Software

#### Last edited by tig4605246 ---2017/07/30
---------------------------------------


This branch is for the Solar Project

The core structure of the software is complete. For now it still needs some test with collectors and uploaders. 

The following picture is a n explanation of the structure:

[[https://github.com/tig4605246/SIGS/blob/master/image/SGS_Structure_20170705.png|alt=SGS_Structure]]



* All codes (Current Status)
  * Core:
    * SGSmaster         [done]
    * UploaderMaster    [done]
    * DataBufferMaster  [done]
    * CollectorMaster   [done]
    * EventHandler      [done]
    * MailingAgent      [done]
    * Logger            [done]
  * Agents:
    * SolarPost         [done]
    * SolarPut          [undone]
    * GWInfo            [done]
    * SolarCollect      [undone]

* Future Plan:
  * Stability :
    * Run tests with all funtions
    * Test all situations
  * Flexibility :
    * Compatibility :
      * Protocols
      * Functions
      * Platforms
  * Maintainence
    * Version update
    * Error Report 
    * Source Code Readibility
  * Release:
    * SDK for custimized programs



[Wiki](https://github.com/tig4605246/SIGS/wiki)


## Requirements

* External Libraries
  * [libmodbus 3.0.6](https://github.com/stephane/libmodbus)
  * [libcurl](https://curl.haxx.se/)
  * [cJSON](https://github.com/DaveGamble/cJSON)
  * [sqlite3](https://www.sqlite.org/download.html)
* Packages Needed
  * [sendmail](https://www.proofpoint.com/us/products/sendmail-sentrion) __Not in use__
  * [OpenSSH](https://www.openssh.com/)





