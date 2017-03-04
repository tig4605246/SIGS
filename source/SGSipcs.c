/*

    Name: Xu Xi-Ping
    Date: March 3,2017
    Last Update: March 3,2017
    Program statement: 
        we write the detail of the function here 

*/

int sgsInitDeviceInfo(deviceInfo *deviceInfoPtr)
{
    FILE *deviceConfigFile = NULL;
    int line = 0;
    char buf[128] = "";
    char *sp1 = NULL, *sp2 = NULL, *sp3 = NULL;
    deviceInfo *tempPtr = NULL;
    deviceInfo *tempPtr2 = NULL;

    deviceInfoPtr = NULL;

    deviceConfigFile = fopen("./device.conf","r");
    if(deviceConfigFile == NULL)
    {

        printf("[%s,%s] Can't find the device.conf file\n",__FUNCTION__,__LINE__);
        return -1;

    }
    while(!feof(fd))
    {

        memset(buf,'\0',sizeof(buf));
        if(fscanf(fd, "%[^\n]\n", buf) < 0) break;
        if(!strlen(buf)) continue;
        if(buf[0] == '#') continue;
        line++;

        //split the config  buf is deviceName, sp1 is interface, sp2 is protocolConfig

        sp1 = strchr(buf, ',');
        sp2 = sp3 = NULL;
        if(sp1 != NULL) sp2 = strchr(sp1+1, ',');
        if(sp2 != NULL) sp3 = strchr(sp2+1, ',');
        if(sp1 == NULL || sp2 == NULL || sp3 == NULL)
        {

            printf("[ERR] Invalid bus config in line %d in device.conf \n",line);
            sgsDeleteAll(deviceInfoPtr, -1);
            fclose(deviceConfigFile);
            return -1;

        }
        *sp1 = 0; sp1 += 1;
        *sp2 = 0; sp2 += 1;
        *sp3 = 0; sp3 += 1;

        //uncomment this if you want some config checking things

        /*
        if(deviceInfoPtr != NULL)
        {

            tempPtr = deviceInfoPtr;
            do{

                if(!strcmp(buf, tempPtr->deviceName))
                {

                    printf("[ERR] duplicate interface '%s' in line %d in %s\n",
                              sp1, line, BOARDCONF_FILE);
                    break;

                }
                if(!strcmp(sp1, tempPtr->interface))
                {

                    printf("[ERR] duplicate device node '%s' in line %d in %s\n",
                              sp1, line, BOARDCONF_FILE);
                    break;

                }
                tempPtr = tempPtr->next;

            }while(tempPtr != NULL);
            
            if(tempPtr != NULL)
            {

                sgsDeleteAll(deviceInfoPtr, -1);
                fclose(fd);
                return -1;

            }
        }
        */

        //Put device data into deviceInfo

        tempPtr = (deviceInfo *)malloc(sizeof(deviceInfo));
        memset((void *)tempPtr, 0, sizeof(deviceInfo));
        strncpy(tempPtr->deviceName, buf, 31); 
        btag->deviceName[31] = 0;
        strncpy(tempPtr->interface, sp1, 31); 
        btag->interface[31] = 0;
        strncpy(tempPtr->protocolConfig, sp2, 31); 
        btag->protocolConfig[31] = 0;

        //put tempPtr into linked-list which head is deviceInfoPtr;

        if(deviceInfoPtr == NULL)
        {

            deviceInfoPtr = tempPtr;
            deviceInfoPtr->next == NULL;

        }
        else
        {
            tempPtr2 = deviceInfoPtr;
            while(tempPtr2->next != NULL)
                tempPtr2 = tempPtr2->next;
            tempPtr2->next = tempPtr;
            tempPtr2->next->next = NULL;

        }

    }

    return 0;

}

