PROG1=SGSmaster
PROG2=GWInfo
PROG3=cpm70_agent_standalone
PROG5=aemdra_agent_standalone
PROG6=SGSlogger
PROG8=FakeCollector


TEST1=openSql
TEST2=createSqlTable
TEST3=insertSqlData
TEST4=selectOperation
TEST5=updateOperation
TEST6=deleteOperation
TEST7=writesgs

# flag for paho mqtt [ -rdynamic -lpaho-mqtt3a ] 
# flag for cJSON  [ -lm ]
# flag -lsqlite3
 
# ARM core (soft)

#CC=arm-linux-gnueabi-gcc

# x86 core

CC=gcc

all:

	#$(CC) -o ../../bin/$(PROG1) $(PROG1).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../protocol/SGSmodbus.c -pthread -lm #`pkg-config --cflags --libs libmodbus`

	#$(CC) -o ../../bin/$(PROG2) ../collector/$(PROG2).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../protocol/SGSmodbus.c -pthread -lm # `pkg-config --cflags --libs libmodbus`

	$(CC) -o ../../bin/cpm70_agent ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../collector/$(PROG3).c ../thirdparty/cJSON.c -pthread -lm  

	$(CC) -o ../../bin/aemdra_agent ../collector/$(PROG5).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../thirdparty/cJSON.c  -pthread -lm

	#$(CC) -o ../../bin/$(PROG6) ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../log/SGSlogfile.c ../thirdparty/sqlite3.c $(PROG6).c -pthread -lm -ldl

	#$(CC) -o ../../bin/$(PROG8) ../collector/$(PROG8).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../protocol/SGSmodbus.c -pthread -lm `pkg-config --cflags --libs libmodbus`

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
