#!/bin/sh

#set up directories and start up the SEG 

PATH=/home/SGS/SIGS/

# case "$1" in
#   start)

# 	cd /home/kelier-nb/SGS/
# 	echo -n "Starting SGSmaster: "
#         start-stop-daemon -b -S -q -m -p /home/kelier-nb/SGS/bin/pid/SGSmaster.pid \
#             --exec /home/kelier-nb/SGS/bin/SGSmaster
#         [ $? = 0 ] && echo "OK" || echo "FAIL"

# 	;;

#   stop)

# 	cd /home/kelier-nb/SGS/
# 	echo -n "Stoping SGSmaster: "
#         start-stop-daemon -K -q -p /home/kelier-nb/SGS/bin/pid/SGSmaster.pid
#         [ $? = 0 ] && echo "OK" || echo "FAIL"

# 	;;

# esac

cd ${PATH}bin/

./SGSmaster &

return 0
