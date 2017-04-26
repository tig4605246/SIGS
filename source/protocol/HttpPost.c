/*
    Name: Xu Xi-Ping
    Date: April 25,2017
    Last Update: April 25,2017
    Program statement: 
        This program will manage all other sub processes it creates.
        Also, it's responsible for opening and closing all sub processes.

*/


#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h>

#define SA      struct sockaddr
#define MAXLINE 16384
#define MAXSUB  16384


//I don't know what's this for

extern int h_errno;

//Server ip

char *hname = "140.118.70.136";

//Which port server is using

#define SERVERPORT 9000

//the RESTAPI 

char *page = "/solar_rowdata";
	

ssize_t process_http( char *content)
{
    
    int sockfd;
	struct sockaddr_in servaddr;
	char **pptr;
	char str[50];
	struct hostent *hptr;
	char sendline[MAXLINE + 1], recvline[MAXLINE + 1];
    int i=0;
    char *error = NULL;
	ssize_t n;


    if ((hptr = gethostbyname(hname)) == NULL) 
    {

		fprintf(stderr, " gethostbyname error for host: %s: %s",hname ,hstrerror(h_errno));
		return -1;

	}
	printf("hostname: %s\n", hptr->h_name);
	if (hptr->h_addrtype == AF_INET && (pptr = hptr->h_addr_list) != NULL) 
    {

		printf("address: %s\n",inet_ntop( hptr->h_addrtype , *pptr , str , sizeof(str) ));

	} 
    else
    {

		fprintf(stderr, "Error call inet_ntop \n");
        return -1;

	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERVERPORT);
	inet_pton(AF_INET, str, &servaddr.sin_addr);

	connect(sockfd, (SA *) & servaddr, sizeof(servaddr));


	snprintf(sendline, MAXSUB,
		 "POST %s HTTP/1.1\r\n"
		 "Host: %s\r\n"
		 "Content-type: application/json; charset=UTF-8\r\n"
		 "Content-Length: %ld\r\n\r\n"
         "User-Agent: Kelier/0.1"
		 "%s", page, hname, strlen(content), content);

	write(sockfd, sendline, strlen(sendline));

	while ((n = read(sockfd, recvline, MAXLINE)) > 0) 
    {

		recvline[n] = '\0';
        error = strstr(recvline,"fail");
        if(error != NULL)
        {

            printf("\033[1;31m""%s\n""\033[1;37m", recvline);
            return -1;

        }
        else
        {
            printf("\033[1;32m""Post Successfully\n""\033[1;37m");
        }
		
	}

    close(sockfd);

	return n;

}

int main(void)
{

	int ret = 0;

	char *content = "mode=login&user=test&password=test\r\n";

    //content means what we want to post

	ret = process_http(content);
	
    printf("Post return %d\n",ret);

	exit(0);

}