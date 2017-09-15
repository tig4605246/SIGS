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

#include "../ipcs/SGSipcs.h"
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

int AddToLogFile(char *filePath, char *log);

int CheckLogFileSize(char *filePath);

int CheckDuplicateByTime(char *ID, char *time);

//Post definitions for max length

#define SA      struct sockaddr
#define MAXLINE 16384
#define MAXSUB  16384

//Upload interval

int upload_interval = 5;

//I don't know what this is for

extern int h_errno;

//Server ip

char *hname = "140.118.70.136";

//Which port server is using

#define SERVERPORT 9110

//the RESTAPI 

//char *page = "/cpm70/rawdata/post/";
char *page = "/cpm70/gw/test";

//the gw id

char gwId[64] = {0};

char *logPath = "/var/log/cpm70_agentLog";

struct MeterList
{

    char meterId[64];
    char prevTime[64];
    int available;

};
typedef struct MeterList meterList;

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

        snprintf(gwId, sizeof(gwId) - 1, "[%s,%d]There's no gw id at /run/GW_ID", __FUNCTION__, __LINE__);
        AddToLogFile(logPath, gwId);

    }

    while(1)
    {

        //printf("GOGO\n");
        ret = getInfoToJSONAndUpload(buf);

        snprintf(buf, sizeof(buf) - 1, "[%s,%d]Upload function return %d\n", __FUNCTION__, __LINE__, ret);
        AddToLogFile(logPath, buf);
        CheckLogFileSize(logPath);

        sleep(upload_interval);

    }

    stopAndLeave(0);
    return 0;

}

int getInfoToJSONAndUpload(char *useless)
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
    char buf[BUFLEN] = {0};
    char buff[1024];

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

    //printf("buf is %s\n",buf);

    //if it's not ok, skip this time

    if(strstr(buf,"ok") == NULL)
    {

        //Should return -1 here
        snprintf(buff, sizeof(buff) - 1, "[%s,%d]AAEON agent return not ok:\n%s\n", __FUNCTION__, __LINE__,buf);
        AddToLogFile(logPath, buff);
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
            
            ret = 0;
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

                raw[i+1] = strchr(raw[i],';');

                if(raw[i+1] != NULL) 
                {
                    *raw[i+1] = 0;
                    raw[i+1]++;
                }
                
                if(i == 1)//Check Duplicate data
                {

                    
                    snprintf(buff, sizeof(buff) - 1, "[%s,%d]raw[%d] %s\n", __FUNCTION__, __LINE__, i-1, raw[i-1]);
                    AddToLogFile(logPath, buff);

                    // if ret == -1, it means the data is duplicated. We should skip this data

                    ret = CheckDuplicateByTime(buf, raw[i-1]);
                    if(ret == -1)
                    {

                        numberOfDevices--;
                        break;

                    } 
                    
                }

            } 

            if(ret == -1)
            {
                continue;
            }

            //Insert data here

            //Create an object inside the "rows"

            cJSON_AddItemToArray(row, field=cJSON_CreateObject());

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

    snprintf(buff, sizeof(buff) - 1, "[%s,%d]JSON Raw:\n%s\n", __FUNCTION__, __LINE__, output);
    AddToLogFile(logPath, buff);

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
    char buf[2048];
	ssize_t n;

    //Intialize host entity with server ip address

    if ((hptr = gethostbyname(hname)) == NULL) 
    {

        snprintf(buf, sizeof(buf) - 1, "[%s:%d] gethostbyname error for host: %s: %s", __FUNCTION__, __LINE__,hname ,hstrerror(h_errno));
        AddToLogFile(logPath, buf);

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

        snprintf(buf, sizeof(buf) - 1, "[%s:%d] Error call inet_ntop \n",__FUNCTION__,__LINE__);
        AddToLogFile(logPath, buf);

        return -1;

	}

    //Create socket

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd == -1)
    {

        snprintf(buf, sizeof(buf) - 1, "socket failed, errno %d, %s\n",errno,strerror(errno));
        AddToLogFile(logPath, buf);
        return -1;

    }
    else
    {

        snprintf(buf, sizeof(buf) - 1, "[%s,%d]socket() done\n",__FUNCTION__,__LINE__);
        AddToLogFile(logPath, buf);

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

        snprintf(buf, sizeof(buf) - 1, "connect() failed, errno %d, %s\n",errno,strerror(errno));
        AddToLogFile(logPath, buf);
        close(sockfd);
        return -1;

    }
    else
    {

        snprintf(buf, sizeof(buf) - 1, "[%s,%d]connect() done\n",__FUNCTION__,__LINE__);
        AddToLogFile(logPath, buf);

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

        snprintf(buf, sizeof(buf) - 1, "write() failed, errno %d, %s\n",errno,strerror(errno));
        AddToLogFile(logPath, buf);
        close(sockfd);
        return -1;

    }
    snprintf(buf, sizeof(buf) - 1, "[%s,%d]Write() done\n",__FUNCTION__,__LINE__);
    AddToLogFile(logPath, buf);

    //Get the result

    ret = my_read(sockfd, recvline, (sizeof(recvline) - 1));

    if(ret == -1)
    {

        snprintf(buf, sizeof(buf) - 1, "read() failed, errno %d, %s\n",errno,strerror(errno));
        AddToLogFile(logPath, buf);
        close(sockfd);
        return -1;

    }
    snprintf(buf, sizeof(buf) - 1, "[%s,%d]Read done\n",__FUNCTION__,__LINE__);
    AddToLogFile(logPath, buf);

    //Check if the last sending process is a success or not

    error = strcasestr(recvline,"true");
    if(error != NULL)
    {

        snprintf(buf, sizeof(buf) - 1, "[%s,%d] Result :\n%s \n",__FUNCTION__,__LINE__,recvline);
        AddToLogFile(logPath, buf);

    }
    else
    {

        snprintf(buf, sizeof(buf) - 1, "[%s,%d] Result :\n%s\n",__FUNCTION__,__LINE__, recvline);
        AddToLogFile(logPath, buf);
        close(sockfd);
        return -1;

    }
    

    //Always release the resources

    close(sockfd);

	return n;

}

