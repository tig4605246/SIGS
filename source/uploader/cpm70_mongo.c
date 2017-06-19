/*

    Name: Xu Xi-Ping
    Date: March 30,2017
    Last Update: March 30,2017
    Program statement: 
        Upload cpm70-agent data to server

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/timeb.h>
#include <time.h>
#include <error.h>
#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


#include "../ipcs/SGSipcs.h"
#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"
#include "../thirdparty/cJSON.h" // cJSON.c -lm

//Post definitions for max length

#define SA      struct sockaddr
#define MAXLINE 16384
#define MAXSUB  16384

deviceInfo *deviceInfoPtr = NULL;

dataInfo *dataInfoPtr = NULL;

deviceInfo *interface = NULL;

//Upload interval

int upload_interval = 30;

//I don't know what this is for

extern int h_errno;

//Server ip

char *hname = "140.118.70.136";

//Which port server is using

#define SERVERPORT 9000

//the RESTAPI 

char *page = "/smartTest";

//perform JSON operation and call curl http POST

int CreateJSONAndRunHTTPCommand(deviceInfo *targetPtr);

//Purpose : Replace curl doing Post
//Pre     : content we want to upload
//Post    : return the packets size we upload to the server (target)

ssize_t process_http( char *content);

//Intent : close program correctly
//Pre    : signal number catched by sigaction
//Post   : Nothing 

void stopAndLeave(int sigNum);

//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre    : Nothing
//Post   : On success, return 0. On error, return -1 and shows the error message

static int initializeInfo();

//Intent : Write data to socket's file descriptor properly
//Pre    : file descriptor, data buffer, data length
//Post   : On success, return 0. On error return -1.

int my_write(int fd, void *buffer, int length);

//Intent : Read data from socket's file descriptor properly
//Pre    : file descriptor, data buffer, data length
//Post   : On success, return 0. On error return -1.

int my_read(int fd, void *buffer, int length);


int main(int argc, char** argv)
{

    time_t timep, timep_t, timep_t2;
    struct sigaction act, oldact;
    char buf[64];
    int i = 0, j = 0;
    int ret = 0;
    int upload_interval = 30;
    FILE *fd;
    deviceInfo *target = deviceInfoPtr;

    openlog("cpm70_mongo", LOG_PID, LOG_USER);

    //record pid for SEG to control it


    ret = sgsInitControl("cpm70_mongo");
    if(ret < 0)
    {

        printf("cpm70_mongo aborting\n");
        return -1;

    }

    //Initialize deviceInfo and dataInfo

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("cpm70_mongo aborting\n");
        return -1;

    }

    //Catch aborting signal

    act.sa_handler = (__sighandler_t)stopAndLeave;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

    
    sleep(5);
    printf("write done\n");

    //Find where cpm70_agent stores the data

    target = deviceInfoPtr;

    while(strcmp(target->deviceName,"cpm70_agent") && target != NULL)
    {

        target = target->next;

    }

    if(target == NULL)
    {

        printf("No target, nothing to do at here, bye~\n");
        exit(0);

    }
    else
    {

         printf("target->deviceName is %s\n",target->deviceName);

    }

   

    interface = deviceInfoPtr;

    while(strcmp(interface->deviceName,"cpm70_mongo") && interface != NULL)
    {

        interface = interface->next;

    }
    if(interface == NULL)
    {

        printf("Warning : cpm70_mongo is not defined in device.conf\n");

    }
    else
    {

        printf("target->deviceName is %s\n",interface->deviceName);

    }

    //main loop 
    
    
    while(1) 
    {

        //times up

        ret = CreateJSONAndRunHTTPCommand(target);
        sleep(30);

    }

}

ssize_t process_http( char *content)
{
    
    int sockfd;
	struct sockaddr_in servaddr;
	char **pptr;
	char str[50];
	struct hostent *hptr;
	char sendline[MAXLINE + 1], recvline[MAXLINE + 1];
    int i = 0, ret = 0;
    char *error = NULL;
	ssize_t n;

    //Intialize host entity with server ip address

    if ((hptr = gethostbyname(hname)) == NULL) 
    {

		syslog(LOG_ERR, "[%s:%d] gethostbyname error for host: %s: %s", __FUNCTION__, __LINE__,hname ,hstrerror(h_errno));

		return -1;

	}

	//syslog(LOG_ERR, "[%s:%d] hostname: %s\n",__FUNCTION__,__LINE__, hptr->h_name);

    //Set up address type (FAMILY)

	if (hptr->h_addrtype == AF_INET && (pptr = hptr->h_addr_list) != NULL) 
    {

		syslog(LOG_ERR, "[%s:%d] address: %s\n",__FUNCTION__,__LINE__,inet_ntop( hptr->h_addrtype , *pptr , str , sizeof(str) ));

	} 
    else
    {

		syslog(LOG_ERR, "[%s:%d] Error call inet_ntop \n",__FUNCTION__,__LINE__);

        return -1;

	}

    //Create socket

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd == -1)
    {

        printf("socket failed, errno %d, %s\n",errno,strerror(errno));
        return -1;

    }
    else
    {

        printf("[%s,%d]socket() done\n",__FUNCTION__,__LINE__);

    }

    //Set to 0  (Initialization)

	bzero(&servaddr, sizeof(servaddr));

    //Fill in the parameters

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERVERPORT);
	inet_pton(AF_INET, str, &servaddr.sin_addr);

    //Connect to the target server

	ret = connect(sockfd, (SA *) & servaddr, sizeof(servaddr));

    if(ret == -1)
    {

        printf("connect() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    else
    {

        //printf("[%s,%d]connect() done\n",__FUNCTION__,__LINE__);

    }

    //Header content for HTTP POST

	snprintf(sendline, MAXSUB,
		 "POST %s HTTP/1.1\r\n"
		 "Host: %s\r\n"
		 "Content-type: application/json; charset=UTF-8\r\n"
         "User-Agent: Kelier/0.1\r\n"
		 "Content-Length: %u\r\n\r\n"
		 "%s", page, hname, strlen(content), content);

    //print out the content

    //printf("sendline : \n %s\n",sendline);

    //Send the packet to the server

    ret = my_write(sockfd, sendline, strlen(sendline));

    if(ret == -1)
    {

        printf("write() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    //printf("[%s,%d]Write() done\n",__FUNCTION__,__LINE__);

    //Get the result

    ret = my_read(sockfd, recvline, (sizeof(recvline) - 1));

    if(ret == -1)
    {

        printf("read() failed, errno %d, %s\n",errno,strerror(errno));
        close(sockfd);
        return -1;

    }
    //printf("[%s,%d]Read done\n",__FUNCTION__,__LINE__);

    //Check if the last sending process is a success or not

    error = strcasestr(recvline,"true");
    if(error != NULL)
    {

        syslog(LOG_ERR, "\033[1;32m""[%s,%d] Post Successfully\n""\033[1;37m",__FUNCTION__,__LINE__);
        //printf( "\033[1;32m""[%s,%d] Post Result : %s \n""\033[1;37m",__FUNCTION__,__LINE__,recvline);

    }
    else
    {

        syslog(LOG_ERR, "\033[1;31m""[%s,%d] %s\n""\033[1;37m",__FUNCTION__,__LINE__, recvline);
        //printf("\033[1;31m""[%s,%d] %s\n""\033[1;37m",__FUNCTION__,__LINE__, recvline);
        close(sockfd);
        return -1;

    }
    

    //Always release the resources

    close(sockfd);

	return n;

}

int CreateJSONAndRunHTTPCommand(deviceInfo *targetPtr)
{

    int i = 0;
    int ret=0;
    char *output = NULL;
    char *format = NULL;
    cJSON *root, *row; 
    cJSON *field;
	char *errstr;
	char Content_Length[32];
    dataInfo *dataTemp = targetPtr->dataInfoPtr;
    dataLog dLog ;

    //printf( "[%s:%d] CreateJSONAndRunHTTPCommand called\n", __FUNCTION__, __LINE__);

    // Initialize cJSON

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "rows", row=cJSON_CreateArray() );
    //syslog(LOG_ERR,"[%s:%d]  ", __FUNCTION__, __LINE__);

    /*
        ---Stuff cJSON at here---
    */

    //Loop until everything is stuffed

    while(dataTemp != NULL)
    {

        //Create an object inside the "rows"

        cJSON_AddItemToArray(row, field=cJSON_CreateObject());
        syslog(LOG_ERR,"[%s:%d]  ", __FUNCTION__, __LINE__);
        
        //initialize dLog->value.s

        memset(dLog.value.s,'\0',sizeof(dLog.value.s));
        dLog.valueType = STRING_VALUE;

        //Get data from shared memory

        sgsReadSharedMemory(dataTemp,&dLog);

        //Put ID into the JSON file

        cJSON_AddStringToObject(field,dataTemp->valueName,dLog.value.s);

        //printf("[%s:%d]  dataTemp-valueName %s \n", __FUNCTION__, __LINE__,dataTemp->valueName);
        dataTemp = dataTemp->next;
        

        //Put rest of the data which are from same sensor into the JSON Object
        //If we meet a new ID, then create a new Object

        while( dataTemp != NULL)
        {

            if(!strcmp(dataTemp->valueName,"ID"))
                break;

            //syslog(LOG_ERR,"[%s:%d] dataTemp->valueName %s", __FUNCTION__, __LINE__,dataTemp->valueName);
            
            memset(dLog.value.s,'\0',sizeof(dLog.value.s));
            dLog.valueType = STRING_VALUE;

            //Get data from shared memory

            sgsReadSharedMemory(dataTemp,&dLog);

            //Put data into the JSON Object

            cJSON_AddStringToObject(field,dataTemp->valueName,dLog.value.s);

            //syslog(LOG_ERR,"[%s:%d] dataTemp->valueName %s", __FUNCTION__, __LINE__,dataTemp->valueName);
            
            dataTemp = dataTemp->next;
            
            
        }

    }

    //printf("[%s:%d] cJSON done \n", __FUNCTION__, __LINE__);

    // We check the JSON data at here

    output = cJSON_PrintUnformatted(root);

    format = cJSON_Print(root);

    // Print the JSON data        

    //printf("%s\n",output);
    // Adding curl http options

    sprintf(Content_Length,"Content-Length: %o",50);
    //printf("%s\n",Content_Length);

    ret = process_http(output);

    // Clean JSON pointer

    cJSON_Delete(root); 
    
    free(format);
    free(output);
    //printf("[%s:%d] Finished\n", __FUNCTION__, __LINE__);
    return ret;

}

