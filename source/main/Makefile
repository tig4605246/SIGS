PROG1=SGSmaster
PROG2=SGSeventhandler
PROG3=SGSdatabuffermaster
PROG4=SGSuploadermaster
PROG5=SGScollectormaster
PROG6=SGSlogger
PROG7=SGSmailagent
PROG8=ZKWZKSCollector
PROG9=ZKWZKSPost
PROG10=SolarPut
PROG11=GWInfo


TEST1=openSql
TEST2=createSqlTable
TEST3=insertSqlData
TEST4=selectOperation
TEST5=updateOperation
TEST6=deleteOperation
TEST7=writesgs

# mqtt flag 		[ -rdynamic -lpaho-mqtt3a ] 
# cJSON flag  		[ -lm ]
# sqlite3 flag 		[-lsqlite3 -ldl]
# curl flag 		[-lcurl]
# libmodbus flag 	[`pkg-config --cflags --libs libmodbus`]
 
# ARM core (soft)

#CC=arm-linux-gnueabi-gcc

# x86 core

CC=gcc

all:

	$(CC) -o ../../bin/$(PROG1) $(PROG1).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c -pthread -lm -lcurl #`pkg-config --cflags --libs libmodbus`

	#$(CC) -o ../../bin/$(PROG2) $(PROG2).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c -pthread -lm -lcurl

	#$(CC) -o ../../bin/$(PROG3) $(PROG3).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c -pthread -lm # `pkg-config --cflags --libs libmodbus`

	#$(CC) -o ../../bin/$(PROG4) $(PROG4).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c -pthread -lm -lcurl 

	#$(CC) -o ../../bin/$(PROG5) $(PROG5).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c -pthread -lm -lcurl 

	#$(CC) -o ../../bin/$(PROG6) ../events/SGSEvent.c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../log/SGSlogfile.c ../thirdparty/sqlite3.c $(PROG6).c -pthread -lm -ldl -lcurl

	#$(CC) -o ../../bin/$(PROG7) ./$(PROG7).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c -pthread -lm -lcurl

	$(CC) -o ../../bin/$(PROG8) ../collector/$(PROG8).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c ../protocol/SGSmodbus.c -pthread -lm -lcurl `pkg-config --cflags --libs libmodbus`

	$(CC) -o ../../bin/$(PROG9) ../uploader/$(PROG9).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c ../thirdparty/cJSON.c -pthread -lm -lcurl 

	$(CC) -o ../../bin/$(PROG10) ../uploader/$(PROG10).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c ../thirdparty/cJSON.c ../thirdparty/sqlite3.c -pthread -lm -lcurl -ldl 

	$(CC) -o ../../bin/$(PROG11) ../collector/$(PROG11).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../events/SGSEvent.c ../protocol/SGSmodbus.c -pthread -lm  -lcurl


#Below is testing functions

	#$(CC) -o ../../bin/test/$(TEST1) ../test/Sqlite3/$(TEST1).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../thirdparty/sqlite3.c -pthread -lsqlite3 -ldl
	#$(CC) -o ../../bin/test/$(TEST2) ../test/Sqlite3/$(TEST2).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../thirdparty/sqlite3.c -pthread -lsqlite3 -ldl
	#$(CC) -o ../../bin/test/$(TEST3) ../test/Sqlite3/$(TEST3).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../thirdparty/sqlite3.c -pthread -lsqlite3 -ldl
	#$(CC) -o ../../bin/test/$(TEST4) ../test/Sqlite3/$(TEST4).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../thirdparty/sqlite3.c -pthread -lsqlite3 -ldl
	#$(CC) -o ../../bin/test/$(TEST5) ../test/Sqlite3/$(TEST5).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../thirdparty/sqlite3.c -pthread -lsqlite3 -ldl
	#$(CC) -o ../../bin/test/$(TEST6) ../test/Sqlite3/$(TEST6).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../thirdparty/sqlite3.c -pthread -lsqlite3 -ldl
	#$(CC) -o ../../bin/$(TEST7) ../test/$(TEST7).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c -pthread -lm
	

clean:

	rm -rf *.o
	rm -rf ../bin/$(PROG1)
