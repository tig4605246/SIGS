#!/bin/sh

#set up directories and start up the SEG 

SGSPATH=/home/ntust/SIGS/

#The name for distinguishing every GW

GWID=ExampleID

case "$1" in

  start)

    echo ${GWID}>/run/GW_ID

    echo -n "Starting cpm70 and aemdra agents: \n"
          cd ${SGSPATH}bin/

          ./cpm70_agent &

          ./aemdra_agent &

 	;;

  stop)

    echo -n "Stoping cpm70 and aemdra agents: \n"
          cpm70_PID=`cat ${SGSPATH}bin/pid/cpm70_agent.pid`
          aemdra_PID=`cat ${SGSPATH}bin/pid/aemdra_agent.pid`
          kill -15 ${cpm70_PID} ${aemdra_PID}
         

 	;;

esac



return 0
