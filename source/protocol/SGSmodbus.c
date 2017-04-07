/*

    Name: Xu Xi-Ping
    Date: March 11,2017
    Last Update: March 11,2017
    Program statement: 
        1. Define the details of the SGSmodbus functions
        

*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <syslog.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SGSmodbus.h"

int handle, ser_debug;

int sgsSetupModbusRTU(char *devname, char *parameter)
{
    int	brate;
    struct  termios term;

    //open device

    handle = open(devname, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if(handle <= 0) 
    {

        printf("Serial_Init:: Open() failed\n");
        return -1;

    }

    //get baud rate constant from numeric value

    if(strstr(parameter, "B115200"))
    {

        if(ser_debug) printf("[%s:%d] B115200\n",__FUNCTION__,__LINE__);
        brate = B115200;

    }
    else if(strstr(parameter, "B57600"))
    {

        if(ser_debug) printf("[%s:%d] B57600\n",__FUNCTION__,__LINE__);
        brate = B57600;

    }
    else if(strstr(parameter, "B38400"))
    {

        if(ser_debug) printf("[%s:%d] B38400\n",__FUNCTION__,__LINE__);
        brate = B38400;

    }
    else if(strstr(parameter, "B19200"))
    {

        if(ser_debug) printf("[%s:%d] B19200\n",__FUNCTION__,__LINE__);
        brate = B19200;

    }
    else if(strstr(parameter, "B9600"))
    {

        if(ser_debug) printf("[%s:%d] B9600\n",__FUNCTION__,__LINE__);
        brate = B9600;

    }
    else if(strstr(parameter, "B4800"))
    {
        
        if(ser_debug) printf("[%s:%d] B4800\n",__FUNCTION__,__LINE__);
        brate = B4800;

    }
    else if(strstr(parameter, "B2400"))
    {

        if(ser_debug) printf("[%s:%d] B2400\n",__FUNCTION__,__LINE__);
        brate = B2400;

    }
    else
    {

        printf("Serial_Init:: Invalid baud rate: %s \n", parameter);
        return -1;

    }

    //get device struct

    if(tcgetattr(handle, &term) != 0) 
    {

        printf("Serial_Init:: tcgetattr() failed\n");
        return -1;

    }

    //input modes

    term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | INPCK | ISTRIP | INLCR |
                      IGNCR | ICRNL | IXON | IXOFF);
    term.c_iflag |= IGNPAR;

    //output modes 

    #if defined(FREEBSD_80)

        term.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET);

    #else

        term.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET | OFILL |
                        OFDEL | NLDLY | CRDLY | TABDLY | BSDLY | VTDLY | FFDLY);

    #endif

    //control modes

    if(strstr(parameter, "-8n1"))
    {

        if(ser_debug) printf("[%s:%d] 8n1\n",__FUNCTION__,__LINE__);

        term.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB | HUPCL | CRTSCTS);
        term.c_cflag |= CREAD | CS8 | CLOCAL;

    }
    else if(strstr(parameter, "-odd"))
    { 

        //8o1

        if(ser_debug) printf("[%s:%d] odd\n",__FUNCTION__,__LINE__);

        term.c_cflag &= ~(CSIZE | CSTOPB | HUPCL | CRTSCTS);
        term.c_cflag |= CREAD | CS8 | CLOCAL | PARENB | PARODD;

    }
    else if(strstr(parameter, "-even"))
    { 
        //8e1

        if(ser_debug) printf("[%s:%d] even\n",__FUNCTION__,__LINE__);

        term.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB | HUPCL | CRTSCTS);
        term.c_cflag |= CREAD | CS8 | CLOCAL | PARENB;

    }
    else
    { 
        // 8n2

        if(ser_debug) printf("[%s:%d] 8n2\n",__FUNCTION__,__LINE__);

        term.c_cflag &= ~(CSIZE | PARENB | PARODD | HUPCL | CRTSCTS);
        term.c_cflag |= CREAD | CS8 | CSTOPB | CLOCAL;

    }

    //local modes 

    term.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
    term.c_lflag |= NOFLSH;

    //set baud rate

    cfsetospeed(&term, brate);
    cfsetispeed(&term, brate);

    //set new device settings

    if(tcsetattr(handle, TCSANOW, &term)  != 0) 
    {

        printf("Serial_Init:: tcsetattr() failed\n");
        return -1;

    }

    //printf("Serial_Init:: success\n");

    return(0);
}

unsigned short sgsCaculateCRC(unsigned char *puchMsg, unsigned short usDataLen)
{

    /* high byte of CRC initialized */

    unsigned char uchCRCHi = 0xFF ;  

    /* low byte of CRC initialized */

    unsigned char uchCRCLo = 0xFF ;     

    /* will index into CRC lookup table */

    unsigned uIndex ;                   

    while (usDataLen--)
    {                
        
        /* pass through message buffer */
        
        uIndex = uchCRCHi ^ *puchMsg++ ;
        
        /* calculate the CRC */
        
        uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
        uchCRCLo = auchCRCLo[uIndex] ;

    }
    return (uchCRCHi << 8 | uchCRCLo) ;
    
}

