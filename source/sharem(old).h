/* 2017/01/09 Kevin */
/* Version 2.1 */

/*
	Every function has a brief comment at the start of it
	This header is mainly for the setting of the IPC funtions and the general parameters
*/

#include <error.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/syslog.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>





int errno;

struct msgbuff{
	long mtype;
	char mtext[512];
};//this struct is for queue to use

struct Bus_tag
{
    char bus_name[32];

    //ttyS0 / ttyS1 / [ip address of ethernet converter]:[port]

    char dev_node[32];

    //RS232 / RS422 / RS485

    char interface[16];

    // [baudrate],[len][parity][stop bit]. For example: b115200,8n1

    char parameter[32];

    int delay_time;
};

struct conf_tag{
    char name[16];//data name. For example, Response000.
    char daemon[16];// slave brand. For example, deltarpi.
    char bus_name[32];// /dev/ttyS* serial output 
    unsigned char lock;// shared memory lock
    int id;//modbus id
    int func;//modbus function
    int addr;//modbus address
    int read_length;//modbus read length
    unsigned char cmd[64];// combined command for write() function
    struct timeb read_time;// timestamp of data
    int flag;// used by deltarpi
    int response_length;// modbus response length
    int response[64];// modbus response value
    int dev_status;// modbus slave status  1 is alive , 0 is no response
    int fail_count;// failure times of receiving response from slave

}__attribute__((aligned(1), packed));

struct shm_param{
	void *sharem;// pointer to shared memory
	int shid;// shared memory id
	int semid;// semaphore id
	key_t key;// key of the semaphore
	size_t size;// the size of the shared memory
};

struct data_tag{// Please reference to the conf_tag
	char name[16];
    char daemon[16];
    int id;
    unsigned char cmd[64];
    struct timeb read_time;
    int flag;
    int response_length;
    int response[32];
    int dev_status;
}__attribute__((aligned(1), packed));

struct dtable{// 
	struct data_tag store[MAX_DATA];
}__attribute__((aligned(1), packed));

struct dtable iven;// global variable of dtable. For ptc_datameter to make backup data

int gwlog_save(char *type, char *message)
{
	/* 
		This function is used to save message for email service 
	*/
    int fd, ret;
    char tempfile[256], buf[1024];
    time_t timep;
    struct tm *p;

    if(!strcmp(type, "ptc")){
    }else if(!strcmp(type, "ssh")){
    }else if(!strcmp(type, "meter")){
    }else if(!strcmp(type, "sys")){
    }else{
        syslog(LOG_ERR, "[%s:%d] ERR! Invalid type '%s'",
               __FUNCTION__, __LINE__, type);
        return -1;
    }
    time(&timep);
    p = localtime(&timep);
    sprintf(tempfile,"/tw_daemon_log/log/mail/%s/%d%02d%02d%02d%02d%02d-XXXXXX",
                     type,
                     p->tm_year + 1900,
                     p->tm_mon + 1,
                     p->tm_mday,
                     p->tm_hour,
                     p->tm_min,
                     p->tm_sec);
    if((fd = mkstemp(tempfile)) <= 0){
        syslog(LOG_ERR, "[%s:%d] ERR! Fail in creating a unique temporary file"
                        " '%s'", __FUNCTION__, __LINE__, tempfile);
        return -2;
    }
    sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d - %s\n",
                 p->tm_year + 1900,
                 p->tm_mon + 1,
                 p->tm_mday,
                 p->tm_hour,
                 p->tm_min,
                 p->tm_sec,
                 message);

    ret = write(fd, buf, strlen(buf));
    close(fd);
    if(ret != strlen(buf)){
        syslog(LOG_ERR, "[%s:%d] ERR! Fail in writing temporary file",
                        __FUNCTION__, __LINE__);
        return -3;
    }
    sprintf(buf, "%s.txt", tempfile);
    if(rename(tempfile, buf) != 0)
        syslog(LOG_ERR, "[%s:%d] fail in removeing %s log file",
                        __FUNCTION__, __LINE__, tempfile);

    return 0;
}




