/*

    Name: Xu Xi-Ping
    Date: March 11,2017
    Last Update: March 11,2017
    Program statement: 
        1. Announce functions related to modbus protocols at here
        2. Define General variables at here 

        Turn on INCLUDE_LIBMODBUS if libmodbus is available

*/
//#define INCLUDE_LIBMODBUS 1

#include "../ipcs/SGSipcs.h"

#ifdef INCLUDE_LIBMODBUS

//#include <modbus.h> 

#endif


extern const unsigned char auchCRCHi[] ;

extern const char auchCRCLo[] ;


//Intent : Open up the seriral port for transmitting modbus datas
//Pre : Device node number and protocol parameters. For example, 0 (ttyS0) and -8n1
//Post : On success, return file descriptor of the port. Otherwise return -1 and show error messages

int sgsSetupModbusRTU(char *devname, char *parameter);

//Intent : Send modbus command by RTU mode
//Pre : Pointer to command, command length, delay time before intending to receive message, buffer for storing response message
//Post : On success, return length of the receiving data, otherwise return -1 and show the error messages 

int sgsSendModbusCommandRTU(unsigned char *cmd, int cmd_len, int delay, unsigned char *respond);

//Intent : Caculate the 2th compliment
//Pre : target pointer, length of response, buffer for result
//Post : On success, return 0. Otherwise return -1 and shows error

int sgsCaculate2thCompliment(unsigned char *response, int *response_length, int *response_int);

//Intent : Caculate the float type data
//Pre : target pointer, length of response, buffer for result
//Post : On success, return 0. Otherwise return -1 and shows error

int sgsCaculateFloatWordswap(unsigned char *response, int *response_length, int *response_int);

//Intent : Caculate the CRC
//Pre : Pointer to the data, length of the data
//Post : The result of CRC caculation

unsigned short sgsCaculateCRC(unsigned char *puchMsg, unsigned short usDataLen);


/*

    Enable libmodbus to use TCP modbus functions

*/

#ifdef INCLUDE_LIBMODBUS

//Intent : Open up the connection of TCP modbus
//Pre : destination IP and port number
//Post : On success, return pointer to modbus_t, Otherwise return NULL

modbus_t* sgsSetupModbusTCP(const char *ip, int port);

//Intent : get sensor's data from TCP modbus
//Pre : modbus_t, slave id, command, read address, read length, buffer for retuning result , This function use 0x03
//Post : On success, return payload length. Otherwise return -1 with error messages

int sgsReadModbusRegisterTCP(modbus_t *ctx, int slave, int command, int start_addr, int read_len, uint16_t *tab_reg);


//intent : handle closing and free TCP modbus resources
//Pre : pointer to modbus_t 
//Post : Nothing

void sgsCloseModbusTcp(modbus_t *ctx);

#endif