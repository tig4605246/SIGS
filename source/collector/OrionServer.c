//Name: Kevin & Oswin
//Date: April 20,2017
//Last updated: April 20,2017
//Prupose: Serve as a server and upload datas to the mongodb

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include <time.h>

#include <sys/syslog.h>

#include <curl/curl.h> //`pkg-config --cflags --libs ` or -lcurl
#include "../ipcs/SGSipcs.h"
#include "../controlling/SGScontrol.h"
#include "../thirdparty/cJSON.h" // cJSON.c -lm

#ifndef COLORS

#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"

#endif

#define MAXBUF 1024

#define OBD_STRING_LENGTH  64

#define MIN(x,y) (x < y ) ? x : y

//Where we store our logs

#define LOGPATH "./log"

void DisconnectAndQuit(int sigNum);

struct string{
    char *ptr;
    size_t len;
};

size_t response_handler(void *ptr, size_t size, size_t nmemb, struct string *s);



char *UserLogEventId[] = { 
							"LGTK" , "LMTK" , "LACN" , "LACF" , "LD2N" , "LD2F" , "LD3N" , "LD3F" , "LD4N" , "LD4F" ,
							"LD5N" , "LD5F" , "LD6N" , "LD6F" , "LDIS" , "LVML" , "LANG" , "LIDL" , "LMOV" , "LSIN" ,
							"LSUT" , "LGPS" , "LMPN" , "LMPF" , "LBAT" , "LGIN" , "LGUT" , "LUIN" , "LKIN" , "LGRK" ,
							"LGSK" , "LMSK" , "LIBT" , "LMPL" , "LMRK" , "LGEK" , "LMEK" , "LGSR" , "LRPN" , "LDTC" ,
							"LRPF" , "LCTN" , "LCTF" , "LHSA" , "LHSB" , "LHSC" , "LTLN" , "LTLF" , "LGTN" , "LCRH" ,
							"LTSK" , "LTTK" , "LTEK" , "LBTL" , "LCAL" , "LCAS" , "LCAZ" , "LCAF" , "LSYS" , "LOBD" ,
							"NULL" , "LHSN" , "LHSF"
							};

char *OBDInfo[] = {"DeviceID","MODEL","IMEI","Location","Heading","Speed","Odometer","HDOP","EngineLoad","EngineCoolantTemp",
                    "FuelLevel","IntakeAirTemp","EngineRPM","MassAirFlow","IntakeManifoldAbsolutePressure","MalfunctionIndicatorLamp",
                    "ThrottlePosition","VIN","FuelUsed","MainVoltage","SN",
                    "PendingCodeStatus","ReportID","ReportTime", NULL};


struct OBUData{
	
	char *deviceID;
	char *reportID;
    char *model;
	char *location;
	char *utcTime;
	char *VIN;
	char *pendingCodeStatus;
	char *IMEI;
	char *SN;

	float malfunctionIndicatorLamp;
	float speed;
	float hdop;
	float fuelLevel;
	float intakeAirTemp;
	float massAirFlow;
	float intakeManifoldAbsolutePressure;
	
	float throttlePosition;
	float fuelUsed;

	float mainVoltage;
	float odometer;
	float heading;

	
	float engineLoad;
	float engineCoolantTemp;
	float engineRPM;
	
} typedef OBUData;

//Puspose : Create log files
//Pre : buffer data
//Post : On success, return 0, otherwise return -1

int CreateLog(unsigned char *buf, int len);

int CreateJSONAndRunHTTPCommand(OBUData *data);		

//typedef struct OBUData OBUData;

int lenHelper(int x);

int OBDWriteToSgs(OBUData *OBUData);

char *getModel(unsigned char str[]);							
	
char *getSN(unsigned char str[]);
	
char *getIMEI(unsigned char str[]);

char *getLocation(unsigned char USER_LOG[]);

float getSpeed(unsigned char USER_LOG[]);

float getHeading(unsigned char USER_LOG[]);

float getOdometer(unsigned char USER_LOG[]);


char *getReportID(unsigned char USER_LOG[]);

char *getPendingCodeStatus(unsigned char USER_LOG[]);

char *getUTCDateTime(unsigned char USER_LOG[]);


char *getFuelLevel(unsigned char USER_LOG[]); 

///////////////////////////
float getHDOP(unsigned char USER_LOG[]);  


char *getIntakeAirTemp(unsigned char USER_LOG[]);  
char *getMassAirFlow(unsigned char USER_LOG[]);  

char *getIntakeManifoldAbsolutePressure(unsigned char USER_LOG[]); 
float getThrottlePosition(unsigned char OptionData[]); 
char *getVIN(unsigned char OptionData[]); 
float getMalfunctionIndicatorLamp(unsigned char USER_LOG[]);  
char *getFuelUsed(unsigned char USER_LOG[]);  


float getMainVoltage(unsigned char OptionData[]);

float getBattery(unsigned char OptionData[]);

float getEngineRPM(unsigned char OptionData[]);

float getEngineCoolantTemp(unsigned char OptionData[]);

float getEngineLoad(unsigned char OptionData[]); 





// OBUData.HDOP = 0.0;
	// OBUData.IntakeAirTemp = 0.0;
	// OBUData.MassAirFlow = 0.0;
	// OBUData.IntakeManifoldAbsolutePressure = 0.0;
	// OBUData.ThrottlePosition = 0.0;
	// OBUData.VIN = "VIN";
	// OBUData.MalfunctionIndicatorLamp = 0.0;
	// OBUData.FuelUsed = 0.0;
	