void stopAndLeave(int sigNum)
{

    char buf[256];
    snprintf(buf, sizeof(buf) - 1, "cpm70_agent is quitting...\n");
    AddToLogFile(logPath, buf);
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

int AddToLogFile(char *filePath, char *log)
{

    FILE *fp = NULL;
    time_t t = time(NULL);
    DATETIME timeStruct  = *localtime(&t);

    fp = fopen(filePath,"a");

    if(fp == NULL)
    {

        printf("Failed to open the log file %s \n",filePath);
        return -1;

    }

    fprintf(fp,"[%04d-%02d-%02d %02d:%02d:%02d],%s\n"
        , timeStruct.tm_year + 1900, timeStruct.tm_mon + 1, timeStruct.tm_mday, timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec, log);

    fclose(fp);
    return 0;

}

int CheckLogFileSize(char *filePath)
{

    struct stat st;
    FILE *fp = NULL;
    int ret = -1;
    
    ret = stat(filePath, &st);

    if(ret == -1)
    {

        fp = fopen(filePath,"w");
        fprintf(fp,"No log file detected. Create one here\n");
        fclose(fp);
        return 0;

    }

    if(st.st_size > MAXLOGSIZE)
    {

        fp = fopen(filePath,"w");
        fclose(fp);

    }
    return 0;

}

int CheckDuplicateByTime(char *ID, char *time)
{

    static meterList mList[100];
    static int init = 0;
    int i = 0;
    char buf[256] = {0};

    if(init == 0)
    {

        memset(mList, 0, 100 * sizeof(meterList));
        init = 1;

    }

    while( i < 100)
    {

        if(mList[i].available == 0)
        {

            strncpy(mList[i].meterId, ID, sizeof(mList[i].meterId) - 1);
            strncpy(mList[i].prevTime, time, sizeof(mList[i].prevTime) - 1);
            mList[i].available = 1;
            return 0;

        }
        else
        {

            if(!strcmp(mList[i].meterId, ID))
            {

                if(!strcmp(mList[i].prevTime, time))
                {

                    snprintf(buf, sizeof(buf) -1, "[%s,%d]%s is duplicated (%s)\n", __FUNCTION__, __LINE__, ID, time);
                    AddToLogFile(logPath, buf);
                    return -1;

                }
                else
                {

                    strncpy(mList[i].prevTime, time, sizeof(mList[i].prevTime) - 1);
                    return 0;

                }

            }

        }
        i++;

    }

}