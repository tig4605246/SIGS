/*

    Name: Xu Xi-Ping
    Date: March 28,2017
    Last Update: March 31,2017
    Program statement: 
        Dynamically forms the JSON format every time it fetches data from the cpm70-agent-tx


*/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/timeb.h>

#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>


#include <sys/syslog.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <time.h>
#include <error.h>
#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>

//We declare our own libraries at below

#include "../events/SGSEvent.h"
#include "../controlling/SGScontrol.h"
#include "../thirdparty/cJSON.h" // cJSON.c -lm


#define CMDLEN 128
#define BUFLEN 2048

//Intent : execute cpm70_agent-tx, and format the data
//Pre : buf for result 
//Post : On success, return 0. On error, return -1 and shows the error message

int getInfoToJSONAndUpload(char *buf);

//Purpose : Replace curl doing Post
//Pre     : content we want to upload
//Post    : return the packets size we upload to the server (target)

ssize_t process_http( char *content);

//Intent : Write data to socket's file descriptor properly
//Pre    : file descriptor, data buffer, data length
//Post   : On success, return 0. On error return -1.

int my_write(int fd, void *buffer, int length);

//Intent : Read data from socket's file descriptor properly
//Pre    : file descriptor, data buffer, data length
//Post   : On success, return 0. On error return -1.

int my_read(int fd, void *buffer, int length);

//Intent : close program correctly
//Pre : signal number catched by sigaction
//Post : Nothing 

void stopAndLeave(int sigNum);

//Post definitions for max length

#define SA      struct sockaddr
#define MAXLINE 16384
#define MAXSUB  16384

//Upload interval

int upload_interval = 30;

//I don't know what this is for

extern int h_errno;

//Server ip

char *hname = "140.118.70.136";

//Which port server is using

#define SERVERPORT 9110

//the RESTAPI 

char *page = "/cpm70/rawdata/post/";

//the gw id

char gwId[64] = {0};


int main(int argc, char *argv[])
{

    int ret = 0;
    struct sigaction act, oldact;
    char buf[BUFLEN];
    FILE *fp = NULL;
    

    //Recording program's pid

    ret = sgsInitControl("cpm70_agent");
    if(ret < 0)
    {

        printf("cpm70_agent aborting\n");
        return -1;

    }


    //Catch aborting signal

    act.sa_handler = (__sighandler_t)stopAndLeave;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

    fp = fopen("/run/GW_ID","r");

    if(fp != NULL)
    {
        fscanf(fp,"%s",gwId);
        fclose(fp);
    }
    else
    {
        snprintf(gwId, sizeof(gwId) - 1, "There's no gw id at /run/GW_ID");
    }

    while(1)
    {

        ret = getInfoToJSONAndUpload(buf);

        //printf("function return %d\n",ret);

        sleep(upload_interval);

    }

    stopAndLeave(0);
    return 0;

}

