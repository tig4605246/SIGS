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

#include "../ipcs/SGSipcs.h"
#include "../controlling/SGScontrol.h"
#include "../thirdparty/cJSON.h" // cJSON.c -lm

#define MAXBUF 1024

#define OBD_STRING_LENGTH  64

#define MIN(x,y) (x < y ) ? x : y

deviceInfo *deviceInfoPtr = NULL;

dataInfo *dataInfoPtr = NULL;

dataInfo *interface = NULL;


//int shmID = 0;//test only

//Intent : close and quit when catching SIGUSR2 signal (will call releaseResource() too)
//Pre : sigNum
//Post : Nothing

void DisconnectAndQuit(int sigNum);

//Intent : free deviceInfoPtr, dataInforPtr and free the shared memory (get pointers from global parameters)
//Pre : Nothing
//Post : On success, return 0. On error, return -1 and shows the error messages

void releaseResource();

static int initializeInfo();

dataInfo* findTag(dataInfo * temp , char *valueName , char *gwid);


//

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

//

int main(int argc, char *argv[])
{

		int pid = -1;	
		int sockfd = -1, new_fd = -1;
		int i = 0;
		int loop = 0;//child process stopping criteria
		int ret = 0;
		socklen_t len;
		struct sockaddr_in my_addr, their_addr;
		unsigned int myport, lisnum;
		unsigned char buf[MAXBUF + 1];//Receive packet
		unsigned char packetAck[4];//return packet ACK
		struct sigaction act, oldact;

		deviceInfo *target = NULL;
		deviceInfo *temp = NULL;

		dataLog datalog;

		OBUData *OBUData = NULL;		

		myport = 9001;
		
		lisnum = 5;

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
		{

				perror("socket");
				exit(EXIT_FAILURE);

		}
		

		act.sa_handler = (__sighandler_t)DisconnectAndQuit;
    	act.sa_flags = SA_ONESHOT|SA_NOMASK;
    	sigaction(SIGUSR2, &act, &oldact);

		ret = sgsInitControl("OrionServer");
		if(ret < 0)
		{

			printf("OrionServer aborting\n");
			return -1;

		}

		ret = initializeInfo();
		if(ret < 0)
		{

			printf("OrionServer aborting\n");
			return -1;

		}

		//Find where cpm70_agent stores the data

		target = deviceInfoPtr;

		while(strcmp(target->deviceName,"Orion_mongo") && target != NULL)
		{

			target = target->next;

		}
		
		if(target == NULL)
		{

			printf("No target, nothing to do at here, bye~\n");
			exit(0);

		}
		else
		{

			printf("Orion Server target->deviceName is %s\n",target->deviceName);
			sgsShowAll(target);
			printf("show done\n");
			

		}
		//printf("123W\n\n\n\n\n\n\n\n\n\n\n\n");
	

		interface = target->dataInfoPtr;

		if(interface == NULL)
		{
			printf("interface is NULL\n");
			releaseResource();
		}

		while(strcmp(interface->deviceName,"Orion_mongo") && interface != NULL)
		{
			//printf("cd\n");
			interface = interface->next;
			//printf("c\n");

		}
		printf("What the fuck ???\n");
		if(interface == NULL)
		{

			printf("Warning : Orion_mongo is not defined in device.conf\n");
			

		}
		else
		{

			printf("interface->deviceName is %s\n",interface->deviceName);				
			dataInfoPtr = interface;

		}

		printf("Nothing bad here\n");
		bzero(&my_addr, sizeof(my_addr));
		my_addr.sin_family = AF_INET;
		my_addr.sin_port = htons(myport);


		my_addr.sin_addr.s_addr = inet_addr("140.118.121.63");

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
							
							/* */
							
							
							OBUData = malloc(sizeof(struct OBUData));
							OBUData = parseData(OBUData , buf , sizeof(buf));
							printf("OBUData deviceID %s\n",OBUData->deviceID);
							OBDWriteToSgs(OBUData);


							for(i = 0 ; i < len ; i++)
							{

								if(i%16==0)
									printf("\n");
								

								printf("%2x ",buf[i]);
								

							}
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
	releaseResource();
	//close(new_fd);
	exit(0);

}

