#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <ctype.h>


#include "../protocol/SGSmodbus.h"
#include "../ipcs/SGSipcs.h"
#include "../controlling/SGScontrol.h"
#include "../events/SGSEvent.h"


int handle = 0;
int ser_debug = 0;
int sensor_fail = 0;
int msgid; // Kevin 2016/10/26

int freezer_process(struct conf_tag *my_ctag, struct Bus_tag *my_btag, unsigned char *char_rep);//typical function for collecting data

int freezer_control(struct Bus_tag my_btag, int id, int address, int value);//execute 0x06 command whenever receives queue message

void Get_Queue_Message(int msgid, struct Bus_tag my_btag){//Kevin 2016/10/26
    char buf[512];//Kevin 2016/10/26
    int id=0, address=0, value=0;
    int result; // Kevin 2016/10/26
    
    int ret;
    

    memset(buf,'\0',sizeof(buf));
    result = Recv_queue_msg(msgid,buf,1);
    if(result == 0){
        sscanf(buf,"%d,%d,%d", &id, &address, &value);
        ret = freezer_control(my_btag, id, address, value);
        
        return;
    }
    else{
        printf("no queue message\n");
        return;
    } 
}


int main(int argc, char *argv[])
{

    int ret = 0;
    int devfd = -1;
    struct sigaction act, oldact;
    deviceInfo *deviceTemp = NULL;
    

    //Recording program's pid

    ret = sgsInitControl("freezer");
    if(ret < 0)
    {

        printf("freezer aborting\n");
        return -1;

    }

    //Initialize deviceInfo and dataInfo

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("freezer aborting\n");
        return -1;

    }

    //Catch aborting signal

    act.sa_handler = (__sighandler_t)forceQuit;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGINT, &act, &oldact);

    deviceTemp = deviceInfoPtr;

    //Get deviceInfoPtr which name is taida_deltarpi

    while(deviceTemp != NULL)
    {
        if(strcmp(deviceTemp->deviceName,"freezer")
            deviceTemp = deviceTemp->next;
        else
            break;
    }
        

    if(deviceTemp == NULL)
    {

        printf("[%s][%s,%d] There's no data need to be written in the shm\n",argv[0],__FUNCTION__,__LINE__);
        forceQuit(-1);

    }

    //Open up the serial port

    devfd = sgsSetupModbusRTU(deviceTemp->interface,deviceTemp->protocolConfig);
    if(devfd < 0)
    {

        printf("[%s][%s,%d] Open port %s failed, bye bye.\n",argv[0],__FUNCTION__,__LINE__,deviceTemp->interface);
        forceQuit(-1);

    }
    else
    {

        printf("[%s][%s,%d] Open port %s successfully, configuration : %s , devfd = %d .\n",argv[0],__FUNCTION__,__LINE__,deviceTemp->interface,deviceTemp->protocolConfig,devfd);

    }


    while(1)
    {

        usleep(50000);
        ret = CollectData(deviceTemp, devfd);

    }

}

int freezer_process(dataInfo *dataTemp, int devfd)(struct conf_tag *my_ctag, struct Bus_tag *my_btag, unsigned char *char_rep)
{
    
    int i = 0, ret = 0;
    int tmp1 = 0, tmp2 = 0;
    dataLog dLog;

    if(deviceTemp == NULL || devfd <= 0)
    {

        printf("[%s,%d] parameters are not correct\n",__FUNCTION__,__LINE__);
        return -1;

    }
    

    ret = sgsSendModbusCommandRTU(dataTemp->modbusInfo.cmd,8,330000,dataTemp->modbusInfo.response);

    if(ret == -1 ){
        //printf("taida_process read value failed!!\n");
        my_ctag->fail_count++;
        if(my_ctag->fail_count > 10){
            my_ctag->dev_status = -1;
            my_ctag->response_length = 1;
            my_ctag->response[0] = 0;
        }
        return ret;
    }
    if((my_ctag->addr >= 8336 && my_ctag->addr <= 8339) ){//Little endian
        my_ctag->fail_count = 0;
        my_ctag->dev_status = 1;
        if(char_rep[3] != 0xff){
            my_ctag->response[0] = (char_rep[3]*256 + char_rep[4]) / 10;
        }
        else{
            my_ctag->response[0] = -1*( (char_rep[4] ^ 0xff) + 1 );// doing a 2th compliment
        }
            
        my_ctag->response_length = 1;
        printf("8336");
    }
    else if( (my_ctag->addr >= 8352 && my_ctag->addr <= 8354) ){
        my_ctag->fail_count = 0;
        my_ctag->dev_status = 1;
        if(char_rep[3] != 0xff){
            my_ctag->response[0] = (char_rep[3]*256 + char_rep[4]) / 10;
        }
        else{
            my_ctag->response[0] = -1*( (char_rep[4] ^ 0xff) + 1 );// doing a 2th compliment
        }
            
        my_ctag->response_length = 1;
        printf("8352");
    }
    else{
        my_ctag->fail_count = 0;
        my_ctag->dev_status = 1;
        my_ctag->response[0] = char_rep[3]*256 + char_rep[4];
        my_ctag->response_length = 1;
    }
    
    
    return ret;
}