int sgssendModbusCommandRTU(unsigned char *cmd, int cmd_len, int delay, unsigned char *respond)
{

    int i, ret, crc;

    //printf("[%s:%d] Prepare to send message\n", __FUNCTION__, __LINE__);

    if(ser_debug)
    {

        printf("[%s:%d] TX:", __FUNCTION__, __LINE__);
        for(i=0; i<cmd_len; i++)
            printf(" %02x", *(cmd + i));
        printf("\n");

    }

    //printf("[%s:%d] Write Start\n", __FUNCTION__, __LINE__);

    ret = write(handle, cmd, cmd_len);

    //printf("[%s:%d] write over ret is %d cmd_len is %d\n", __FUNCTION__, __LINE__,ret,cmd_len);

    usleep(delay);

    if(ret != cmd_len)
    {

        if(ser_debug)
            printf("[%s:%d] system call \"write()\" return error\n",
                       __FUNCTION__, __LINE__);
        return -1;

    }
    //printf("[%s:%d] Start Reading\n", __FUNCTION__, __LINE__);

    ret = read(handle, respond, 4096);

    //printf("[%s:%d] Read finished\n", __FUNCTION__, __LINE__);

    if(ser_debug)
    {

        if(ret > 0)
        {

            printf("[%s:%d] RX:", __FUNCTION__, __LINE__);

            for(i=0; i<ret; i++)
                printf(" %02x", *(respond + i));

            printf("\n");

        }
        printf("[%s:%d] RX Count = %d\n", __FUNCTION__, __LINE__, ret);

    }

    //printf("[%s:%d] RX check finished\n", __FUNCTION__, __LINE__);

    if(ret < 5)
    {

        if(ser_debug)
            printf("[%s:%d] ERR: invalid respond\n",
                       __FUNCTION__, __LINE__);
        return -1;

    }

    /*
    if(*(cmd+1) != *(respond+1)){
        if(ser_debug)
            printf("[%s:%d] ERR: respond func code is not the same\n",
                       __FUNCTION__, __LINE__);
        return -1;
    }
    */

    //printf("[%s:%d] Caculate CRC\n", __FUNCTION__, __LINE__);

    crc = sgsCaculateCRC(respond, ret - 2);

    if(*(respond + ret - 2) != (crc >> 8) ||*(respond + ret - 1) != (crc & 0xff))
    {

        if(ser_debug)
            printf("[%s:%d] ERR: crc16 checksum is wrong\n",__FUNCTION__, __LINE__);

        return -1;

    }

    return ret;

}

int sgsCaculate2thCompliment(unsigned char *response, int *response_length, int *response_int)
{

    unsigned char value[2]={response[3],response[4]};
    int real_integer = 0;
    int i=0;

    //if value[0] == 0xff means value is < 0

    if(value[0] == 0xff)
    {

        // doing a 2th compliment

        real_integer = -1*( (value[1] ^ 0xff) + 1 );

    }
    else
    {

        real_integer = value[0]*256 + value[1];

    }

    for(i=0;i<64;i++)
    {

        response_int[i] = 0;

    }
    *response_length = 1;
    response_int[0] = real_integer;
    return 0;

}

int sgsCaculateFloatWordswap(unsigned char *response, int *response_length, int *response_int)
{

    unsigned char value[4]={response[5],response[6],response[3],response[4]};

    //this is for real_point

    unsigned char compare = 0x80; 

    int i=0;
    float real_point = 0;

    // 2^-1 , 2^-2 , 2^-3... this is for real_point

    double Exponent = 0.5;

    int real_integer = 1;

    //this is for real_integer

    int exponent_count = 0;
    
    

    for(i=0;i<4;i++)
    {

        printf("value[%d] %d\t ",i,value[0]);

    }

    printf("\n");

    //exclude the sign bit

    value[0] = value[0] << 1;

    //AND with 1111 1110 

    value[0] = value[0] & 0xfe; 

    if((value[1] / 128) > 0 )
    {

        value[1] = value[1] << 1;

        //AND with 1111 1110 

        value[1] = value[1] & 0xfe; 
        value[0]++;

    }

    //find out real_point

    for(i=0;i<3;i++)
    {

        real_point += (value[1] & compare)*Exponent;
        compare >>= 1;
        Exponent *= Exponent;

    }
    printf("real_point %f\n",real_point);

    //make it suitable for integer

    real_point *= 10000;

    //find out real_integer

    if( (value[0] - 127) > 0 )
    {

        exponent_count =  value[0] - 127;
        for(i=0;i<exponent_count;i++)
        {

            real_integer <<= 1;

        }
        
    }
    else
    {

        real_integer = 1; //2^0 = 1

    }
    
    printf("real_integer %d\n",real_integer);

    //flush the response for containing processed value

    for(i=0;i<64;i++)
    {

        response_int[i] = 0;

    }

    *response_length = 2;
    response_int[0] = real_integer;
    response_int[1] = real_point;
    return 0;
}




#ifdef INCLUDE_LIBMODBUS

modbus_t* sgsSetupModbusTCP(const char *ip, int port)
{

    modbus_t *ctx;

    ctx = modbus_new_tcp(ip, port);

    if (ctx == NULL) 
    {

        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return NULL;

    }

    if (modbus_connect(ctx) == -1) 
    {

        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return NULL;

    }
    return ctx;

}

int sgsReadModbusRegisterTCP(modbus_t *ctx, int slave, int command, int start_addr, int read_len, uint16_t *tab_reg)
{

    int rc = 0;
    int i;

    rc = modbus_set_slave(ctx, slave);

    if (rc == -1) 
    {

        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;

    }

    if(command == 3)
        rc = modbus_read_registers(ctx, start_addr, read_len, tab_reg);

    else if(command == 4)
        rc = modbus_read_input_registers(ctx, start_addr, read_len, tab_reg);

    if (rc == -1) 
    {

        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;

    }

    for (i=0; i < rc; i++) 
    {

        printf("reg[%d]=%d (0x%X)\n", i, tab_reg[i], tab_reg[i]);

    }
    return rc;

}

void sgsCloseModbusTcp(modbus_t *ctx)
{

    modbus_close(ctx);
    modbus_free(ctx);
    return ;

}

#endif