void stopAndLeave(int sigNum)
{

    sgsDeleteAll(deviceInfoPtr, -1);
    printf("cpm70_mongo is quitting...\n");
    exit(0);

}

static int initializeInfo()
{

    int ret = 0;
    ret = sgsInitDeviceInfo(&deviceInfoPtr);
    if(ret != 0)
    {

        printf("[%s,%d] init device conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    } 

    ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 0);
    if(ret == -1) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    

    return 0;

}

int my_write(int fd, void *buffer, int length)
{

    int bytes_left;
    int written_bytes;
    char *ptr;

    ptr=buffer;
    bytes_left=length;

    while(bytes_left>0)
    {
            
            //printf("Write loop\n");
            written_bytes=write(fd,ptr,bytes_left);
            //printf("Write %d bytes\n",written_bytes);

            if(written_bytes<=0)
            {       

                    if(errno==EINTR)

                        written_bytes=0;

                    else             

                        return(-1);

            }

            bytes_left-=written_bytes;
            ptr+=written_bytes;   

    }

    return(0);

}

int my_read(int fd, void *buffer, int length)
{

    int bytes_left;
    int bytes_read;
    char *ptr;
    
    ptr=buffer;
    bytes_left=length;

    while(bytes_left>0)
    {

        bytes_read=read(fd,ptr,bytes_read);

        if(bytes_read<0)
        {

            if(errno==EINTR)

                bytes_read=0;

            else

                return(-1);

        }

        else if(bytes_read==0)
            break;

        bytes_left-=bytes_read;
        ptr+=bytes_read;

    }

    return(length-bytes_left);

}
