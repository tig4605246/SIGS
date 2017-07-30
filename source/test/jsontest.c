#include <stdio.h>
#include <stdlib.h>

#include "../thirdparty/cJSON.h"



int main(int argc, char *argv[])
{

    FILE *fp = NULL;
    char buf[2048] = {'\0'};
    char *format;
    cJSON *ptr;
    cJSON *tmp;
    cJSON *target;
    cJSON *valuename;
    int ret = -1;
    fp = fopen("./solar_output_json", "r");

    fgets(buf, 2047, fp);

    ptr = cJSON_Parse(buf);

    format = cJSON_Print(ptr);

    printf("format:\n%s\n", format);

    ret = cJSON_HasObjectItem(ptr, "rows");

    printf("rows return %d\n",ret);

    ret = cJSON_HasObjectItem(ptr, "ZZZ");

    printf("ZZZ return %d\n",ret);

    tmp = cJSON_GetObjectItem(ptr, "rows");

    target = cJSON_GetArrayItem(tmp, 1);

    valuename = cJSON_GetObjectItem(target, "Station_ID");

    format = cJSON_Print(valuename);

    printf("format:\n%s\n", format);

    return 0;



}