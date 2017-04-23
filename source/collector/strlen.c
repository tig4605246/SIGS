/*
    Name: Xu Xi-Ping
    Date: March 1,2017
    Last Update: March 8,2017
    Program statement: 
        This program will manage all other sub processes it creates.
        Also, it's responsible for opening and closing all sub processes.

*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

void main()
{

    char strings[100];


    bzero(strings,sizeof(strings));

    sprintf(strings,"ABCDEFGH");
    printf("length %lu last 3 : %c %c %c\n",strlen(strings),*(strings +strlen(strings)-3),*(strings +strlen(strings)-2),*(strings +strlen(strings)-1));
    return;

}