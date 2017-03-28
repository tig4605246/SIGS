PROG1=SGSmaster
PROG2=GWInfo
PROG3=testCallFunc
PROG4=MQTTAsync_sub
PROG5=mongoagent

# flag for paho mqtt  -rdynamic -lpaho-mqtt3a 

all:

	$(CC) -o ../../bin/$(PROG1) $(PROG1).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../protocol/SGSmodbus.c -pthread -lm `pkg-config --cflags --libs libmodbus`

	$(CC) -o ../../bin/$(PROG2) ../collector/$(PROG2).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../protocol/SGSmodbus.c -pthread -lm `pkg-config --cflags --libs libmodbus`

	$(CC) -o ../../bin/$(PROG3) $(PROG3).c

	$(CC) -o ../../bin/$(PROG4) ../collector/$(PROG4).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c  -pthread -rdynamic -lpaho-mqtt3a  
	$(CC) -o ../../bin/$(PROG4) ../collector/$(PROG4).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../thirdparty/cJSON.c  -pthread -lm -lcurl -rdynamic -lpaho-mqtt3a

clean:
	rm -rf *.o
	rm -rf ../bin/$(PROG1)




