// written by Kevin
//Last edited at 2017/02/24
/*

  upload_interval in main determines upload interval

  id in   CreateJSONAndRunHTTPCommand determines the source of data

  1 is 302
  2 is 202

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
#include "sharem.h"
#include "lib_ptc.h"
#include "cJSON.h" // cJSON.c -lm

//how many commands

int tag_num=0;

// catch post response for handler

struct string{
    char *ptr;
    size_t len;
};

//we use this function to initialize string

void init_string(struct string *s);

//curl callback hendler

size_t response_handler(void *ptr, size_t size, size_t nmemb, struct string *s);

//examine the correctness of the file    

int UploadDataProcess(void );

//perform JSON operation and call curl http POST

int CreateJSONAndRunHTTPCommand(int IndexNum, char *dcode);


int main(int argc, char** argv){
    time_t timep, timep_t, timep_t2;
    char buf[64];
    int i = 0, j = 0;
    int ret = 0;
    int upload_interval = 30;
    FILE *fd;

    openlog("mongo", LOG_PID, LOG_USER);

    //record pid for SEG to control it


    memset(buf,'\0',sizeof(buf));

    sprintf(buf, "%s", GW_data_127_PID);
    if((fd=fopen(buf, "w")) == NULL){
        syslog(LOG_ERR, "[mongo:%d] failed in fopen(%s)! %s",
                        __LINE__, buf, strerror(errno));
        //gwlog_save("ptc", "mongo program failed in calling fopen(/run/tw_daemon_127.pid)");
        closelog();
        return -1;
    }
    fprintf(fd, "%d\n", getpid());
    fclose(fd);
    printf("write done\n");
    sleep(30);

    //get tag numbers

    tag_num = atoi(argv[1]);

    //look up log index

    look_for_rowindex_127();

    //get first timestamp

    time(&timep_t);
    timep_t2 = timep_t;

    //main loop

    while(1) {
        usleep(500000);
        time(&timep);
        if(((timep%upload_interval) == 0) ||
            ((timep-timep_t) >= upload_interval )){
            syslog(LOG_ERR, "[%s:%d] time interval >= 30", __FUNCTION__, __LINE__);
            for(j=0; j<3; j++){
                ret = UploadDataProcess();
                syslog(LOG_ERR, "[%s:%d] upload_data() = %d", __FUNCTION__, __LINE__,ret);
                if( ret == 0){
                    i = 0;
                    break;
                }else
                    i = i + 1;
            }
            time(&timep_t);
        }
        if(((timep%100) == 0) ||((timep-timep_t2) >= 100 )){
            syslog(LOG_ERR, "[%s:%d] timep goes to 100 == 0 ", __FUNCTION__, __LINE__);
            timep_t2 = timep;
            if(i > 8){
                
                printf("Cannot get the correct respond when uploading data to PTC\n");
            }
            
                
        }
    }

}

void init_string(struct string *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t response_handler(void *ptr, size_t size, size_t nmemb, struct string *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
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
    int i, fd;
    char buf[128], filename[64];
    struct timeb tp;
    DATETIME cap_time;
    DATETIME n_time;

    int irr_value = 0;
    int temp_value = 0;
    int ret=0;
    char *output = NULL;
    char *format = NULL;
    cJSON *root; 
	cJSON *row;
	cJSON *field, *alarmfield;
    CURL *curl;
	char *errstr;
	char Content_Length[32];
	struct curl_slist *chunk = NULL;
    struct string s;

    printf( "[%s:%d] CreateJSONAndRunHTTPCommand called", __FUNCTION__, __LINE__);

    sprintf(filename, "%s/P%06d/%06d.tw", LOG_PATH_127, (IndexNum/5000)*5000, IndexNum);
    if((fd=open(filename, O_RDONLY)) < 0){
        syslog(LOG_ERR, "[%s:%d] fail in opening %s log file",
                         __FUNCTION__, __LINE__, filename);
        sprintf(buf, "%s/invalid/U-%06d-open.inv", LOG_PATH_127, IndexNum);
        if(rename(filename, buf) != 0)
            syslog(LOG_ERR, "[%s:%d] fail in removeing %s log file",
                            __FUNCTION__, __LINE__, filename);
        return -1;
    }
    
    i = read(fd, (void *)&iven, sizeof(iven));
    close(fd);
    if(i != sizeof(iven)){
        syslog(LOG_ERR, "[%s:%d] fail in reading %s log file",
                         __FUNCTION__, __LINE__, filename);
        sprintf(buf, "%s/invalid/U-%06d-read.inv", LOG_PATH_127, IndexNum);
        if(rename(filename, buf) != 0)
            syslog(LOG_ERR, "[%s:%d] fail in removeing %s log file",
                            __FUNCTION__, __LINE__, filename);
        return -1;
    }
    init_string(&s);
    ftime(&tp);
    n_time = tp.time*1000 + tp.millitm;
    cap_time = iven.store[0].read_time.time*1000 + iven.store[0].read_time.millitm;

    //Initialize curl headers
    syslog(LOG_INFO, "[%s:%d] Initialize curl headers ", __FUNCTION__, __LINE__);
    chunk = curl_slist_append(chunk, "Accept: text/plain");
	chunk = curl_slist_append(chunk, "Accept-Encoding: gzip, deflate");
    chunk = curl_slist_append(chunk, "application/json; charset=UTF-8");
	chunk = curl_slist_append(chunk, "Content_Length");
	chunk = curl_slist_append(chunk, "User-Agent: Kelier/0.1");

    // initialize JSON
    syslog(LOG_INFO, "[%s:%d] initialize JSON ", __FUNCTION__, __LINE__);
    root = cJSON_CreateObject();

    //we insert id at here

    cJSON_AddItemToObject(root, "id",cJSON_CreateString("2") );

    cJSON_AddItemToObject(root, "rows", row=cJSON_CreateArray() );

    irr_value = iven.store[tag_num-1].response[0];
    temp_value = iven.store[tag_num-2].response[0];
    syslog(LOG_INFO, "[%s:%d] irr = %d temp = %d ", __FUNCTION__, __LINE__,irr_value,temp_value);
    for(i=0;i<(tag_num-2);i+=30){
        if(strcmp(iven.store[0].daemon, "deltarpi")) break;
        cJSON_AddItemToArray(row, field=cJSON_CreateObject());
        cJSON_AddNumberToObject(field, "InverterID", iven.store[i].id);
        cJSON_AddNumberToObject(field, "upload_timestamp",  cap_time );
        cJSON_AddNumberToObject(field, "Inverter_Status",  iven.store[i].dev_status );

        cJSON_AddNumberToObject(field, "L1_ACA", iven.store[i].response[0]);
        cJSON_AddNumberToObject(field, "L1_ACV", iven.store[i+1].response[0]);
        cJSON_AddNumberToObject(field, "L1_ACP", iven.store[i+2].response[0]);
        cJSON_AddNumberToObject(field, "L1_AC_Freq", iven.store[i+3].response[0]);

        cJSON_AddNumberToObject(field, "L2_ACA", iven.store[i+4].response[0]);
        cJSON_AddNumberToObject(field, "L2_ACV", iven.store[i+5].response[0]);
        cJSON_AddNumberToObject(field, "L2_ACP", iven.store[i+6].response[0]);
        cJSON_AddNumberToObject(field, "L2_AC_Freq", iven.store[i+7].response[0]);
        
        cJSON_AddNumberToObject(field, "L3_ACA", iven.store[i+8].response[0]);
        cJSON_AddNumberToObject(field, "L3_ACV", iven.store[i+9].response[0]);
        cJSON_AddNumberToObject(field, "L3_ACP", iven.store[i+10].response[0]);
        cJSON_AddNumberToObject(field, "L3_AC_Freq", iven.store[i+11].response[0]);

        cJSON_AddNumberToObject(field, "DC1_DCA", iven.store[i+12].response[0]);
        cJSON_AddNumberToObject(field, "DC1_DCV", iven.store[i+13].response[0]);
        cJSON_AddNumberToObject(field, "DC1_DCP", iven.store[i+14].response[0]);

        cJSON_AddNumberToObject(field, "DC2_DCA", iven.store[i+15].response[0]);
        cJSON_AddNumberToObject(field, "DC2_DCV", iven.store[i+16].response[0]);
        cJSON_AddNumberToObject(field, "DC2_DCP", iven.store[i+17].response[0]);

        cJSON_AddNumberToObject(field, "Daily_kwh", (iven.store[i+18].response[0] +  iven.store[i+18].response[1]*65536) );
        cJSON_AddNumberToObject(field, "Life_kwh",  (iven.store[i+19].response[0] +  iven.store[i+19].response[1]*65536) );
        cJSON_AddNumberToObject(field, "inverter_temp", ( iven.store[i+20].response[0] ));
        
        //Create alarm JSON
        cJSON_AddItemToObject(field, "Alarm", alarmfield=cJSON_CreateObject() );
        
        cJSON_AddNumberToObject(alarmfield, "alarm_01", ( iven.store[i+21].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_02", ( iven.store[i+22].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_03", ( iven.store[i+23].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_04", ( iven.store[i+24].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_05", ( iven.store[i+25].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_06", ( iven.store[i+26].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_07", ( iven.store[i+27].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_08", ( iven.store[i+28].response[0] ));
        cJSON_AddNumberToObject(alarmfield, "alarm_09", ( iven.store[i+29].response[0] ));

        cJSON_AddNumberToObject(field, "Irr", irr_value);
        cJSON_AddNumberToObject(field, "PVTemp", temp_value);

    }
    syslog(LOG_INFO, "[%s:%d] for loop over ", __FUNCTION__, __LINE__);
    output = cJSON_PrintUnformatted(root);
    format = cJSON_Print(root);
    printf("%s\n",output);//printf out the JSON
    
    sprintf(Content_Length,"Content-Length: %o",tag_num);
    printf("%s\n",Content_Length);

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();

    if(curl) {
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
    else{
        printf("initialize curl failed\n");
        return -1;
    }

    if(ret != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(ret));
    if(s.ptr[11] == 't') ret = 0;
    else ret = -1;
    //curl_slist_free_all(chunk);
        /* always cleanup */
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    cJSON_Delete(root); // Clean JSON pointer
    free(format);
    free(output);
    free(s.ptr);
    printf(  "[%s:%d] Finished\n", __FUNCTION__, __LINE__);
    return ret;
}