//////////////////////////
///////////

float getMainVoltage(unsigned char OptionData[]);

float getBattery(unsigned char OptionData[]);

float getEngineRPM(unsigned char OptionData[]);

float getEngineCoolantTemp(unsigned char OptionData[]);

float getEngineLoad(unsigned char OptionData[]); 

OBUData *parseData(OBUData *OBUData , unsigned char TCP_ReceiveData[] , int size);

int main(int argc, char *argv[])
{

		int pid = -1;
        int log_pid = -1;	
		int sockfd = -1, new_fd = -1;
		int i = 0;
		int ret = 0;
		socklen_t len;
		struct sockaddr_in my_addr, their_addr;
		unsigned int myport, lisnum;
		unsigned char buf[MAXBUF + 1];//Receive packet
		unsigned char packetAck[4];//return packet ACK
		struct sigaction act, oldact, oldact2;
		
		OBUData *OBUData = NULL;

		myport = 9001;
		
		lisnum = 30;

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
		{

				perror("socket");
				exit(EXIT_FAILURE);

		}
		

		act.sa_handler = (__sighandler_t)DisconnectAndQuit;
    	act.sa_flags = SA_ONESHOT|SA_NOMASK;
    	sigaction(SIGINT, &act, &oldact);

		//act.sa_handler = (__sighandler_t)DisconnectAndQuit;
    	//act.sa_flags = SA_NOCLDSTOP|SA_NOCLDWAIT;
    	//sigaction(SIGCHLD, &act, &oldact2);

		signal(SIGCHLD, SIG_IGN);
		
		bzero(&my_addr, sizeof(my_addr));
		my_addr.sin_family = AF_INET;
		my_addr.sin_port = htons(myport);


		my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

		if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr))== -1) 
		{

				perror("bind");
				exit(EXIT_FAILURE);

		}

		if (listen(sockfd,lisnum ) == -1) 
		{

				perror("listen");
				exit(EXIT_FAILURE);

		}

		printf("wait for connect\n");	
		len = sizeof(struct sockaddr);

		while(1)
		{

			if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr,&len)) == -1) 
			{

					perror("accept");
					exit(EXIT_FAILURE);

			} 
			else
			{

				printf("server: got connection from %s, port %d, socket %d\n",inet_ntoa(their_addr.sin_addr),ntohs(their_addr.sin_port), new_fd);

				if(-1==(pid=fork()))	
				{

					perror("fork");
					exit(EXIT_FAILURE);

				}

				if(pid == 0)
				{	

					close(sockfd);

					printf("PID == ZERO\n");
						
					//Receive Sync message from Orion

					bzero(buf, MAXBUF );//Clear buf
					len = recv(new_fd, buf, MAXBUF, 0);
					if(len < 0)
					{

						printf("fail to receive sync data from Orion\n");
						close(new_fd);
						exit(0);

					}
					else if(len == 0)
					{

						printf("Remote side closed , quitting\n");
						close(new_fd);
						exit(0);

					}

					//Return Sync message to Orion

					len = send(new_fd, buf, len , 0);

					while(1)
					{

						//Receive data packet

						bzero(buf, MAXBUF );//Clear buf
						len = recv(new_fd, buf, MAXBUF, 0);
						printf("len is %d before if\n",len);
						if(len > 0)
						{

							printf("message recv successful , %dByte recv\n", len);
														
							/* Here, We open a child process to help us create log files */
							#if 1

							log_pid = fork();

							if(log_pid < 0)
                            {

                                printf(LIGHT_RED"fork failed, %s\n"NONE,strerror(errno));

                            }
							printf("mypid is %d log_pid is %d\n",getpid(),log_pid);
                            if(log_pid == 0)
                            {

								printf("This is Child\n");
								
                                close(new_fd);
									
								ret = CreateLog(buf,len);

								if(ret != 0)
								{

									printf(LIGHT_RED"Create Log failed\n"NONE);

								}
								

                                exit(0);

                            }

							#endif
							printf("This is parent pid is %d\n",getpid());
							for(i = 0 ; i < len ; i++)
							{

								if(i%16==0)
									printf("\n");
								

								printf("%2x ",buf[i]);
								

							}
							printf("\n");
							OBUData = malloc(sizeof(struct OBUData));
							OBUData = parseData(OBUData , buf , len);
							ret = CreateJSONAndRunHTTPCommand(OBUData);

							printf("\n");

							//Return last 3 bytes of the data
							bzero(packetAck,sizeof(packetAck));
							for(i =  0 ; i < 3  ; i++)
							{

								packetAck[i] = *(buf + len - 3 + i);
								printf("%2x ",packetAck[i]);
								
							}
							printf("\n");
							len = send(new_fd, packetAck, sizeof(packetAck) - 1 , 0);
							printf("packetAck len[%lu] send data ACK %x %x %x\n",sizeof(packetAck),*(packetAck + len-3), *(packetAck + len-2), *(packetAck + len-1));
							if(len < 0)
							{

								printf("send data ACK failed, %s\n",strerror(errno));

							}
							else if(len == 0)
							{

								printf("Remote side closed , quitting\n");
								close(new_fd);
								exit(0);

							}

						}
						else if(len < 0)
						{

							printf("fail to receive sync data from Orion, %s\n",strerror(errno));
							close(new_fd);
							exit(0);

						}
						else if(len == 0)
						{

							printf("Remote side closed , quitting\n");
							close(new_fd);
							exit(0);

						}

					}



					//close and leave (Child)

					close(new_fd);
					exit(0);
					
				}
				close(new_fd);

			}

		}

		close(new_fd);
		close(sockfd);
		return 0;

}


