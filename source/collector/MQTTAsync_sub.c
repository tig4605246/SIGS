/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *******************************************************************************/
 /*

    Last edited by Kevin at 2017/03/22
    edited list:
        connlost : Restart the program when reconnection fails
        msgarrvd : Store message to the shared memory
        onConnect : Restart the program when subscribtion fails

        main : The program will stores pid when started, set up sigaction to SIGUSR1

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "MQTTAsync.h"

#include "../ipcs/SGSipcs.h"


#if !defined(WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif

#define ADDRESS     "140.118.121.55"
#define CLIENTID    "KevinRaul"
#define TOPIC       "a"
#define PAYLOAD     "Fuck the world"
#define QOS         1
#define TIMEOUT     10000L


volatile MQTTAsync_token deliveredtoken;

int disc_finished = 0;
int subscribed = 0;
int finished = 0;
dataInfo *dataInfoPtr = NULL;
deviceInfo *deviceInfoPtr = NULL;
int shmID = 0;


void connlost(void *context, char *cause);


int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message);


void onDisconnect(void* context, MQTTAsync_successData* response);


void onSubscribe(void* context, MQTTAsync_successData* response);


void onSubscribeFailure(void* context, MQTTAsync_failureData* response);


void onConnectFailure(void* context, MQTTAsync_failureData* response);


void onConnect(void* context, MQTTAsync_successData* response);

void stopAndAbort(int sigNum);


//Intent : set up dataInfo and deviceInfo (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error message

int initializeInfo();

int main(int argc, char* argv[])
{
    
	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTAsync_token token;
    
	int rc;
	int ch;
    int ret;
    
    //For writing to pid file

    FILE *pidFile = NULL;

    //struct for catching signals (SIGUSR1)

    struct sigaction act, oldact;

    //Stores own pid to the file
    printf("Kai To De Di Fang\n");
    
    pidFile = fopen("./pid/MQTTAsync_sub.pid","w");
    if(pidFile != NULL)
    {
        fprintf(pidFile,"%d",getpid());
        fclose(pidFile);
    }

    

    //initialize sigaction
    act.sa_handler = (__sighandler_t)stopAndAbort;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);

    ret = initializeInfo();
    if(ret < 0)
    {

        printf("[%s,%d] Failed to initialize the configure and shared memory, quitting\n",__FUNCTION__,__LINE__);

    }

    printf("Oswin's pussy\n");

	MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{

		printf("Failed to start connect, return code %d\n", rc);
		exit(EXIT_FAILURE);

	}

	while	(!subscribed)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	if (finished)
		goto exit;

	do 
	{

		ch = 'd';

	} while (ch!='Q' && ch != 'q');

	disc_opts.onSuccess = onDisconnect;
	if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
	{

		printf("Failed to start disconnect, return code %d\n", rc);
		exit(EXIT_FAILURE);

	}
 	while	(!disc_finished)
		#if defined(WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

exit:
	MQTTAsync_destroy(&client);
    stopAndAbort(-1);
 	return rc;


}


void connlost(void *context, char *cause)
{

	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;
    pid_t pid;

	printf("\nConnection lost\n");
	printf("     cause: %s\n", cause);

	printf("Reconnecting\n");
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{

		printf("Failed to start connect, return code %d\n", rc);
        printf("[%s,%d] Restarting...\n",__FUNCTION__,__LINE__);
        pid = fork();

        if(pid == 0)
        {

            execlp("./MQTTAsync_sub","./MQTTAsync_sub",NULL);
            perror("execlp");
            exit(-1);

        }

	    exit(EXIT_FAILURE);

	}

    return ;

}


int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{

    int i;
    char* payloadptr;
    dataLog data;
    dataInfo *temp = dataInfoPtr;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = message->payload;

    while(temp != NULL)
    {
        printf("temp->valueName: %s topicName: %s \n",temp->valueName,topicName);
        if(strcmp(temp->valueName,topicName))
            temp = temp->next;
        else 
        {
            printf("break La\n");
            break;

        }

    }
    for(i=0; i<message->payloadlen; i++)
    {

        putchar(*payloadptr++);

    }
    putchar('\n');
    if(temp == NULL)
    {

        printf("No matched tag\n");
        MQTTAsync_freeMessage(&message);
        MQTTAsync_free(topicName);
        return -1;

    }
    payloadptr = message->payload;

    data.valueType = STRING_VALUE;
    strncpy(data.value.s,payloadptr,message->payloadlen);
    sgsWriteSharedMemory(temp,&data);

    
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;

}


void onDisconnect(void* context, MQTTAsync_successData* response)
{

	printf("Successful disconnection\n");
	disc_finished = 1;

}


void onSubscribe(void* context, MQTTAsync_successData* response)
{

	printf("Subscribe succeeded\n");
	subscribed = 1;

}

void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{

    pid_t pid;
	printf("Subscribe failed, rc %d\n", response ? response->code : 0);
    pid = fork();

    if(pid == 0)
    {

        execlp("./MQTTAsync_sub","./MQTTAsync_sub",NULL);
        perror("execlp");
        exit(-1);

    }
	exit(EXIT_FAILURE);

}


void onConnectFailure(void* context, MQTTAsync_failureData* response)
{

    pid_t pid;
	printf("Connect failed, rc %d\n", response ? response->code : 0);
    pid = fork();

    if(pid == 0)
    {

        execlp("./MQTTAsync_sub","./MQTTAsync_sub",NULL);
        perror("execlp");
        exit(-1);

    }
	exit(EXIT_FAILURE);

}


void onConnect(void* context, MQTTAsync_successData* response)
{

	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;
    pid_t pid;

	printf("Successful connection\n");

	printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
	opts.onSuccess = onSubscribe;
	opts.onFailure = onSubscribeFailure;
	opts.context = client;

	deliveredtoken = 0;

	if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS)
	{

		printf("Failed to start subscribe, return code %d\n", rc);
        pid = fork();

        if(pid == 0)
        {

            execlp("./MQTTAsync_sub","./MQTTAsync_sub",NULL);
            perror("execlp");
            exit(-1);

        }
        
		exit(EXIT_FAILURE);

	}

}

void stopAndAbort(int sigNum)
{

    sgsDeleteDeviceInfo(deviceInfoPtr);
    sgsDeleteDataInfo(dataInfoPtr,-1);
    exit(0);

}

int initializeInfo()
{

    int ret = 0;
    printf("KILLKILLKILL\n");
    ret = sgsInitDeviceInfo(&deviceInfoPtr);
    if(ret != 0)
    {

        printf("[%s,%d] init device conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    } 

    ret = sgsInitDataInfo(NULL, &dataInfoPtr, 0);
    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    shmID = ret;

    return 0;

}

