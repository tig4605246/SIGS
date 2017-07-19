
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>


#if 1

int main(int argc, char *argv[])
{

    FILE *fp = NULL;
    int i = 0;
    char buf[128];
    char *infoName = NULL;

    printf("argv[1] is %s\n",argv[1]);

    fp = fopen(argv[1],"r");

    if(fp == NULL)
    {
        printf("fp is NULL, fuck\n");
        return -1;
    }

    while(!feof(fp))
    {

        printf("\033[1;33m""loop %d\n""\033[m",i++);

        
        if( fgets(buf,127,fp) != NULL )
        {
            
            if(buf[0] != '#')
            {

                infoName = strtok(buf," ");
                printf("%s=",infoName);
                infoName = strtok(NULL," ");
                infoName[strlen(infoName) - 1] = '\0';
                printf("%s\n",infoName);

            }   
            else
            {
                printf("%s\n",buf);
            }
            
        }

    }
    fclose(fp);
    return 0;

}

#else

int main(int argc, char *argv[])
{

    struct stat sb;

    if(stat(argv[1], &sb) == 0)
    {
        printf("%s return 0\n",argv[1]);
    }
    else
    {
        printf("%s return -1\n",argv[1]);

    }
    return 0;
}

#endif