void DisconnectAndQuit(int sigNum)
{

	printf("[pid : %d]The other side disconnected, leaving\n",getpid());
	//close(sockfd);
	exit(0);

}

OBUData *parseData(OBUData *z , unsigned char TCP_ReceiveData[] , int size){
	
	OBUData *a = NULL;
	a = malloc(sizeof(OBUData));
	//a->deviceID = malloc(MAX_deviceID_LENGTH*sizeof(char));
	a->deviceID = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->deviceID,'\0',sizeof(a->deviceID));
	
	a->location = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->location,'\0',sizeof(a->location));
	
	a->reportID = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->reportID,'\0',sizeof(a->reportID));
	
	a->pendingCodeStatus = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->pendingCodeStatus,'\0',sizeof(a->pendingCodeStatus));
	
	a->utcTime = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->utcTime,'\0',sizeof(a->utcTime));
	
	a->VIN = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->VIN,'\0',sizeof(a->VIN));

	a->model = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->model,'\0',sizeof(a->model));
	
	a->IMEI = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->IMEI,'\0',sizeof(a->IMEI));

	a->SN = (char *)malloc(sizeof(char) * (OBD_STRING_LENGTH));
	memset(a->SN,'\0',sizeof(a->SN));


	printf("parseData \n");
	
	unsigned char str[size];	
	unsigned char USER_LOGS[size];	
	unsigned char OptionData[size];
	unsigned char parmData[size];
	printf(" sizeof %d : \n\n" ,size);
	memset(parmData,'\0',sizeof(parmData));
	
	//memcpy(USER_LOGS, TCP_ReceiveData+8, sizeof(TCP_ReceiveData) - 3 - 8 + 1); 
	printf("TCP_ReceiveData sizeof %d : \n\n" ,sizeof(TCP_ReceiveData)); 
	memcpy(str, TCP_ReceiveData + 4, 10);
	memcpy(USER_LOGS, TCP_ReceiveData + 8 , size - 8);		
	memcpy(OptionData, USER_LOGS + 36 ,size - 36 );	
	
	memcpy(parmData, OptionData ,OptionData[1]);	
	printf("parmData sizeof %d : \n\n" ,sizeof(parmData)); 
	char *parm;
	float num;
	//s = (char *)malloc(sizeof(char) * 127);
	
	int index = 0;	
	//index += OptionData[1];
	int cutpoint = size - 3 - index;
	
	while(sizeof(parmData) != 0){
		printf("\n--parmData-- %x : \n" ,(parmData[0])); 
		 switch (parmData[0]) {
			 
			case 2: // OPT-RPM-TEMP
            case 130:					     
					
				printf("getEngineRPM:%f\n",getEngineRPM(parmData));
				printf("getEngineCoolantTemp:%f\n",getEngineCoolantTemp(parmData));
				num = getEngineRPM(parmData);	
  				a->engineRPM = num;
				num = getEngineCoolantTemp(parmData);	
  				a->engineCoolantTemp = num;
            break;
			 
			case 3: // OPT-VOLTAGE
			case 131:
				printf("getBattery:%f\n",getBattery(parmData));
				printf("getMainVoltage:%f\n",getMainVoltage(parmData));
				//num = getMainVoltage(parmData);	
  				//a->mainVoltage = num;
				//num = getBattery(parmData);	
  				//a->batt = num;
			break;	
			case 12: // OPT-ENGINE-LOAD
			case 140:
				
				// 2016-11-23 for FET
				// ENGINE_LOAD_DATA = Common.ByteToHexString(OptionData[2]);
				printf("getEngineLoad:%f\n",getEngineLoad(parmData));
				num = getEngineLoad(parmData);	
  				a->engineLoad = num;
			break;
						
			
			case 19: // OPT-OBD-VIN
			case 147:
				// 2016-11-23 for FET
				// ENGINE_LOAD_DATA = Common.ByteToHexString(OptionData[2]);
				printf("\n*-------Find VIN--------*\n");				
				printf("getVIN:%s\n",getVIN(parmData));
				parm = getVIN(parmData);			
				strncpy(a->VIN , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
				a->VIN[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';	
			break;
			
			case 20: // (OPT-OBD-TP-MIL)	
				// 2016-11-23 for FET
				// ENGINE_LOAD_DATA = Common.ByteToHexString(OptionData[2]);
				printf("getThrottlePosition:%f\n",getThrottlePosition(parmData));
			break;
			
			default:
			
				printf("\ncan't find match case !\nOptionData[0] : 0x%x\n\n",parmData[0]);
			break;
		 }
		 //OptionData = Common.SubByteArray(OptionData, System.Convert.ToInt16(OptionData[1]));
		 if(parmData[1] == 0x00)		
		 {		
			printf("BREAK; OptionData[0] : %x\n" ,parmData[0]); 
			break;
		 }
		 
		 printf("\n\nmove OptionData[1] : %x\n" ,parmData[1]); 
		 //printf("index : %d\n" ,index); 
		 if(index >= size - 44 - 3)
		 {
			 printf("\n\n break index : %d\n" ,index); 
			 break;
		 }

		 //printf("OptionData sizeof %d : \n\n" ,sizeof(OptionData)); 		 

		 //memcpy(parmData, OptionData + index ,  parmData[1]);	
		 memcpy(parmData, OptionData + index ,  OptionData[ index + 1 ]);	
		  printf("copy len is  : %x\n" ,OptionData[ index + 1 ]);
		 index = index + parmData[1];
		
	}
		//printf("parm %s %d",parm , strlen(parm));
	parm = getIMEI(str);			
	strncpy(a->deviceID , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->deviceID[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';	
	
	parm = getLocation(USER_LOGS);		
	strncpy(a->location , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->location[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';
	
	parm = getReportID(USER_LOGS);		
	strncpy(a->reportID , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->reportID[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';
	
	parm = getUTCDateTime(USER_LOGS);		
	strncpy(a->utcTime , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->utcTime[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';
	
	parm = getModel(USER_LOGS);		
	strncpy(a->model , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->model[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';

	parm = getIMEI(str);		
	strncpy(a->IMEI , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->IMEI[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';

	parm = getPendingCodeStatus(str);		
	strncpy(a->pendingCodeStatus , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->pendingCodeStatus[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';

	parm = getIMEI(str);		
	strncpy(a->SN , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->SN[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';

	/*parm = getVIN(OptionData);		
	strncpy(a->VIN , parm , MIN(strlen(parm) , OBD_STRING_LENGTH));
    a->VIN[MIN(strlen(parm) , OBD_STRING_LENGTH)] = '\0';*/
	
	num = getSpeed(USER_LOGS);			
    a->speed = num;
	
	num = getHeading(USER_LOGS);			
    a->heading = num;
	
	num = getOdometer(USER_LOGS);			
    a->odometer = num;
	
	num = getEngineCoolantTemp(OptionData);
	a->engineCoolantTemp = num;
	
	printf("ddddgetLocation:%s\n",getLocation(USER_LOGS));
	printf("getSpeed:%lf\n",getSpeed(USER_LOGS));
	printf("getHeading:%f\n",getHeading(USER_LOGS));
	printf("getOdometer:%f\n",getOdometer(USER_LOGS));	
	printf("getReportID:%s\n",getReportID(USER_LOGS));
	printf("getUTCDateTime:%s\n",getUTCDateTime(USER_LOGS));
	////////////////////////
	
	printf("parseData part 2 \n");
	
	
	
	//OBUData->mainVoltage = 20.0;
	
	//printf("OBUData %f\n",OBUData->mainVoltage);
	
	printf("TCP_ReceiveData [0] : %x\n", TCP_ReceiveData[0]);
	
	
	printf("*------OBUData--------*\n");
	printf("deviceID %s\n",a->deviceID);
	printf("VIN %s\n",a->VIN);
	//printf("IMEI %s\n",a->IMEI);
	//printf("SN %s\n",a->SN);
	printf("location %s\n",a->location);
	printf("reportID %s\n",a->reportID);
	
	printf("mainVoltage %f\n",a->mainVoltage);
	printf("engineCoolantTemp %f\n",a->engineCoolantTemp);
	//printf("HDOP %f\n",a->hdop);
	//printf("FuelLevel %f\n",a->fuelLevel);
	//printf("IntakeAirTemp %f\n",a->intakeAirTemp);
	//printf("MassAirFlow %f\n",a->massAirFlow);
	//printf("intakeManifoldAbsolutePressure %f\n",a->intakeManifoldAbsolutePressure);
	//printf("ThrottlePosition %f\n",a->throttlePosition);
	//printf("MalfunctionIndicatorLamp %f\n",a->malfunctionIndicatorLamp);
	//printf("FuelUsed %f\n",a->fuelUsed);
	//printf("PendingCodeStatus %f\n",a->pendingCodeStatus);
	
	printf("odometer %f\n",a->odometer);
	printf("heading %f\n",a->heading);
	printf("engineLoad %f\n",a->engineLoad);
	printf("speed %f\n",a->speed);
	printf("UTCDateTime:%s\n",a->utcTime);
	printf("*-------------------*\n");
	printf("z5zz\n");
	
	return a;
}


char *getIMEI(unsigned char USER_LOG[]){
	
	int IMEI = 0;
	int val;
	char *result = malloc(64);
	IMEI += USER_LOG[0] * pow (256, 0) + USER_LOG[1] * pow (256, 1) + USER_LOG[2] * pow (256, 2) + USER_LOG[3] * pow (256, 3);
	
	snprintf( result, lenHelper(IMEI) +1, "%d", IMEI );  

	return result;
}


int lenHelper(int x) {
    if(x>=1000000000) return 10;
    if(x>=100000000) return 9;
    if(x>=10000000) return 8;
    if(x>=1000000) return 7;
    if(x>=100000) return 6;
    if(x>=10000) return 5;
    if(x>=1000) return 4;
    if(x>=100) return 3;
    if(x>=10) return 2;
    return 1;
}


char *getLocation(unsigned char USER_LOG[]){
	
	
	
	int IMEI = 0;
	int val;
	double val_double;
	double lat_tiny, lng_tiny;
	char *result = malloc(64);
	long int X, Y;
	double lat, lng;	
	
	// 2010-08-15 處理經緯度正負號問題
	//Lat
	X = USER_LOG[4] * pow(256, 0) + USER_LOG[5] * pow(256, 1) + USER_LOG[6] * pow(256, 2) + USER_LOG[7] * pow(256, 3);
	X = (X > 2147483647 ? (4294967295 - X) * -1 : X) ;
	
	lat= X/1000000; // no divion '.0'		// get degree part
	val = (X%1000000);
	val_double = val;
	lat_tiny = (val_double/60.0)/10000.0;	// transfer minutes and seconds to degree 0.xxxx
  	lat += lat_tiny;			// combine two part


	//Lng
	Y = USER_LOG[8] * pow(256, 0) + USER_LOG[9] * pow(256, 1) + USER_LOG[10] * pow(256, 2) + USER_LOG[11] * pow(256, 3);
	Y = (Y > 2147483647 ? (4294967295 - Y) * -1 : Y) ;	
	

	lng= Y/1000000;		// get degree part
	val = (Y%1000000);
	val_double = val;
	lng_tiny = (val_double/60.0)/10000.0;	// transfer minutes and seconds to degree 0.xxxx
  	lng += lng_tiny;			// combine two part
	
	printf("Get Location\n");
	printf("lat X : %lf\n",lat);
	printf("lng Y : %lf\n",lng);

	// OBUData.Latitude = System.Convert.ToDouble(Data[4]) / 100 + ((System.Convert.ToDouble(Data[4]) % 100) / 60);
    // OBUData.Longitude = System.Convert.ToDouble(Data[6]) / 100 + ((System.Convert.ToDouble(Data[6]) % 100) / 60);
    //lng = Y  + lat_tiny;
	//lat = X  + lng_tiny;
	
	snprintf( result, 20, "%lf:%lf", lat , lng );  	
	//snprintf( result, 20, "%lf:%lf", X/100 , Y/100 );  	
	return result;
}


float getSpeed(unsigned char USER_LOG[]){
	
	float speed;	
	//System.Convert.ToString((USER_LOG[16] * Math.Pow(256, 0) + USER_LOG[17] * Math.Pow(256, 1)) / 10.0);	// Speed
	speed = (USER_LOG[16] * pow(256, 0) + (USER_LOG[17] * pow(256, 1)) ) / 10.0;	
	return speed;
}

char *getUTCDateTime(unsigned char USER_LOG[]){
	
	char *strtime;
	//Result[26] = USER_LOG[18].ToString("00") + USER_LOG[19].ToString("00") + USER_LOG[20].ToString("00") + USER_LOG[21].ToString("00") + USER_LOG[22].ToString("00") + USER_LOG[23].ToString("00");	// UTCDateTime 	
	// 'yyyy-MM-dd HH:mm:ss'
	snprintf( strtime, 19 , "20%d-%d-%d %d:%d:%d", USER_LOG[18] , USER_LOG[19], USER_LOG[20] , USER_LOG[21] , USER_LOG[22] , USER_LOG[23]  );  
	//sprintf(strtime ,  "%d-%d-%d %d:%d:%d", USER_LOG[18] , USER_LOG[19] , USER_LOG[20] , USER_LOG[21] , USER_LOG[22] , USER_LOG[23]  );  
	printf("%s\n",strtime);
	return strtime;
}


float getMainVoltage(unsigned char OptionData[]){
	
	float MainVoltage;	
	// MainPower = System.Convert.ToString((OptionData[2] * Math.Pow(256, 0) + OptionData[3] * Math.Pow(256, 1)) / 10);	// MainPower
    // Battery = System.Convert.ToString((OptionData[4] * Math.Pow(256, 0) + OptionData[5] * Math.Pow(256, 1)) / 100);	// Battery                                                 
	
	MainVoltage = (OptionData[2] * pow(256, 0) + OptionData[3] * pow(256, 1)) / 10.0;	
	
	return MainVoltage;
}

float getBattery(unsigned char OptionData[]){
	
	float Battery;		
    // Battery = System.Convert.ToString((OptionData[4] * Math.Pow(256, 0) + OptionData[5] * Math.Pow(256, 1)) / 100);	// Battery                                                 	
	Battery = (OptionData[4] * pow(256, 0) + OptionData[5] * pow(256, 1)) / 100.0;	
	
	return Battery;
}

float getEngineRPM(unsigned char OptionData[]){
	
	float EngineRPM;		
    // RPM = System.Convert.ToString(OptionData[2] * Math.Pow(256, 0) + OptionData[3] * Math.Pow(256, 1));	    	
	EngineRPM = (OptionData[2] * pow(256, 0) + OptionData[3] * pow(256, 1));	
	
	return EngineRPM;
}

float getEngineCoolantTemp(unsigned char OptionData[]){
	
	float EngineCoolantTemp;		
	// RadiatorTemperature = System.Convert.ToString((int)OptionData[4] - 40);       	
	EngineCoolantTemp =  (OptionData[4]) - 40;	
	
	return EngineCoolantTemp;
}

float getEngineLoad(unsigned char OptionData[]){
	
	float EngineLoad;		
	// ENGINE_LOAD_DATA = System.Convert.ToInt16(OptionData[2]).ToString();     	
	EngineLoad =  OptionData[2];	
	
	return EngineLoad;
}

float getHeading(unsigned char USER_LOG[]){
	
	float Heading;		
	// System.Convert.ToString(USER_LOG[14] * Math.Pow(256, 0) + USER_LOG[15] * Math.Pow(256, 1));	// Direction    	
	Heading =  USER_LOG[14] * pow(256, 0) + (USER_LOG[15] * pow(256, 1)) ;		
	
	return Heading;
}

float getOdometer(unsigned char USER_LOG[]){
	
	float Odometer;		
	// Result[22] = System.Convert.ToString(USER_LOG[31] * Math.Pow(256, 0) + USER_LOG[32] * Math.Pow(256, 1) + USER_LOG[33] * Math.Pow(256, 2) + USER_LOG[34] * Math.Pow(256, 3));	// Mileage
	Odometer =  USER_LOG[31] * pow(256, 0) + (USER_LOG[32] * pow(256, 1)) + (USER_LOG[33] * pow(256, 2)) + (USER_LOG[34] * pow(256, 3));		
	
	return Odometer;
}

char *getReportID(unsigned char USER_LOG[]){
		
	//Result[17] += UserLogEventId[(int)USER_LOG[0]];	// 原 DataType，這裡 UserLogEventId
	//printf("USER_LOG[0]:%d\n",USER_LOG[0]);		
	//snprintf( ReportID, sizeof( ReportID ), "%s", UserLogEventId[USER_LOG[0]] );  
printf("sfdsf\n");	
	char *result = malloc((char *)malloc(strlen(UserLogEventId[USER_LOG[0]])+1));	
printf("sccc\n");	
    strcpy(result , UserLogEventId[USER_LOG[0]]);
	
	return result;
}

char *getPendingCodeStatus(unsigned char USER_LOG[]){
		
	//  Result[14] += "0";	// LDTC Error Code 

	
	return "0";
}

char *getFuelLevel(unsigned char USER_LOG[]){
		
	//   Result[25] = "0";	// Fuel

	
	return "0";
}

char *getModel(unsigned char str[]){
	
	
	return "BD3112";
}							
	
char *getSN(unsigned char str[]){
	
	return "???";
	
} 

float getHDOP(unsigned char USER_LOG[]){
	
	return 0.0;
	
} 
char *getIntakeAirTemp(unsigned char USER_LOG[]){
	
	return "0.0";
	
}   
char *getMassAirFlow(unsigned char USER_LOG[]){
	
	return "0.0";
	
}   

char *getIntakeManifoldAbsolutePressure(unsigned char USER_LOG[]){
	
	return "0.0";
	
}  
float getThrottlePosition(unsigned char OptionData[]){
	
	float ThrottlePosition ; 
	ThrottlePosition = OptionData[2];
	
	
	return ThrottlePosition;
	
} 
char *getVIN(unsigned char OptionData[]){

	char *VIN =  malloc(64);

	snprintf( VIN, 18 , "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\0", OptionData[3] , OptionData[4] , OptionData[5] , OptionData[6] , OptionData[7] , OptionData[8] ,
							    	OptionData[9] , OptionData[10] , OptionData[11] , OptionData[12] , OptionData[13] , OptionData[14] ,
							    	OptionData[15] , OptionData[16] , OptionData[17] , OptionData[18] , OptionData[19] );

	return VIN;
	
} 
float getMalfunctionIndicatorLamp(unsigned char USER_LOG[]){
	
	return 0.0;
	
}   
char *getFuelUsed(unsigned char USER_LOG[]){
	
	return "0.0";
	
}  

int CreateJSONAndRunHTTPCommand(OBUData *data)
{
    
    int i = 0;
    //struct timeb tp;
    DATETIME n_time;
	
    int irr_value = 0;
    int temp_value = 0;
    int ret=0;
    char *output = NULL;
    char *format = NULL;
    cJSON *root; 
    cJSON *fmt;
    cJSON *array;
    CURL *curl;
	char *errstr;
	char Content_Length[32];
	struct curl_slist *chunk = NULL;
    struct string s;
	
    //dataInfo *head = dataInfoPtr;
    //dataInfo *head = interface;
    //dataInfo *temp ;
    //dataLog dest;
    //dataLog datalog;

    int j;
    printf( "[%s:%d] CreateJSONAndRunHTTPCommand called", __FUNCTION__, __LINE__);
    printf("\n");
    

    
    //ftime(&tp);
    //n_time = tp.time*1000 + tp.millitm;
    // Initialize curl headers
    syslog(LOG_INFO, "[%s:%d] Initialize curl headers ", __FUNCTION__, __LINE__);
    chunk = curl_slist_append(chunk, "Accept: text/plain");
	chunk = curl_slist_append(chunk, "Accept-Encoding: gzip, deflate");
    chunk = curl_slist_append(chunk, "application/json; charset=UTF-8");
	chunk = curl_slist_append(chunk, "Content_Length");
	chunk = curl_slist_append(chunk, "User-Agent: Kelier/0.1");

    // Initialize cJSON

    ///*   
        //---Stuff cJSON at here---
    
    j=0;
    curl_global_init(CURL_GLOBAL_ALL);

 
    curl = curl_easy_init();
   
   // while(DeviceName[j] != NULL){
        init_string(&s);
    //while(head != NULL){      
    //while(DeviceName[j] != NULL){
        //head = interface;
        root = cJSON_CreateObject();
		 cJSON_AddStringToObject(root, "DeviceId", data->deviceID);
		 cJSON_AddStringToObject(root, "UnitID", data->deviceID);           
		 cJSON_AddStringToObject(root, "Model", data->model); 
		 cJSON_AddStringToObject(root, "IMEI", data->IMEI);
		 cJSON_AddStringToObject(root, "Location", data->location);
		 cJSON_AddNumberToObject(root, "Heading", data->heading);
		 cJSON_AddNumberToObject(root, "Speed", data->speed);
		 cJSON_AddNumberToObject(root, "Odometer", data->odometer);
		 cJSON_AddNumberToObject(root, "EngineCoolantTemp", data->engineCoolantTemp);
		 cJSON_AddNumberToObject(root, "EngineLoad", data->engineLoad);
		 cJSON_AddNumberToObject(root, "FuelLevel", data->fuelLevel);
		 cJSON_AddNumberToObject(root, "IntakeAirTemp", data->intakeAirTemp);
		 cJSON_AddNumberToObject(root, "MassAirFlow", data->massAirFlow);
		 cJSON_AddNumberToObject(root, "FuelUsed", data->fuelUsed);
		 cJSON_AddNumberToObject(root, "EngineRPM", data->engineRPM);
		 cJSON_AddNumberToObject(root, "ThrottlePosition", data->throttlePosition);
		 cJSON_AddNumberToObject(root, "MalfunctionIndicatorLamp", data->malfunctionIndicatorLamp);
		 cJSON_AddNumberToObject(root, "IntakeManifoldAbsolutePressure", data->intakeManifoldAbsolutePressure);
		 cJSON_AddStringToObject(root, "SN", data->SN);
		 cJSON_AddNumberToObject(root, "HDOP", data->hdop);
		 cJSON_AddNumberToObject(root, "MainVoltage", data->mainVoltage);
		 cJSON_AddStringToObject(root, "VIN", data->VIN);		 
		 cJSON_AddStringToObject(root, "ReportTime", data->utcTime);
		 cJSON_AddStringToObject(root, "ReportID", data->reportID);
		 cJSON_AddStringToObject(root, "PendingCodeStatus", data->pendingCodeStatus);
  		 cJSON_AddStringToObject(root, "PlateNumber", "0");
         cJSON_AddStringToObject(root, "ProductModel", "0");  
         cJSON_AddStringToObject(root, "EventCode", "0");
        /*if(head == NULL)
        {
             printf("head is NULL \n");
        }*/
/*
        while(head != NULL){
            ret = sgsReadSharedMemory(head,&dest);

            if(ret != 0){
                printf("Read %s shm value failed\n",head->valueName);
                printf("\n");        
            }           
            if(!strcmp(head->sensorName,DeviceName[j])){    
                printf("*------------Get it!--------*\n");
                printf("*--head->valueName %s *\n",head->valueName);
                if(!strcmp(head->valueName, "DeviceID")){
                    printf("*match DeviceID*\n");
                    printf("*dest.value.s %s*\n",dest.value.s);
                    cJSON_AddStringToObject(root, "DeviceId", dest.value.s);
                    cJSON_AddStringToObject(root, "UnitID", dest.value.s);                
                }              

                else if(!strcmp(head->valueName, "MODEL")){
                    cJSON_AddStringToObject(root, "Model", dest.value.s);
                }
                else if(!strcmp(head->valueName, "IMEI")){
                    cJSON_AddStringToObject(root, "IMEI", dest.value.s);
                }
                else if(!strcmp(head->valueName, "Location")){
                    cJSON_AddStringToObject(root, "Location", dest.value.s);
                }
                else if(!strcmp(head->valueName, "Heading")){
                    cJSON_AddNumberToObject(root, "Heading", dest.value.f);
                }
                else if(!strcmp(head->valueName, "Speed")){
                    cJSON_AddNumberToObject(root, "Speed", dest.value.f);
                }
                else if(!strcmp(head->valueName, "Odometer")){
                    cJSON_AddNumberToObject(root, "Odometer", dest.value.f);
                }           
                else if(!strcmp(head->valueName, "EngineLoad")){
                    cJSON_AddNumberToObject(root, "EngineLoad", dest.value.f);
                }
                else if(!strcmp(head->valueName, "EngineCoolantTemp")){
                    cJSON_AddNumberToObject(root, "EngineCoolantTemp", dest.value.f);
                }
                else if(!strcmp(head->valueName, "FuelLevel")){
                    cJSON_AddNumberToObject(root, "FuelLevel", dest.value.f);
                }
                else if(!strcmp(head->valueName, "IntakeAirTemp")){
                    cJSON_AddNumberToObject(root, "IntakeAirTemp", dest.value.f);
                }
                else if(!strcmp(head->valueName, "EngineRPM")){
                    cJSON_AddNumberToObject(root, "EngineRPM", dest.value.f);
                }
                else if(!strcmp(head->valueName, "FuelUsed")){
                    cJSON_AddNumberToObject(root, "FuelUsed", dest.value.f);
                }                
                else if(!strcmp(head->valueName, "MassAirFlow")){
                    cJSON_AddNumberToObject(root, "MassAirFlow", dest.value.f);
                }
                else if(!strcmp(head->valueName, "IntakeManifoldAbsolutePressure")){
                    cJSON_AddNumberToObject(root, "IntakeManifoldAbsolutePressure", dest.value.f);
                }
                else if(!strcmp(head->valueName, "MalfunctionIndicatorLamp")){
                    cJSON_AddNumberToObject(root, "MalfunctionIndicatorLamp", dest.value.f);
                }
                else if(!strcmp(head->valueName, "ThrottlePosition")){
                    cJSON_AddNumberToObject(root, "ThrottlePosition", dest.value.f);
                }
                else if(!strcmp(head->valueName, "VIN")){
                    cJSON_AddStringToObject(root, "VIN", dest.value.s);
                }
                else if(!strcmp(head->valueName, "MainVoltage")){
                    cJSON_AddNumberToObject(root, "MainVoltage", dest.value.f);
                } 
                else if(!strcmp(head->valueName, "HDOP")){
                    cJSON_AddNumberToObject(root, "HDOP", dest.value.f);
                }             
                else if(!strcmp(head->valueName, "SN")){
                    cJSON_AddStringToObject(root, "SN", dest.value.s);
                }
                else if(!strcmp(head->valueName, "PendingCodeStatus")){
                    cJSON_AddStringToObject(root, "PendingCodeStatus", dest.value.s);
                }
                else if(!strcmp(head->valueName, "ReportID")){
                    cJSON_AddStringToObject(root, "ReportID", dest.value.s);
                }
                else if(!strcmp(head->valueName, "ReportTime")){
                    cJSON_AddStringToObject(root, "ReportTime", dest.value.s);
                }                              
                else
                    printf("Error value name %s!\n",head->valueName);
                ;     

                  
                

                if(!strcmp(dest.value.s, "Value is not ready yet")){
                printf("dest.value.s : \n%s+\n",dest.value.s);
                printf("HTTP Value is not ready yet\n");
                cJSON_Delete(root); 
                root = cJSON_CreateObject();

                printf("Value is not ready yet %s\n", DeviceName[j]);
                output = cJSON_PrintUnformatted(root);

                format = cJSON_Print(root);
         
                break;
                //return -1;
                }
                // Value of Sensor Type Ready and Do HTTP                
            }            
           
            head = head->next;
        }
*/
        ///*------*/------
        //----------------------------------
        // We check the JSON data at here
     

        printf("cJSON_PrintUnformatted \n");
        output = cJSON_PrintUnformatted(root);

        format = cJSON_Print(root);
        
        // Print the JSON data 
        printf("-****-output-*****- \n%s\n\n",output);   

        if(root == NULL){
           printf("--------------root NULL--------------\n"); 
        }
        printf("--------------output > len--------------\n");         
        if(strlen(output) > 40){
            /*temp = findTag(interface, "DeviceID" , DeviceName[j]);
            if(temp != NULL){                             
                datalog.valueType = STRING_VALUE;
                strncpy(datalog.value.s , "Value is not ready yet" , MIN(strlen("Value is not ready yet") , OBD_STRING_LENGTH));
                datalog.value.s[MIN(strlen("Value is not ready yet") , OBD_STRING_LENGTH)] = '\0';
                ret = sgsWriteSharedMemory(temp,&datalog);                  
                if(ret){
                    printf("write fail! ret %d\n",ret);
                }
                else
                {
                    printf("write DeviceID %s value to Value is not ready yet \n",DeviceName[j]);
                }
			}*/
            
            //----------------------------------
            // Adding curl http options

            sprintf(Content_Length,"Content-Length: %o",50);
            printf("%s\n",Content_Length);
 
            
           
            if(curl) 
            {
            printf("init\n");
            //ret = curl_easy_setopt(curl, CURLOPT_URL, "http://140.118.121.61:8000/car_fet");
            ret = curl_easy_setopt(curl, CURLOPT_URL, "http://140.118.121.61:8000/car_fet");
            //ret = curl_easy_setopt(curl, CURLOPT_URL, "http://140.118.70.136:9001/car_fet");
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

           
            
            
            // Clean JSON pointer

            cJSON_Delete(root); 
            

            free(format);
            free(output);
            free(s.ptr);
            
            printf(  "[%s:%d] Finished\n", __FUNCTION__, __LINE__);
            ///*------*/------
        }      
            
        j++;
 //   }    
    
 
    curl_global_cleanup();
     curl_easy_cleanup(curl);
    printf("return ret %d\n*-------------------*\n", ret);
    return ret;
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

int CreateLog(unsigned char *buf, int len)
{

	struct stat sb;
	time_t nowTime;
	struct tm nowTm;
	char timeString[128];
	char logFilePath[128];
	FILE *fp = NULL;
	int i = 0;

	if(stat(LOGPATH, &sb) != 0) 
    	mkdir(LOGPATH, 0755);
	
	if(buf == NULL)
	{
		printf(LIGHT_RED"[%s,%d] buf is empty\n"NONE,__FUNCTION__,__LINE__);
		return -1;
	}

	time(&nowTime);
	nowTm = *localtime(&nowTime);

	memset(timeString,'\0',sizeof(timeString));
	//printf("[%s,%d] memset done\n",__FUNCTION__,__LINE__);
	strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &(nowTm));
	//printf("[%s,%d] strftime done\n",__FUNCTION__,__LINE__);
	printf(YELLOW"\t\t\tupdated time : %s\n"NONE,timeString);

	memset(logFilePath,'\0',sizeof(logFilePath));

	snprintf(logFilePath,sizeof(logFilePath),"%s/%s",LOGPATH,timeString);
	printf(YELLOW"\t\t\tlogFilePath %s\n"NONE,logFilePath);

	fp = fopen(logFilePath,"w");

	if(fp == NULL)
	{
		printf(LIGHT_RED"[%s,%d] Open %s failed\n"NONE,__FUNCTION__,__LINE__,logFilePath);
		return -1;
	}


	for(i = 0 ; i < len ; i++)
	{

		if(i%16==0)
			fprintf(fp,"\n");
		

		fprintf(fp,"%2x ",buf[i]);
		

	}
	fclose(fp);
	return 0;


}