int getInfoToJSONAndUpload(char *buf)
{

    FILE *fpRead = NULL;
    char cmd[CMDLEN];
    char* raw[34];
    char *tempPt = NULL;
    char *output = NULL;
    char *format = NULL;
    int i = 0;
    int ret = 0;
    int numberOfDevices = 0;

    cJSON *root, *row; 
    cJSON *field;

    //Ready the command at here

    memset(cmd,0,sizeof(sizeof(cmd)));

    snprintf(cmd,CMDLEN,"/home/aaeon/API/cpm70-agent-tx --get-dev-status");
    //snprintf(cmd,CMDLEN,"./cpm70-agent --get-dev-status");

    //execute command

    fpRead = popen(cmd,"r");

    //printf("%s\n",cmd);

    memset(buf,'\0',sizeof(buf));

    //Read the first line of the result to buf

    fgets(buf, BUFLEN , fpRead);

    //if it's not ok, skip this time

    if(strstr(buf,"ok") == NULL)
    {
        //Should return -1 here

        return -1;
    }
    tempPt = strstr(buf,";");
    *tempPt = 0;
    tempPt++;
    numberOfDevices = atoi(tempPt);
    //printf("device number is %d\n",numberOfDevices);
    
    // Initialize cJSON

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "rows", row=cJSON_CreateArray() );


    //Read Detail to buf at this loop

    while((numberOfDevices) > 0)
    {

        //Create an object inside the "rows"

        cJSON_AddItemToArray(row, field=cJSON_CreateObject());

        //Initialize char pointer array

        for(i = 0 ; i < 33 ; i++)
            raw[i] = NULL;

        //flush input buffer

        memset(buf,'\0',sizeof(buf));

        //Get one sensor's info at a time

        fgets(buf, BUFLEN , fpRead);

        //Set the last char to be zero, prevents overflow

        buf[strlen(buf) - 1] = '\0';

        //printf("buf len %lu\n%s",strlen(buf),buf);//debugging

        if(strlen(buf) != 0)
        {
            
            for(i = 0 ; i < 30 ; i++)
            {

                if(i == 0)
                {

                    raw[i] = strchr(buf,';');
                    if(raw[i] != NULL) 
                    {
                        *raw[i] = 0;
                        raw[i]++;
                    }

                }
                //printf("i = %d\n",i);//debugging

                raw[i+1] = strchr(raw[i],';');

                if(raw[i+1] != NULL) 
                {
                    *raw[i+1] = 0;
                    raw[i+1]++;
                }

            } 

            //Insert data here

            i = 0;

            if(1)
            {

                cJSON_AddStringToObject(field,"GWID",gwId);
                cJSON_AddStringToObject(field,"devID",buf);

                cJSON_AddStringToObject(field,"lastReportTime",raw[i++]);
                cJSON_AddStringToObject(field,"wire",raw[i++]);
                cJSON_AddStringToObject(field,"freq",raw[i++]);
                cJSON_AddStringToObject(field,"ua",raw[i++]);
                cJSON_AddStringToObject(field,"ub",raw[i++]);

                cJSON_AddStringToObject(field,"uc",raw[i++]);
                cJSON_AddStringToObject(field,"u_avg",raw[i++]);
                cJSON_AddStringToObject(field,"uab",raw[i++]);
                cJSON_AddStringToObject(field,"ubc",raw[i++]);
                cJSON_AddStringToObject(field,"uca",raw[i++]);

                cJSON_AddStringToObject(field,"uln_avg",raw[i++]);
                cJSON_AddStringToObject(field,"ia",raw[i++]);
                cJSON_AddStringToObject(field,"ib",raw[i++]);
                cJSON_AddStringToObject(field,"ic",raw[i++]);
                cJSON_AddStringToObject(field,"i_avg",raw[i++]);

                cJSON_AddStringToObject(field,"pa",raw[i++]);
                cJSON_AddStringToObject(field,"pb",raw[i++]);
                cJSON_AddStringToObject(field,"pc",raw[i++]);
                cJSON_AddStringToObject(field,"p_sum",raw[i++]);
                cJSON_AddStringToObject(field,"sa",raw[i++]);

                cJSON_AddStringToObject(field,"sb",raw[i++]);
                cJSON_AddStringToObject(field,"sc",raw[i++]);
                cJSON_AddStringToObject(field,"s_sum",raw[i++]);
                cJSON_AddStringToObject(field,"pfa",raw[i++]);
                cJSON_AddStringToObject(field,"pfb",raw[i++]);

                cJSON_AddStringToObject(field,"pfc",raw[i++]);
                cJSON_AddStringToObject(field,"pf_avg",raw[i++]);
                cJSON_AddStringToObject(field,"ae_tot",raw[i++]);
                cJSON_AddStringToObject(field,"uavg_thd",raw[i++]);
                cJSON_AddStringToObject(field,"iavg_thd",raw[i++]);
                

            }

            
            
        }
        else
        {

            //printf("Read done\n");

            break;

        }

        numberOfDevices -= 1;

    }

    // We check the JSON data at here

    output = cJSON_PrintUnformatted(root);

    //format = cJSON_Print(root);

    ret = process_http(output);

    //Free JSON

    cJSON_Delete(root); 

    free(output);

    //Always do clean up

    fclose(fpRead);

    return ret;

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
    char address[64];
	ssize_t n;

    //Intialize host entity with server ip address

    if ((hptr = gethostbyname(hname)) == NULL) 
    {

		syslog(LOG_ERR, "[%s:%d] gethostbyname error for host: %s: %s", __FUNCTION__, __LINE__,hname ,hstrerror(h_errno));

		return -1;

	}

	//printf("[%s:%d] hostname: %s\n",__FUNCTION__,__LINE__, hptr->h_name);

    //Set up address type (FAMILY)

    memset(address,'\0',sizeof(address));

	if (hptr->h_addrtype == AF_INET && (pptr = hptr->h_addr_list) != NULL) 
    {

        inet_ntop( hptr->h_addrtype , *pptr , str , sizeof(str) );
		//printf( "[%s:%d] address: %s\n",__FUNCTION__,__LINE__,inet_ntop( hptr->h_addrtype , *pptr , str , sizeof(str) ));
        

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

        //printf("[%s,%d]socket() done\n",__FUNCTION__,__LINE__);

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
		 "Content-Length: %lu\r\n\r\n"
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

        //syslog(LOG_ERR, "\033[1;32m""[%s,%d] Post Successfully\n""\033[1;37m",__FUNCTION__,__LINE__);
        //printf( "\033[1;32m""[%s,%d] Post Result : %s \n""\033[1;37m",__FUNCTION__,__LINE__,recvline);

    }
    else
    {

        //syslog(LOG_ERR, "\033[1;31m""[%s,%d] %s\n""\033[1;37m",__FUNCTION__,__LINE__, recvline);
        //printf("\033[1;31m""[%s,%d] %s\n""\033[1;37m",__FUNCTION__,__LINE__, recvline);
        close(sockfd);
        return -1;

    }
    

    //Always release the resources

    close(sockfd);

	return n;

}

void stopAndLeave(int sigNum)
{

    printf("cpm70_agent is quitting...\n");
    exit(0);
    return;

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