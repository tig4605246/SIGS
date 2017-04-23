 /*

    Name: Xu Xi-Ping
    Date: April 14,2017
    Last Update: April 14,2017
    Program statement: 
        Upload Orion data to server

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

dataInfo *interface = NULL;

char *DeviceName[] = {"616230912", "616230309" ,"616230884","zzz",NULL};

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

    openlog("Orion_mongo", LOG_PID, LOG_USER);

    //record pid for SEG to control it


    ret = sgsInitControl("Orion_mongo");
    if(ret < 0)
    {

        printf("Orion_mongo aborting\n");
        return -1;

    }

    //Initialize deviceInfo and dataInfo

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("Orion_mongo aborting\n");
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

    while(strcmp(target->deviceName,"Orion_mongo") && target != NULL)
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

   

    interface = target->dataInfoPtr;

    while(strcmp(interface->deviceName,"Orion_mongo") && interface != NULL)
    {

        interface = interface->next;

    }
    if(interface == NULL)
    {

        printf("Warning : Orion_mongo is not defined in device.conf\n");

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
        sleep(15);

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
    struct timeb tp;
    DATETIME n_time;

    int irr_value = 0;
    int temp_value = 0;
    int ret=0;
    char *output = NULL;
    char *format = NULL;
    cJSON *root; 
    cJSON *fmt;
    cJSON *array;

    CURL *curl;
	char *errstr;
	char Content_Length[32];
	struct curl_slist *chunk = NULL;
    struct string s;

    //dataInfo *head = dataInfoPtr;
    dataInfo *head = interface;
    dataLog dest;
    int j;
    printf( "[%s:%d] CreateJSONAndRunHTTPCommand called", __FUNCTION__, __LINE__);
    printf("\n");
    

    
    ftime(&tp);
    n_time = tp.time*1000 + tp.millitm;
    // Initialize curl headers
    syslog(LOG_INFO, "[%s:%d] Initialize curl headers ", __FUNCTION__, __LINE__);
    chunk = curl_slist_append(chunk, "Accept: text/plain");
	chunk = curl_slist_append(chunk, "Accept-Encoding: gzip, deflate");
    chunk = curl_slist_append(chunk, "application/json; charset=UTF-8");
	chunk = curl_slist_append(chunk, "Content_Length");
	chunk = curl_slist_append(chunk, "User-Agent: Kelier/0.1");

    // Initialize cJSON

    ///*   
        //---Stuff cJSON at here---
    
    j=0;
    curl_global_init(CURL_GLOBAL_ALL);

 
    curl = curl_easy_init();
   
    while(DeviceName[j] != NULL){
        init_string(&s);
    //while(head != NULL){      
    //while(DeviceName[j] != NULL){
        head = interface;
        root = cJSON_CreateObject();
        if(head == NULL)
        {
             printf("head is NULL \n");
        }

        while(head != NULL){
            ret = sgsReadSharedMemory(head,&dest);

            if(ret != 0){
                printf("Read %s shm value failed\n",head->valueName);
                printf("\n");        
            }           
            if(!strcmp(head->sensorName,DeviceName[j])){    
                printf("*------------Get it!--------*\n");
                printf("*--head->valueName %s *\n",head->valueName);
                if(!strcmp(head->valueName, "DeviceID")){
                    printf("*match DeviceID*\n");
                    printf("*dest.value.s %s*\n",dest.value.s);
                    cJSON_AddStringToObject(root, "DeviceId", dest.value.s);
                    cJSON_AddStringToObject(root, "UnitID", dest.value.s);                
                }              

                else if(!strcmp(head->valueName, "MODEL")){
                    cJSON_AddStringToObject(root, "Model", dest.value.s);
                }
                else if(!strcmp(head->valueName, "IMEI")){
                    cJSON_AddStringToObject(root, "IMEI", dest.value.s);
                }
                else if(!strcmp(head->valueName, "Location")){
                    cJSON_AddStringToObject(root, "Location", dest.value.s);
                }
                else if(!strcmp(head->valueName, "Heading")){
                    cJSON_AddNumberToObject(root, "Heading", dest.value.f);
                }
                else if(!strcmp(head->valueName, "Speed")){
                    cJSON_AddNumberToObject(root, "Speed", dest.value.f);
                }
                else if(!strcmp(head->valueName, "Odometer")){
                    cJSON_AddNumberToObject(root, "Odometer", dest.value.f);
                }           
                else if(!strcmp(head->valueName, "EngineLoad")){
                    cJSON_AddNumberToObject(root, "EngineLoad", dest.value.f);
                }
                else if(!strcmp(head->valueName, "EngineCoolantTemp")){
                    cJSON_AddNumberToObject(root, "EngineCoolantTemp", dest.value.f);
                }
                else if(!strcmp(head->valueName, "FuelLevel")){
                    cJSON_AddNumberToObject(root, "FuelLevel", dest.value.f);
                }
                else if(!strcmp(head->valueName, "IntakeAirTemp")){
                    cJSON_AddNumberToObject(root, "IntakeAirTemp", dest.value.f);
                }
                else if(!strcmp(head->valueName, "EngineRPM")){
                    cJSON_AddNumberToObject(root, "EngineRPM", dest.value.f);
                }
                else if(!strcmp(head->valueName, "FuelUsed")){
                    cJSON_AddNumberToObject(root, "FuelUsed", dest.value.f);
                }                
                else if(!strcmp(head->valueName, "MassAirFlow")){
                    cJSON_AddNumberToObject(root, "MassAirFlow", dest.value.f);
                }
                else if(!strcmp(head->valueName, "IntakeManifoldAbsolutePressure")){
                    cJSON_AddNumberToObject(root, "IntakeManifoldAbsolutePressure", dest.value.f);
                }
                else if(!strcmp(head->valueName, "MalfunctionIndicatorLamp")){
                    cJSON_AddNumberToObject(root, "MalfunctionIndicatorLamp", dest.value.f);
                }
                else if(!strcmp(head->valueName, "ThrottlePosition")){
                    cJSON_AddNumberToObject(root, "ThrottlePosition", dest.value.f);
                }
                else if(!strcmp(head->valueName, "VIN")){
                    cJSON_AddStringToObject(root, "VIN", dest.value.s);
                }
                else if(!strcmp(head->valueName, "MainVoltage")){
                    cJSON_AddNumberToObject(root, "MainVoltage", dest.value.f);
                } 
                else if(!strcmp(head->valueName, "HDOP")){
                    cJSON_AddNumberToObject(root, "HDOP", dest.value.f);
                }             
                else if(!strcmp(head->valueName, "SN")){
                    cJSON_AddStringToObject(root, "SN", dest.value.s);
                }
                else if(!strcmp(head->valueName, "PendingCodeStatus")){
                    cJSON_AddStringToObject(root, "PendingCodeStatus", dest.value.s);
                }
                else if(!strcmp(head->valueName, "ReportID")){
                    cJSON_AddStringToObject(root, "ReportID", dest.value.s);
                }
                else if(!strcmp(head->valueName, "ReportTime")){
                    cJSON_AddStringToObject(root, "ReportTime", dest.value.s);
                }                              
                else
                    printf("Error value name %s!\n",head->valueName);
                ;     

                  
                

                if(!strcmp(dest.value.s, "Value is not ready yet")){
                printf("dest.value.s : \n%s+\n",dest.value.s);
                printf("HTTP Value is not ready yet\n");
                cJSON_Delete(root); 
                root = cJSON_CreateObject();

                printf("Value is not ready yet %s\n", DeviceName[j]);
                output = cJSON_PrintUnformatted(root);

                format = cJSON_Print(root);
         
                break;
                //return -1;
                }
                // Value of Sensor Type Ready and Do HTTP                
            }            
           
            head = head->next;
        }

        ///*------*/------
        //----------------------------------
        // We check the JSON data at here
        cJSON_AddStringToObject(root, "PlateNumber", "0");
        cJSON_AddStringToObject(root, "ProductModel", "0");  
        cJSON_AddStringToObject(root, "EventCode", "0");

        printf("cJSON_PrintUnformatted %s\n", DeviceName[j]);
        output = cJSON_PrintUnformatted(root);

        format = cJSON_Print(root);
        
        // Print the JSON data 
        printf("-****-output-*****- \n%s\n\n",output);   

        if(root == NULL){
           printf("--------------root NULL--------------\n"); 
        }
        printf("--------------output > len--------------\n");         
        if(strlen(output) > 20){
              
            //----------------------------------
            // Adding curl http options

            sprintf(Content_Length,"Content-Length: %o",50);
            printf("%s\n",Content_Length);
 
            
           
            if(curl) 
            {
            printf("init\n");
            //ret = curl_easy_setopt(curl, CURLOPT_URL, "http://140.118.121.61:8000/car_fet");
            ret = curl_easy_setopt(curl, CURLOPT_URL, "http://140.118.121.61:8000/car_fet");
            //ret = curl_easy_setopt(curl, CURLOPT_URL, "http://140.118.70.136:9001/car_fet");
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

           
            
            
            // Clean JSON pointer

            cJSON_Delete(root); 
            

            free(format);
            free(output);
            free(s.ptr);
            
            printf(  "[%s:%d] Finished\n", __FUNCTION__, __LINE__);
            ///*------*/------
        }      
            
        j++;
    }    
    
 
    curl_global_cleanup();
     curl_easy_cleanup(curl);
    printf("return ret %d\n*-------------------*\n", ret);
    return ret;
}

void stopAndLeave(int sigNum)
{

    sgsDeleteAll(deviceInfoPtr, -1);
    printf("Orion_mongo is quitting...\n");
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

    //ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 0);
    ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 0);
    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    

    return 0;

}