int OpenSHM(struct shm_param *ptr, size_t size, int create){
	/*
		Create shared memory with certain size and key
		if create == 1, open a new shared memory with the key
		if create == 0, open an existed shared memory with the key
	*/
	struct conf_tag *temp;
	printf("OpenSHM: ptr->shid %d ptr->key %d\n",ptr->shid,ptr->key);	
	if(create) {
		ptr->shid=shmget(ptr->key,size,0600|IPC_CREAT|IPC_EXCL ); //create a new shared memory with key
		if(ptr->shid==-1)
		{
			perror("shmget");
			return -1;
		}

	}
	else ptr->shid=shmget(ptr->key,size,0600);// get an exist shared memory with key

	if(ptr->shid==-1)
	{
		perror("shmget");
		return -1;
	}

	
	ptr->sharem=shmat(ptr->shid,NULL,0);
	if(ptr->sharem==NULL)
	{
		perror("shmat");

		return -1;
	}
	if(create){
		memset(ptr->sharem,'\0',size);
		temp = (struct conf_tag*)ptr->sharem;
		temp->lock = 0;
	}

	return 0;
}

int DetachSHM(void *sharem){
	/*
		Detach the shared memory before you delete it, or you might fail to delete it.
	*/
	int ret;
	ret = shmdt(sharem);
	if(ret == 0){
		printf("Detached SHM successfully\n");
		return 0;
	}
	else{
		perror("DetachSHM");
		return -1;
	}

}

int DeleteSHM(int shm_id){
	/*
		Delete the shared memory
	*/
	int ret;
	if(shm_id>=0){
		ret = shmctl(shm_id,IPC_RMID,0);
		if(ret != 0){
			perror("DeleteSHM");
			return -1;
		}
		printf("delete SHM successfully\n");
		return 0;
	}
	else{
		printf("shm_id is %d\n",shm_id);
		return -1;
	}
}

void child_stop_handler(struct sigaction *act, void *sharem)
{
	int ret;
    printf("[%s] Abort the child\n", __FUNCTION__);
    ret = DetachSHM(sharem);
    printf("DetachSHM return %d\n",ret);
    exit(0);
}

void child_signal_init(void)
{
    struct sigaction act, oldact;

    act.sa_handler = (__sighandler_t)child_stop_handler;
    act.sa_flags = SA_ONESHOT|SA_NOMASK;
    sigaction(SIGUSR1, &act, &oldact);
}

int WriteSHM(struct shm_param *ptr, struct conf_tag *data, size_t size){
	/*
		Write back to the shared memory
	*/

	int count = 100;

	struct conf_tag *tmp =  (struct conf_tag *)ptr->sharem;

	while(count)
	{	
		//if(count%10 == 0) printf("count %d tmp->flag is %d\n",count,tmp->flag);
		if(tmp->lock==0)
		{	
			tmp->lock = 1;
			data->lock = 1;
			//printf("writing data in SHM\n");
			memcpy(ptr->sharem, data, size);
			//printf("writing finished\n");
			tmp->lock = 0;
			return 0;
		}
		usleep(30000);
		count--;
	}
	printf("--Writing shm failed!! flag is racing--\n");
	syslog(LOG_INFO, "[%s,%d]WriteSHM failed",__FUNCTION__, __LINE__);
	return -1;
}

int ReadSHM(struct shm_param *ptr, struct conf_tag *data, size_t size){
	/*
		Update data from the shared memory
	*/

	int count = 100;


	struct conf_tag *tmp =  (struct conf_tag *)ptr->sharem;

	//printf("-------I am goinggggg count is %d-----------\n",count);
	while(count)
	{
		if(count%10 == 0) printf("count %d tmp->lock is %d\n",count,tmp->flag);
		if(tmp->lock == 0 )
		{	
			tmp->lock = 1;
			
 			//printf("Reading data in SHM\n");
			memcpy(data, ptr->sharem, size);
			//printf("Reading Finished\n");
			tmp->lock = 0;
			return 0;
		}
		usleep(30000);
		count--;
	}
	printf("--Read shm failed!! flag is racing--\n");
	syslog(LOG_INFO, "[%s,%d]ReadSHM failed",__FUNCTION__, __LINE__);
	return -1;

}