static int initializeInfo()
{

    int ret = 0;
    ret = sgsInitDeviceInfo(&deviceInfoPtr);
	printf("\n\n\ninitializeInfo\n\n\n");
    if(ret != 0)
    {

        printf("[%s,%d] init device conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    } 



    ret = sgsInitDataInfo(deviceInfoPtr, &dataInfoPtr, 0);//Change to 0 if you want to use SGSmaster 
	


    if(ret == 0) 
    {

        printf("[%s,%d] init data conf failed ret = %d\n",__FUNCTION__,__LINE__,ret);
        return -1;

    }

    

    return 0;

}

void releaseResource()
{

    sgsDeleteAll(deviceInfoPtr,-1);

    return ;

}

int OBDWriteToSgs(OBUData *input){
	
	OBUData *OBUData = input;
	deviceInfo *temp = NULL;
	dataLog datalog;
	int ret;
	int j = 0 ;
	printf("OBDWriteToSgs\n");
	while(OBDInfo[j] != NULL){
		
		printf("OBDInfo[%d] %s\n",j ,OBDInfo[j]);
		printf("OBUData->deviceID %s\n",OBUData->deviceID);
		memset(datalog.value.s,'\0',sizeof(datalog.value.s));           
		temp = findTag(dataInfoPtr, OBDInfo[j] , OBUData->deviceID);
		if(temp != NULL){                             
			datalog.valueType = STRING_VALUE;
			if(!strcmp(OBDInfo[j], "DeviceID")){
			strncpy(datalog.value.s,OBUData->deviceID , MIN(strlen(OBUData->deviceID) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->deviceID) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "MODEL")){    
			strncpy(datalog.value.s,OBUData->model,MIN(strlen(OBUData->model) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->model) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "IMEI")){    
			strncpy(datalog.value.s,OBUData->IMEI,MIN(strlen(OBUData->IMEI) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->IMEI) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "Location")){    
			strncpy(datalog.value.s,OBUData->location,MIN(strlen(OBUData->location) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->location) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "Heading")){
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->heading;
			}
			else if(!strcmp(OBDInfo[j], "Speed")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->speed;      			    
			}
			else if(!strcmp(OBDInfo[j], "Odometer")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->odometer;
			}
			else if(!strcmp(OBDInfo[j], "HDOP")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->hdop;
			}
			else if(!strcmp(OBDInfo[j], "EngineLoad")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->engineLoad;
			}
			else if(!strcmp(OBDInfo[j], "EngineCoolantTemp")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->engineCoolantTemp;
			}
			else if(!strcmp(OBDInfo[j], "FuelLevel")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->fuelLevel;
			}
			else if(!strcmp(OBDInfo[j], "IntakeAirTemp")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->intakeAirTemp;
			}
			else if(!strcmp(OBDInfo[j], "EngineRPM")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->engineRPM;
			}
			else if(!strcmp(OBDInfo[j], "MassAirFlow")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->massAirFlow;
			}
			else if(!strcmp(OBDInfo[j], "IntakeManifoldAbsolutePressure")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->intakeManifoldAbsolutePressure;
			}
			else if(!strcmp(OBDInfo[j], "ThrottlePosition")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->throttlePosition;
			}			
			else if(!strcmp(OBDInfo[j], "VIN")){    
			strncpy(datalog.value.s,OBUData->VIN,MIN(strlen(OBUData->VIN) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->VIN) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "FuelUsed")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->fuelUsed;
			}
			else if(!strcmp(OBDInfo[j], "MainVoltage")){    
			datalog.valueType = FLOAT_VALUE;		
			datalog.value.f = OBUData->mainVoltage;
			}
			else if(!strcmp(OBDInfo[j], "PendingCodeStatus")){    
			strncpy(datalog.value.s,OBUData->pendingCodeStatus,MIN(strlen(OBUData->pendingCodeStatus) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->pendingCodeStatus) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "ReportID")){    
			strncpy(datalog.value.s,OBUData->reportID,MIN(strlen(OBUData->reportID) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->reportID) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "SN")){    
			strncpy(datalog.value.s,OBUData->SN, MIN(strlen(OBUData->SN) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->SN) , OBD_STRING_LENGTH)] = '\0';
			}
			else if(!strcmp(OBDInfo[j], "ReportTime")){    
			strncpy(datalog.value.s,OBUData->utcTime,MIN(strlen(OBUData->utcTime) , OBD_STRING_LENGTH));
			datalog.value.s[MIN(strlen(OBUData->utcTime) , OBD_STRING_LENGTH)] = '\0';
			}			
			else{
				printf("Not found value:%s\n",OBUData[j]);
			}
					
			//printf("sgsWriteSharedMemory:%s temp->valueName:%s valueName:%s datalog:%s \n",temp->sensorName ,temp->valueName, LoRaGWInfo[j] , datalog.value.s);  
			//printf("%u\n",temp->modbusInfo.ID);
			ret = sgsWriteSharedMemory(temp,&datalog);                  
			if(ret){
				printf("write fail! ret %d\n",ret);
			}
			else
			{
				printf("write s\n");
			}
			//printf("---------------------------------------\n");        
		}
		else{
			printf("temp is NULL!\n");
		}
		j++;
	}
	

	return 1;
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
		printf("parmData %x : \n\n" ,(parmData[0])); 
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
		 printf("\n\n index : %d\n" ,index); 
		 if(index >= size - 44 - 3)
		 {
			 printf("\n\n break index : %d\n" ,index); 
			 break;
		 }
		 printf("OptionData sizeof %d : \n\n" ,sizeof(OptionData)); 
		
		 memcpy(parmData, OptionData + index ,  parmData[1]);	
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
	double lat_tiny, lng_tiny;
	char *result = malloc(64);
	double X, Y;
	double lat, lng;	
	
	// 2010-08-15 處理經緯度正負號問題
	//Lat
	X = USER_LOG[4] * pow(256, 0) + USER_LOG[5] * pow(256, 1) + USER_LOG[6] * pow(256, 2) + USER_LOG[7] * pow(256, 3);
	X = (X > 2147483647 ? (4294967295 - X) * -1 : X) / 10000.0;
	
	//Lng
	Y = USER_LOG[8] * pow(256, 0) + USER_LOG[9] * pow(256, 1) + USER_LOG[10] * pow(256, 2) + USER_LOG[11] * pow(256, 3);
	Y = (Y > 2147483647 ? (4294967295 - Y) * -1 : Y) / 10000.0;	

	
	val = (X * 100000);	
	val = val  % (100 * 100000);
	lat_tiny = val ;
	lat_tiny = lat_tiny / 100000 / 60;
	
	val = (Y * 100000);	
	val = val  % (100 * 100000);
	lng_tiny = val ;
	lng_tiny = lng_tiny / 100000 / 60;
	
	
	// OBUData.Latitude = System.Convert.ToDouble(Data[4]) / 100 + ((System.Convert.ToDouble(Data[4]) % 100) / 60);
    // OBUData.Longitude = System.Convert.ToDouble(Data[6]) / 100 + ((System.Convert.ToDouble(Data[6]) % 100) / 60);
    lng = Y / 100 + lat_tiny;
	lat = X / 100 + lng_tiny;
	
	snprintf( result, 20, "%lf:%lf", lat , lng );  	
	return result;
}

float getSpeed(unsigned char USER_LOG[]){
	
	float speed;	
	//System.Convert.ToString((USER_LOG[16] * Math.Pow(256, 0) + USER_LOG[17] * Math.Pow(256, 1)) / 10.0);	// Speed
	speed = USER_LOG[16] * pow(256, 0) + (USER_LOG[17] * pow(256, 1)) / 10.0;	
	return speed;
}

char *getUTCDateTime(unsigned char USER_LOG[]){
	
	char *strtime;
	//Result[26] = USER_LOG[18].ToString("00") + USER_LOG[19].ToString("00") + USER_LOG[20].ToString("00") + USER_LOG[21].ToString("00") + USER_LOG[22].ToString("00") + USER_LOG[23].ToString("00");	// UTCDateTime 	
	// 'yyyy-MM-dd HH:mm:ss'
	snprintf( strtime, 19 , "20%d-%d-%d %d:%d:%d", USER_LOG[18] , USER_LOG[19]+1, USER_LOG[20] , USER_LOG[21] , USER_LOG[22] , USER_LOG[23]  );  
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

dataInfo* findTag(dataInfo *temp , char *valueName , char *gwid){
    int i,j;
	//printf("findTag called\n");
    while(temp != NULL){
        i=0;
        //while(DeviceName[i] != NULL){            
            //printf("\ntemp->valueName: %s temp->sensorName:%s  gwid: %s \n", valueName, temp->sensorName, gwid);
            if(!strcmp(temp->sensorName,gwid)){        
                  if(!strcmp(temp->valueName,valueName)){
                    //printf("Match SensorName %s and valueName %s\n",temp->sensorName,valueName);                 
                    //printf("break La\n");
                    return temp;                        
                }      
            }            
            i++;
        //}        
        temp = temp->next;
    }
    return NULL;
}