int load_tagconf(int CreateShm, struct bus_tag *btag);
int sgsInitDataInfo(deviceInfo *deviceInfoPtr, int create)
{
    int line=0, zero=0, j, shmid;
    char buf[512], *sp1, *sp2;
    FILE *dataConfigFile = NULL;
    void *shm;
    struct conf_tag *ctag, *ctag2, *ctag_former=NULL;
    struct shmem_tag *stag;

    dataInfo *temp = NULL;

    if((dataConfigFile=fopen("data.conf", "r"))==NULL)
    {

        perror("sgsInitDataInfo");
        return -1;

    }

    while(!feof(fd))
    {

        if(fscanf(fd, "%[^\n]\n", buf) < 0) 
            break;
        line++;
        //printf("[LINE %d]: %s\n", i, buf);
        if(!strlen(buf))
        { 
            
            zero++; 
            continue;
            
        }
        if(buf[0] == '#')
        { 
            
            zero++; 
            continue;
        
        }
        
        temp = (dataInfo *)malloc(sizeof(dataInfo));
        memset((void *)temp, 0, sizeof(dataInfo));
        ctag->next = NULL;
        if(ctag_head == NULL)
        {

            ctag_head = ctag_tail = ctag;

        }
        else
        {

            ctag_tail->next = ctag;
            ctag_tail = ctag;

        }
        sp2 = buf;
        for(j=0; j<7; j++)
        {

            sp1 = sp2;
            if(j<6)
            {

                sp2 = strchr(sp1, ',');
                if(sp2 == NULL)
                {

                    printf("[ERR] Invalid format in line %d in %s\n",
                               line, TAGCONF_FILE);
                    free_ctag();
                    fclose(fd);
                    return -1;

                }
                *sp2 = 0;
                sp2++;

            }
            switch(j)
            {

              case 0: //'name[64]' field in string char formart
                ctag2 = ctag_head;
                while(ctag2 != NULL)
                {

                    if(!strcmp(ctag2->name, sp1))
                    {

                        printf("[ERR] duplicate tag name '%s' in line %d in %s\n", sp1, line, TAGCONF_FILE);
                        free_ctag();
                        fclose(fd);
                        return -1;

                    }
                    ctag2 = ctag2->next;

                }
                strncpy(ctag->name, sp1, 63); ctag->name[63] = 0;
                break;
              case 1: //'daemon[32]' field in string char format
                strncpy(ctag->daemon, sp1, 31); ctag->daemon[31] = 0;
                break;
              case 2: //'bus_name[32]' field in string char format
                strncpy(ctag->bus_name, sp1, 31); ctag->bus_name[31] = 0;
                if(strcmp(ctag->bus_name, "SystemInfo"))
                {

                    ctag2 = ctag_head;
                    while(ctag2 != NULL)
                    {

                        if(!strcmp(ctag2->bus_name, ctag->bus_name))
                        {

                            if(strcmp(ctag2->daemon, ctag->daemon))
                            {

                                printf("[ERR] bus '%s' is assigned more than one kind of protocol in line %d in %s\n", ctag->bus_name, line, TAGCONF_FILE);
                                free_ctag();
                                fclose(fd);
                                return -1;

                            }

                        }
                        ctag2 = ctag2->next;

                    }

                }
                break;
              case 3: //'id' field in integer format
                ctag->id = atoi(sp1);
                break;
              case 4: //'func' field in integer format
                ctag->func = atoi(sp1);
                break;
              case 5: //'addr' field in integer format
                ctag->addr = atoi(sp1);
                break;
              case 6: //'cmd' field in string char format
                strcpy(ctag->cmd, sp1);
                break;
              default: //invalid field
                printf("[ERR] The format of tag configuration file is wrong!\n");
                free_ctag();
                fclose(fd);
                return -1;

            }

        }
        //printf("  %d: name=%s, daemon=%s, bus=%s, id=%d, addr=%d, cmd=%s, rectime=%d\n", i, ctag->name, ctag->daemon, ctag->bus, ctag->id, ctag->addr, ctag->cmd, ctag->rectime_in_sec);
    }
    fclose(fd);
    if(ctag_head == NULL) return 0;
    line -= (zero + 1);

    if(CreateShm)
    {

        /*
         * Create the segment.
         */
        shmid = shmget(SHM_KEY, sizeof(struct conf_tag)*line,
                          IPC_EXCL | IPC_CREAT | 0666);
        //shmid = shmget(SHM_KEY, sizeof(struct conf_tag)*i, IPC_CREAT | 0666);

    }
    else
    {

        /*
         * Locate the segment.
         */
        shmid = shmget(SHM_KEY, sizeof(struct conf_tag)*line, 0666);

    }
    if(shmid < 0){

        perror("shmget");
        free_ctag();
        return -2;

    }
    /*
     * Now we attach the segment to our data space.
     */
    if ((shm = (void *) shmat(shmid, NULL, 0)) == (char *) -1) 
    {

        perror("shmat");
        free_ctag();
        return -2;

    }
    if(CreateShm) memset(shm, 0, sizeof(struct conf_tag)*line);

    ctag = ctag_head;
    stag = (struct shmem_tag *)shm;
    while(ctag != NULL)
    {

        if(CreateShm)
        {

            /*
             * Now put some things into the memory for the
             * other process to read.
             */
//            stag->id = ctag->id;
//            stag->func = ctag->func;
//            stag->addr = ctag->addr;
//            strcpy(stag->cmd, ctag->cmd);
            stag->flag = FLAG_NOT_READY;
        }
        else if(btag != NULL)
        {

            if(strcmp(ctag->bus_name, btag->bus_name))
            {

                if(ctag_former != NULL)
                {

                    ctag_former->next = ctag->next;
                    free(ctag);
                    ctag = ctag_former->next;

                }
                else
                {

                    ctag_former = ctag;
                    ctag = ctag->next;

                }
                stag++;
                continue;

            }
            if(ctag_head2 == NULL)
            {

                ctag_head2 = ctag;

            }
            ctag_former = ctag;

        }
        ctag->poll = stag;
        stag++;
        ctag = ctag->next;

    }
    return shmid;

}