int Create_msg_queue(key_t key, int create){

	int msgid;
	msgid=msgget(key, /*IPC_NOWAIT |*/ IPC_CREAT | 0600);

	if(msgid == -1){
		printf("Open queue failed, msgget return %d\n",msgid);
		return -1;
	}
	printf("Open queue successfully id is %d\n",msgid);

	return msgid;
}

int Delete_msg_queue(int msgid){
	if (msgid == -1) {
		printf("Message queue does not exist.\n");
		return 0;
	}

	if (msgctl(msgid, IPC_RMID, NULL) == -1) {
		fprintf(stderr, "Message queue could not be deleted.\n");
		return -1;
	}

	printf("Message queue was deleted.\n");

	return 0;
}

int Send_queue_msg(int msgid, char *message, int msgtype ){
	struct msgbuff ptr ;
	int result =0;

	if(strlen(message) > 1024){
		printf("The message is too long\n");
		return -1;
	}
	printf("Prepare to send a message to queue (type %d)\n",msgtype);
	ptr.mtype=(long)msgtype;

	strcpy(ptr.mtext,message);
	printf("ptr %ld %s\n",ptr.mtype,ptr.mtext);
	result = msgsnd(msgid,&ptr,sizeof(struct msgbuff)-sizeof(long),IPC_NOWAIT);
	if(result == -1){
		perror("result");
		return -1;
	}
	printf("msgsnd return: %d \n",result);
	return result;
}

int Recv_queue_msg(int msgid, char *buf, int msgtype ){
	struct msgbuff ptr ;
	int result = 0;
	if(buf == NULL){
		printf("The buf is NULL\n");
		return -1;
	}
	printf("Prepare to receive a message from queue\n");
	ptr.mtype=(long)msgtype;
	result = msgrcv(msgid,&ptr,sizeof(struct msgbuff) - sizeof(long),msgtype,IPC_NOWAIT);//IPC_NOWAIT);	//recv msg type=1
	if(result == -1){
		printf("Currently no queue message available\n");
		perror("msgrcv");
		return -1;
	}
	else{
		printf("Queue message received, length %d bytes, type is %ld\n",result,ptr.mtype);
		printf("message:\n%s",ptr.mtext);
		strcpy(buf,ptr.mtext);
		return 0;
	}

}



