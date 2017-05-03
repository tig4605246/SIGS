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
#include <stdint.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SGSmodbus.h"

int handle, ser_debug;

int errno;

const unsigned char auchCRCHi[] = {
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40
};

const char auchCRCLo[] = {
0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
0x40
};

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

    return handle;
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

int sgsSendModbusCommandRTU(unsigned char *cmd, int cmd_len, int delay, unsigned char *respond)
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
