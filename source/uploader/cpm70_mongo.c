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

#include <curl/curl.h> //`pkg-config --cflags --libs ` or -lcurl
#include "../ipcs/SGSipcs.h"
#include "../controlling/SGScontrol.h"
#include "../thirdparty/cJSON.h" // cJSON.c -lm

deviceInfo *deviceInfoPtr = NULL;

dataInfo *dataInfoPtr = NULL;

deviceInfo *interface = NULL;

// catch post response for handler

struct string{
    char *ptr;
    size_t len;
};

//we use this function to initialize string

void init_string(struct string *s);

//curl callback handler

size_t response_handler(void *ptr, size_t size, size_t nmemb, struct string *s);

//perform JSON operation and call curl http POST

int CreateJSONAndRunHTTPCommand(deviceInfo *targetPtr);

//Intent : close program correctly
//Pre : signal number catched by sigaction
//Post : Nothing 

void stopAndLeave(int sigNum);

//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

static int initializeInfo();


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
        sleep(5);

    }

}

void init_string(struct string *s) 
{

  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) 
  {

    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);

  }
  s->ptr[0] = '\0';

}

size_t response_handler(void *ptr, size_t size, size_t nmemb, struct string *s)
{

  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) 
  {

    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);

  }
  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size*nmemb;

}

int CreateJSONAndRunHTTPCommand(deviceInfo *targetPtr)
{

    int i = 0;
    int ret=0;
    char *output = NULL;
    char *format = NULL;
    cJSON *root, *row; 
    cJSON *field;
    CURL *curl;
	char *errstr;
	char Content_Length[32];
	struct curl_slist *chunk = NULL;
    struct string s;
    dataInfo *dataTemp = targetPtr->dataInfoPtr;
    dataLog dLog ;

    printf( "[%s:%d] CreateJSONAndRunHTTPCommand called\n", __FUNCTION__, __LINE__);

    

    init_string(&s);
    
    // Initialize curl headers
    syslog(LOG_ERR,"[%s:%d]  ", __FUNCTION__, __LINE__);
    /*
    chunk = curl_slist_append(chunk, "Accept: text/plain");
	chunk = curl_slist_append(chunk, "Accept-Encoding: gzip, deflate");
    chunk = curl_slist_append(chunk, "application/json; charset=UTF-8");
	chunk = curl_slist_append(chunk, "Content_Length");
	chunk = curl_slist_append(chunk, "User-Agent: Kelier/0.1");
    */

    // Initialize cJSON

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "rows", row=cJSON_CreateArray() );
    syslog(LOG_ERR,"[%s:%d]  ", __FUNCTION__, __LINE__);
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

        printf("[%s:%d]  dataTemp-valueName %s \n", __FUNCTION__, __LINE__,dataTemp->valueName);
        dataTemp = dataTemp->next;
        

        //Put rest of the data which are from same sensor into the JSON Object
        //If we meet a new ID, then create a new Object

        while( dataTemp != NULL)
        {

            
            memset(dLog.value.s,'\0',sizeof(dLog.value.s));
            dLog.valueType = STRING_VALUE;

            //Get data from shared memory

            sgsReadSharedMemory(dataTemp,&dLog);

            //Put data into the JSON Object

            cJSON_AddStringToObject(field,dataTemp->valueName,dLog.value.s);
            //syslog(LOG_ERR,"[%s:%d] dataTemp->valueName %s", __FUNCTION__, __LINE__,dataTemp->valueName);
            
            if(strcmp(dataTemp->valueName,"ID"))
                dataTemp = dataTemp->next;
            else
                break;
            //syslog(LOG_ERR,"[%s:%d] dataTemp->valueName %s", __FUNCTION__, __LINE__,dataTemp->valueName);

        }

    }

    syslog(LOG_ERR,"[%s:%d] cJSON done ", __FUNCTION__, __LINE__);

    // We check the JSON data at here

    output = cJSON_PrintUnformatted(root);

    format = cJSON_Print(root);

    // Print the JSON data        

    printf("%s\n",output);
    // Adding curl http options

    sprintf(Content_Length,"Content-Length: %o",50);
    printf("%s\n",Content_Length);

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();

    if(curl) 
    {

        printf("init\n");
        if(interface != NULL)
        {   

            printf("using %s\n",interface->interface);
            ret = curl_easy_setopt(curl, CURLOPT_URL, interface->interface);

        }
           
        else
            ret = curl_easy_setopt(curl, CURLOPT_URL, "140.118.121.61:8000/test");
        printf("%d\n",ret);
        //errstr = curl_easy_strerror(ret);
        //printf("%s\n",errstr);

        /* Now specify we want to POST data */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);


        /* size of the POST data */
        //ret = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, sizeof(output));
        printf("setopt FIELDSSIZE\n");
        printf("%d\n",ret);
        //errstr = curl_easy_strerror(ret);
        //printf("%s\n",errstr);

        /* pass in a pointer to the data - libcurl will not copy */
        ret = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, output);
        printf("setopt FIELD\n");
        printf("%d\n",ret);
        //errstr = curl_easy_strerror(ret);
        //printf("%s\n",errstr);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_handler);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        
        ret = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        ret = curl_easy_perform(curl);
        printf("perform return %d\n",ret);
        //errstr = curl_easy_strerror(ret);
        //printf("%s\n",errstr);

    }
    else
    {

        printf("initialize curl failed\n");
        return -1;

    }

    if(ret != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(ret));

    if(s.ptr[11] == 't') ret = 0;
    else ret = -1;

    //curl_slist_free_all(chunk);

    // always cleanup 
    
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    // Clean JSON pointer

    cJSON_Delete(root); 
    
    free(format);
    free(output);
    free(s.ptr);
    printf(  "[%s:%d] Finished\n", __FUNCTION__, __LINE__);
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
    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    

    return 0;

}