int load_path(char *filename){
	/*
		Plan to use this function to get all paths (confs, programs, pid files, datas...etc)

		char realtime[16];
		char gw_data[16];
		char gw_data_127[16];
		char data_collection[16];
		char initconf_1[16];
		char initconf_2[16];


		char key_path[64];
		int create_entry;
		int max_data;
		
		char daemon_modbus_1[64];
		char daemon_modbus_2[64];
		char daemon_realtime[64];
		char daemon_seg[64];
		char daemon_tw_daemon[64];
		char daemon_tw_daemon_127[64];
		
		char daemon_realtime_pid[64];
		char daemon_seg_pid[64];
		char daemon_tw_daemon_pid[64];
		char daemon_tw_daemon_127_pid[64];

		char conf_path[64];// /conf, kinda like this
		char conf_realtime[64];
		char conf_seg[64];
		char conf_tw_daemon[64];
		char conf_tw_daemon_127[64];
		char conf_modbus_1[64];
		char conf_modbus_2[64];

		struct's name is [init_param]
	*/
	/*
	while(running_programs<2){
		memset(config,'\0',sizeof(config));
		if(running_programs == 0){
			sprintf(filename, "%s%s",SIM_CONF_PATH, INITCONF);
			fp = fopen(filename,"r");
			if(fp == NULL){
				printf("can't read init.conf, skip to init2.conf\n ");
				sprintf(filename, "%s%s",SIM_CONF_PATH, INITCONF_2);
				fp = fopen(filename,"r");
				if(fp == NULL){
					printf("can't read init2.conf, quitting...\n ");
					return -1;
				}
			}	
		}
		else if(running_programs == 1){
			sprintf(filename, "%s%s",SIM_CONF_PATH, INITCONF_2);
			fp = fopen(filename,"r");
			if(fp == NULL){
				printf("can't read init2.conf, skipping...\n ");
				break;
			}
		}
	*/
	struct stat sb;
	FILE *p_file;
	char buf[128];
	int line=0;
	int i;
	if(stat(filename, &sb) != 0){
		p_file = fopen(filename,"r");
		if(p_file == NULL){
			printf("file '%s' fopen failed, Please check the permission or so.\n",filename);
		}
	}
	else{
		printf("File '%s' does not exist, Please check it first.\n",filename);
		return -1;
	}
	/*
	memset(init_param.realtime,'\0',sizeof(init_param.realtime));
	memset(init_param.gw_data,'\0',sizeof(init_param.gw_data));
	memset(init_param.gw_data_127,'\0',sizeof(init_param.gw_data_127));
	memset(init_param.data_collection,'\0',sizeof(init_param.data_collection));
	memset(initconf_1,'\0',sizeof(init_param.initconf_1));
	memset(init_param.initconf_2,'\0',sizeof(init_param.initconf_2));

	memset(init_param.key_path,'\0',sizeof(init_param.key_path));

	memset(init_param.daemon_modbus_1,'\0',sizeof(init_param.daemon_modbus_1));
	memset(init_param.daemon_modbus_2,'\0',sizeof(init_param.daemon_modbus_2));
	memset(init_param.daemon_realtime,'\0',sizeof(init_param.daemon_realtime));
	memset(init_param.daemon_seg,'\0',sizeof(init_param.daemon_seg));
	memset(init_param.daemon_tw_daemon,'\0',sizeof(init_param.daemon_tw_daemon));
	memset(init_param.daemon_tw_daemon_127,'\0',sizeof(init_param.daemon_tw_daemon_127));

	memset(init_param.daemon_realtime_pid,'\0',sizeof(init_param.daemon_realtime_pid));
	memset(init_param.daemon_seg_pid,'\0',sizeof(init_param.daemon_seg_pid));
	memset(init_param.daemon_tw_daemon_pid,'\0',sizeof(init_param.daemon_tw_daemon_pid));
	memset(init_param.daemon_tw_daemon_127_pid,'\0',sizeof(init_param.daemon_tw_daemon_127_pid));

	memset(init_param.conf_path,'\0',sizeof(init_param.conf_path));
	memset(init_param.conf_realtime,'\0',sizeof(init_param.conf_realtime));
	memset(init_param.conf_seg,'\0',sizeof(init_param.conf_seg));
	memset(init_param.conf_tw_daemon,'\0',sizeof(init_param.conf_tw_daemon));
	memset(init_param.conf_tw_daemon_127,'\0',sizeof(init_param.conf_tw_daemon_127));
	memset(init_param.conf_modbus_1,'\0',sizeof(init_param.conf_modbus_1));
	memset(init_param.conf_modbus_2,'\0',sizeof(init_param.conf_modbus_2));
	*/
	memset(&init_param,0,sizeof(init_param));

	/* 
		Read in all paths and parameters
	*/
	while(!feof(p_file)){
		memset(buf,'\0',sizeof(buf));
        if(fgets(p_file, "%[^% ]\n", buf) < 0) break;
        printf("line %d %s\n",i ,buf);
        if(line == 0 && strcmp(buf,"#path file version 0.1")){
        	printf("version is not supported\n");
        	return -1;
        }
        


    }




	return 0;
}