int freezer_control(struct Bus_tag my_btag, int id, int address, int value){
    // value or address error return -1  
    // Upon success, return 0
    char buf[512];
    int count = 0;
    int check = 0;
    int j;
    int ret = 0;
    int input_error = 0;
    unsigned char char_rep[64] ;
    unsigned char command[64];
    
    memset(buf,'\0',sizeof(buf));

    switch (address){

        case 16384:
            if(value > 30 || value < -30){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16385:
            if(value > 30 || value < -30){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16386:
            if(value > 30 || value < -30){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16387:
            if(value > 30 || value < -30){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16388:
            if(value > 50 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16389:
            if(value > 50 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16390:
            if(value > 50 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16391:
            if(value > 50 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16392:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16393:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16394:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16395:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16396:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16397:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16398:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16399:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16400:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16401:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16402:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16403:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16404:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16405:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16407:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16408:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            
            break;

        case 16410:
            if(value > 30 || value < -30){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16411:
            if(value > 30 || value < -30){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16412:
            if(value > 30 || value < -30){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16413:
            if(value > 50 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16414:
            if(value > 50 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16415:
            if(value > 50 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16416:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16417:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16418:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16419:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16420:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16421:
            if(value > 60 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16422:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16423:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;
        
        case 16424:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16425:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16426:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16427:
            if(value > 10 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16406:
            if(value > 1 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16409:
            if(value > 1 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16428:
            if(value > 1 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16429:
            if(value > 1 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16430:
            if(value > 1 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16431:
            if(value > 1 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16432:
            if(value > 1 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16433:
            if(value > 120 || value < 0){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            break;

        case 16434:
            if(value > 20 || value < -20){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16435:
            if(value > 20 || value < -20){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16436:
            if(value > 20 || value < -20){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16437:
            if(value > 20 || value < -20){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16438:
            if(value > 20 || value < -20){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16439:
            if(value > 20 || value < -20){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;

        case 16440:
            if(value > 20 || value < -20){
                sprintf(buf,"Error: value is too high or too low");
                Send_queue_msg(msgid,buf,2);
                input_error = 1;
            }
            else value*=10;
            break;
        
        default:
            printf("no matched address\n");
            sprintf(buf,"no matched address");
            Send_queue_msg(msgid,buf,2);
            input_error = 1;
            break;

    }
    if(input_error != 1){
        command[0x00] = id;
        command[0x01] = 0x06;
        command[0x02] = ((address-1) & 0xff00) >> 8;
        command[0x03] = (address-1) & 0x00ff;
        if(value < 0){
            command[0x04] = 0xff;
            command[0x05] = ( value -1) ^ 0xff;
        }
        else{
            command[0x04] = (value & 0xff00) >> 8;
            command[0x05] = value & 0x00ff;
        }
                
        check = CRC16( command, 6);
        command[0x06] = (check & 0xff00) >> 8; 
        command[0x07] = check & 0x00ff;
        for(j=0;j<8;j++){
            printf("command[0x%x]: 0x%x\n",j,command[j]);
        } 
        ret = modbus_write(command,8,my_btag.delay_time,char_rep);
        printf("ret = %d\n",ret);

        memset(buf,'\0',sizeof(buf));

        if(ret>0 && input_error == 0){
            
            if(char_rep[4] == 0xff){
                printf("control success & ret = %d value = %d",ret, ((char_rep[5] & 0xff) * -10));
                sprintf(buf,"success & return: %d", ((char_rep[5] & 0xff) * -10) );
            }
            else{
                if( address > 16383 && address < 16392){
                    printf("control success & ret = %d value = %d", ret, (char_rep[5] + char_rep[4]*256)/10 );
                    sprintf(buf,"success & return: %d", (char_rep[5] + char_rep[4]*256)/10 );
                }
                else if( address > 16409 && address < 16416){
                    printf("control success & ret = %d value = %d", ret, (char_rep[5] + char_rep[4]*256)/10 );
                    sprintf(buf,"success & return: %d", (char_rep[5] + char_rep[4]*256)/10 );
                }
                else if( address > 16433 && address < 16441){
                    printf("control success & ret = %d value = %d", ret, (char_rep[5] + char_rep[4]*256)/10 );
                    sprintf(buf,"success & return: %d", (char_rep[5] + char_rep[4]*256)/10 );
                }
                else {
                    printf("control success & ret = %d value = %d", ret, (char_rep[5] + char_rep[4]*256) );
                    sprintf(buf,"success & return: %d", (char_rep[5] + char_rep[4]*256) );
                }
            }
            Send_queue_msg(msgid,buf,2);
            return 0;
        }
        else if(input_error != 0){
            //message is sent at switch
            //sprintf(buf,"input value %d is too large or too small.",value);
            //Send_queue_msg(msgid,buf,2);
        }
        else{
            sprintf(buf,"ret = %d,control failed, wrong Address (%d) or wrong Sensor ID (%d)", ret, address, id);
            Send_queue_msg(msgid,buf,2);
            //printf("\n");
        }
    }
    
    return 0;
}
        