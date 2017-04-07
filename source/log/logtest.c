#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{

    struct stat st;

    char filename[64];
    int ret = 0;

    memset(filename,'\0',sizeof(filename));
    sprintf(filename, "/run/ppap");
    ret = stat(filename, &st) ;
    printf("%s stat is %d\n",filename,ret);

    memset(filename,'\0',sizeof(filename));
    sprintf(filename, "/conf/init.conf");
    ret = stat(filename, &st) ;
    printf("%s stat is %d\n",filename,ret);

    return 0;
   

}