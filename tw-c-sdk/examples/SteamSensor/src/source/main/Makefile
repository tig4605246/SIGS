PROG1=SGSmaster
PROG2=GWInfo
PROG3=cpm70_agent
PROG4=MQTTAsync_sub
PROG5=cpm70_mongo

# flag for paho mqtt [ -rdynamic -lpaho-mqtt3a ] 
# flag for cJSON  [ -lm ]



all:

	$(CC) -o ../../bin/$(PROG1) $(PROG1).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../protocol/SGSmodbus.c -pthread -lm `pkg-config --cflags --libs libmodbus`

	$(CC) -o ../../bin/$(PROG2) ../collector/$(PROG2).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../protocol/SGSmodbus.c -pthread -lm `pkg-config --cflags --libs libmodbus`

	$(CC) -o ../../bin/$(PROG3) ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../collector/$(PROG3).c -pthread -lm

	#$(CC) -o ../../bin/$(PROG4) ../collector/$(PROG4).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c  -pthread -rdynamic -lpaho-mqtt3a  

	$(CC) -o ../../bin/$(PROG5) ../uploader/$(PROG5).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../controlling/SGScontrol.c ../thirdparty/cJSON.c  -pthread -lm -lcurl 

clean:

	rm -rf *.o
	rm -rf ../bin/$(PROG1)