PROG1=SGSmaster
PROG2=SGSdebugger



all:

	$(CC) -o ../../bin/$(PROG1) $(PROG1).c ../ipcs/SGSipcs.c ../definition/SGSdefinitions.c ../protocol/SGSmodbus.c -pthread -lm `pkg-config --cflags --libs libmodbus`


clean:
	rm -rf *.o
	rm -rf ../bin/$(PROG1)