int UploadDataProcess(void)
{
    char buf[1024],dcode[32],ddcode[5][32],n_time[32];
    int i, j, tail, fd, ret,m;
    char filename[64];
    char entry[10];
    //struct rowtable capdata;
    struct timeb tp;
    struct stat sb;
    DATETIME cap_time;
    DATETIME now_time;
    DATETIME tw_time;


    printf(  "[%s:%d] UploadDataProcess called", __FUNCTION__, __LINE__);

    tail = ptc_row_upload_127 + 1;
    if(tail >= MAX_ROWFILE) tail -= MAX_ROWFILE; 
    printf( "[%s:%d] Ready ptc_row_upload_127 is %d tail is %d\n",__FUNCTION__, __LINE__,ptc_row_upload_127,tail);
    do{
        sprintf(filename, "%s/P%06d/%06d.tw", LOG_PATH_127, (tail/5000)*5000, tail);
        if(stat(filename, &sb) != 0){
            printf( "[%s:%d] tail break\n",__FUNCTION__, __LINE__);
            break;
        } 
        sprintf(filename, "%s/P%06d/%06d.tw", LOG_PATH_127, (ptc_row_upload_127/5000)*5000, ptc_row_upload_127);
        if(stat(filename, &sb) != 0){
            printf( "[%s:%d] ptc_row_upload_127 break\n",__FUNCTION__, __LINE__);
          break;  
        } 
        ftime(&tp);
        tw_time = (tp.time)*1000 + tp.millitm;
        if((fd=open(filename, O_RDONLY)) < 0){
            syslog(LOG_ERR, "[%s:%d] fail in opening %s log file",
                    __FUNCTION__, __LINE__, filename);
            sprintf(buf, "%s/invalid/U-%06d-open.inv",
                    LOG_PATH_127, ptc_row_upload_127);
            if((ret=rename(filename, buf)) != 0)
                syslog(LOG_ERR, "[%s:%d] fail in removeing %s log file",
                                __FUNCTION__, __LINE__, filename);
            ptc_row_upload_127 += 1;
            if(ptc_row_upload_127 >= MAX_ROWFILE) ptc_row_upload_127 = 0;
            return -1;
        }
        
        ret = read(fd, (void *)&iven, sizeof(iven));

        close(fd);

        if(ret != sizeof(iven)){
            syslog(LOG_ERR, "[%s:%d] fail in reading %s log file",
                       __FUNCTION__, __LINE__, filename);
            printf("[%s:%d] fail in reading %s log file\n",
                       __FUNCTION__, __LINE__, filename);
            sprintf(buf, "%s/invalid/U-%06d-read.inv", LOG_PATH_127, ptc_row_upload_127);
            if((ret=rename(filename, buf)) != 0)
                syslog(LOG_ERR, "[%s:%d] fail in removeing %s log file",
                                __FUNCTION__, __LINE__, filename);
            ptc_row_upload_127 += 1;
            if(ptc_row_upload_127 >= MAX_ROWFILE) ptc_row_upload_127 = 0;
            return -1;
        }
        ftime(&tp);
        now_time = tp.time*1000 + tp.millitm;
        cap_time = iven.store[0].read_time.time*1000 + iven.store[0].read_time.millitm;
        

        
        //Here we add row content

        j=ptc_row_upload_127; 

        ret = CreateJSONAndRunHTTPCommand(j, ddcode[j-ptc_row_upload_127 ]);
            

        printf("[%s:%d] upload function return %d\n",__FUNCTION__, __LINE__,ret);

        //ret = 0 means upload successfully, ret = -1 otherwise
        
        if(ret == 0){
            
            if(stat("/tw_daemon_log/tw_127/log/mail/ptc/ptc_alarm", &sb) == 0){
                gwlog_save("ptc", "PTC connection and upload status back to normal");
                ret = remove("/tw_daemon_log/tw_127/log/mail/ptc/ptc_alarm");
                if(ret != 0)
                    syslog(LOG_ERR, "[%s:%d] fail in removing '/tw_daemon_log/tw_127/log/mail/ptc/ptc_alarm' file(%s)", __FUNCTION__, __LINE__, strerror(errno));
            }
            if(tail > ptc_row_upload_127 ){
                for(j=ptc_row_upload_127 ; j<tail; j++){
                    strncpy(filename, ddcode[j-ptc_row_upload_127 ], 8);
                    filename[8] = 0;
                    syslog(LOG_INFO, "[%s:%d] filename %s", __FUNCTION__, __LINE__, filename);
                    sprintf(buf, "%s/backup/%s", LOG_PATH_127 , filename);
                    syslog(LOG_INFO, "[%s:%d] buf %s", __FUNCTION__, __LINE__, buf);
                    if(stat(buf, &sb) != 0) mkdir(buf, 0755);
                    sprintf(buf, "%s/backup/%s/-U%d.tw", LOG_PATH_127 , filename, j);
                    syslog(LOG_INFO, "[%s:%d] buf %s", __FUNCTION__, __LINE__, buf);
                    sprintf(filename, "%s/P%06d/%06d.tw", LOG_PATH_127, (j/5000)*5000, j);
                    syslog(LOG_INFO, "[%s:%d] filename %s", __FUNCTION__, __LINE__, filename);
                    if((ret=rename(filename, buf)) != 0)
                        syslog(LOG_ERR,"[%s:%d] rename failure: %s",
                                    __FUNCTION__, __LINE__,  strerror(ret));
                }
            }else{
                for(j=ptc_row_upload_127 ; j<MAX_ROWFILE; j++){
                    strncpy(filename, ddcode[j-ptc_row_upload], 8);
                    filename[8] = 0;
                    syslog(LOG_INFO, "[%s:%d] filename %s", __FUNCTION__, __LINE__, filename);
                    sprintf(buf, "%s/backup/%s", LOG_PATH_127 , filename);
                    syslog(LOG_INFO, "[%s:%d] buf %s", __FUNCTION__, __LINE__, buf);
                    if(stat(buf, &sb) != 0) mkdir(buf, 0755);
                    sprintf(buf, "%s/backup/%s/-U%d.tw", LOG_PATH_127 , filename,j);
                    syslog(LOG_INFO, "[%s:%d] buf %s", __FUNCTION__, __LINE__, buf);
                    sprintf(filename, "%s/P%06d/%06d.tw", LOG_PATH_127 , (j/5000)*5000, j);
                    syslog(LOG_INFO, "[%s:%d] filename %s", __FUNCTION__, __LINE__, filename);
                    if((ret=rename(filename, buf)) != 0)
                        syslog(LOG_ERR,"[%s:%d] rename failure: %s",
                                    __FUNCTION__, __LINE__,  strerror(ret));
                }
                /*
                for(j=0; j<=tail; j++){
                    ret = j + (MAX_ROWFILE - ptc_row_upload);
                    strncpy(filename, ddcode[ret], 8);
                    filename[8] = 0;
                    sprintf(buf, "%s/backup/%s", LOG_PATH, filename);
                    if(stat(buf, &sb) != 0) mkdir(buf, 0755);
                    sprintf(buf, "%s/backup/%s/%s-U%s.tw", LOG_PATH, filename,ddcode[ret], ucode);
                    sprintf(filename, "%s/P%06d/%06d.tw", LOG_PATH, (j/5000)*5000, j);
                    if((ret=rename(filename, buf)) != 0)
                        syslog(LOG_ERR,"[%s:%d] rename failure: %s",
                                    __FUNCTION__, __LINE__,  strerror(ret));
                }
                */
            }
            sync();
            if(tail  == MAX_ROWFILE) ptc_row_upload_127 = 0;
            else ptc_row_upload_127 = tail ;
            syslog(LOG_INFO, "[%s:%d] down ptc_row_upload is %d tail is %d\n",__FUNCTION__, __LINE__,ptc_row_upload_127 ,tail);
            tail = ptc_row_upload_127 +1;
            if(tail >= MAX_ROWFILE) tail -= MAX_ROWFILE;
            syslog(LOG_INFO, "[%s:%d] Final ptc_row_upload is %d tail is %d\n",__FUNCTION__, __LINE__,ptc_row_upload_127 ,tail);
            break;
        }

        syslog(LOG_INFO, "[%s:%d]  end of UploadDataProcess",
                            __FUNCTION__, __LINE__);
    }while(1);

    return 0;
}