#!/bin/sh

#set up directories and start up the SEG 

SGSPATH=/home/SGS/SGS/

case "$1" in
   start)

 	echo -n "Starting SGSmaster: \n"
        cd ${SGSPATH}bin/

        ./cpm70_agent &

        ./aemdra_agent &

 	;;

   stop)

 	echo -n "Stoping SGSmaster: \n"
        cpm70_PID=`cat ${SGSPATH}bin/pid/cpm70_agent.pid`
        aemdra_PID=`cat ${SGSPATH}bin/pid/aemdra_agent.pid`
        kill -15 ${cpm70_PID} ${aemdra_PID}
         

 	;;

esac



return 0