int sgsCreateMsgQueue(key_t key, int create)
{

	int msgid;
	msgid=msgget(key, /*IPC_NOWAIT |*/ IPC_CREAT | 0600);

	if(msgid == -1)
    {
		printf("Open queue failed, msgget return %d\n",msgid);
		return -1;
	}
	printf("Open queue successfully id is %d\n",msgid);

	return msgid;
}

int sgsDeleteMsgQueue(int msgid)
{
	if (msgid == -1) 
    {
		printf("Message queue does not exist.\n");
		return 0;
	}

	if (msgctl(msgid, IPC_RMID, NULL) == -1) 
    {
		fprintf(stderr, "Message queue could not be deleted.\n");
		return -1;
	}

	printf("Message queue was deleted.\n");

	return 0;
}

int sgsSendQueueMsg(int msgid, char *message, int msgtype )
{
	struct msgbuff ptr ;
	int result =0;

	if(strlen(message) > 1024)
    {
		printf("The message is too long\n");
		return -1;
	}
	printf("Prepare to send a message to queue (type %d)\n",msgtype);
	ptr.mtype=(long)msgtype;

	strcpy(ptr.mtext,message);
	printf("ptr %ld %s\n",ptr.mtype,ptr.mtext);
	result = msgsnd(msgid,&ptr,sizeof(struct msgbuff)-sizeof(long),IPC_NOWAIT);
	if(result == -1)
    {
		perror("result");
		return -1;
	}
	printf("msgsnd return: %d \n",result);
	return result;
}

int sgsRecvQueueMsg(int msgid, char *buf, int msgtype )
{
	struct msgbuff ptr ;
	int result = 0;
	if(buf == NULL)
    {
		printf("The buf is NULL\n");
		return -1;
	}
	printf("Prepare to receive a message from queue\n");
	ptr.mtype=(long)msgtype;
	result = msgrcv(msgid,&ptr,sizeof(struct msgbuff) - sizeof(long),msgtype,IPC_NOWAIT);//IPC_NOWAIT);	//recv msg type=1
	if(result == -1)
    {
		printf("Currently no queue message available\n");
		perror("msgrcv");
		return -1;
	}
	else
    {
		printf("Queue message received, length %d bytes, type is %ld\n",result,ptr.mtype);
		printf("message:\n%s",ptr.mtext);
		strcpy(buf,ptr.mtext);
		return 0;
	}

}