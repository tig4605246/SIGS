/*

    Name: Xu Xi-Ping
    Date: March 20,2017
    Last Update: March 23,2017
    Program statement: 
        Upload MQTT data to server

*/

/*
initialization()
    argv[2] receive payload quantities
    look_for_row_index_127()

while(1)
    count 30s
    check file status

    if (file is broken)
        discard data 
        row_count = row_count + 1
        continue
    else
        load data
        upload data to mongo

        if(upload success)
            move file to backup folder
            row_count = row_count + 1
        else    
            re-upload at next while loop
*/

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
#include "../definition/SGSdefinitions.h"
#include "../ipcs/SGSipcs.h"
#include "../thirdparty/cJSON.h" // cJSON.c -lm


// catch post response for handler

struct string{
    char *ptr;
    size_t len;
};

//we use this function to initialize string

void init_string(struct string *s);

//curl callback handler

size_t response_handler(void *ptr, size_t size, size_t nmemb, struct string *s);

//examine the correctness of the file    

int UploadDataProcess(void );

//perform JSON operation and call curl http POST

int CreateJSONAndRunHTTPCommand(int IndexNum, char *dcode);


int main(int argc, char** argv)
{


    time_t timep, timep_t, timep_t2;
    char buf[64];
    int i = 0, j = 0;
    int ret = 0;
    int upload_interval = 30;
    FILE *fd;

    openlog("mongo", LOG_PID, LOG_USER);

    //record pid for SEG to control it


    memset(buf,'\0',sizeof(buf));

    sprintf(buf, "./pid/mongoagent.pid");
    if((fd=fopen(buf, "w")) == NULL)
    {

        printf( "[mongo:%d] failed in fopen(%s)! %s",__LINE__, buf, strerror(errno));
        closelog();
        return -1;

    }

    fprintf(fd, "%d\n", getpid());
    fclose(fd);
    printf("write done\n");
    sleep(5);

    //get first timestamp

    time(&timep_t);
    timep_t2 = timep_t;

    //main loop
    
    
    while(1) 
    {

        usleep(500000);
        time(&timep);
        if( ((timep%upload_interval) == 0) || ((timep-timep_t) >= upload_interval ) )
        {

            syslog(LOG_ERR, "[%s:%d] time interval >= 30", __FUNCTION__, __LINE__);
            for(j=0; j<3; j++)
            {

                ret = UploadDataProcess();
                syslog(LOG_ERR, "[%s:%d] upload_data() = %d", __FUNCTION__, __LINE__,ret);
                if( ret == 0)
                {

                    i = 0;
                    break;
                }
                else
                    i = i + 1;
            }
            time(&timep_t);
        }
        if(((timep%100) == 0) ||((timep-timep_t2) >= 100 ))
        {

            syslog(LOG_ERR, "[%s:%d] timep goes to 100 == 0 ", __FUNCTION__, __LINE__);
            timep_t2 = timep;
            if(i > 8)
            {
                
                printf("Cannot get the correct respond when uploading data to PTC\n");

            }

        }

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


int CreateJSONAndRunHTTPCommand(int IndexNum, char *dcode)
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
    CURL *curl;
	char *errstr;
	char Content_Length[32];
	struct curl_slist *chunk = NULL;
    struct string s;

    printf( "[%s:%d] CreateJSONAndRunHTTPCommand called", __FUNCTION__, __LINE__);

    

    init_string(&s);
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

    /*

        ---Stuff cJSON at here---

    */

    // We check the JSON data at here

    output = cJSON_PrintUnformatted(root);

    format = cJSON_Print(root);

    // Print the JSON data        

    printf("%s\n",output);
    
    // Adding curl http options

    sprintf(Content_Length,"Content-Length: %o",tag_num);
    printf("%s\n",Content_Length);

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();

    if(curl) 
    {

        printf("init\n");
        ret = curl_easy_setopt(curl, CURLOPT_URL, "http://140.118.70.136:9000/solar_rowdata");
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

