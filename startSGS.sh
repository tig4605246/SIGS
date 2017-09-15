#!/bin/sh

#set up directories and start up the SEG 

SGSPATH=/home/ntust/SIGS/

#The name for distinguishing every GW

GWID=TestGateway

case "$1" in

  start)

    echo ${GWID}>/run/GW_ID

    echo -n "Starting cpm70 and aemdra agents: \n"
          cd ${SGSPATH}bin/

          ./SGSmaster --SmartCampus &


 	;;

  stop)

    echo -n "Stoping cpm70 and aemdra agents: \n"
          SGSmaster_PID=`cat ${SGSPATH}bin/pid/SGSmaster.pid`
          kill -2 ${SGSmaster_PID}
         

 	;;

esac



return 0
