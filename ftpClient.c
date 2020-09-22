/* ftpClient.c */
// client sends the request to the server to create presistent FTP control connection
// it then issues commandsto send and receive files from/to server

/************************************************************************************************************/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <errno.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <ifaddrs.h> 
#include <dirent.h>

#define SOCK struct sockaddr_in 
#define MAXLEN 100
#define LISTENQ 100
FILE *fp;

// receives the file from the server to the client
int clientreceiveFile(int dataSocket, char *fileName) {
	int j = 0;
	char buff[MAXLEN];
	char path[100] = "client_homeDir/";
	bzero(buff, MAXLEN);
	// wait for server to send the file data
	int c = read(dataSocket, buff, sizeof(buff));

	// Calculate file path
	int len = strlen(path);
	for(j=0; j<strlen(fileName); j++)
	{
		path[len+j] = fileName[j];
	}

	fp = fopen(path,"r+");
	// File is already found in the client
	if(fp != NULL)
	{
		printf("File %s is already present!\nDo you want to overwrite the file?\n Type '1' for yes and '0' for no: ", fileName);
		int reply;
		scanf("%d", &reply);
		fclose(fp);
		// if the user wants to overwrite the file, reply = 1
		if(reply == 1)
		{
			// If file is already present Overwrite
			fp = fopen(path, "w");
			fprintf(fp, "%s\n", buff);
			fclose(fp);
		}
	}
	// File is not found in the client. Receive file from server as it is
	else
	{
		fp = fopen(path, "w");
		fprintf(fp, "%s\n", buff);
		fclose(fp);
		printf("File %s has been received successfully\n", fileName);
	}

	return c;
}

// sends the file from the client to the server
void sendFile(int dataSocket, char *fileName){
	int j = 0;
	char path[100] = "client_homeDir/";
	char buffer[MAXLEN];
	char str[MAXLEN];

	// calculate the file path
	int len = strlen(path);
	for(j=0; j<strlen(fileName); j++)
	{
		path[len+j] = fileName[j];
	}

	fp = fopen(path, "r");
	// file not found in the client
	if(fp == NULL)
	{
		printf("Cannot open file %s, File not found\n", fileName);
		return;
	}

	// Read file into buffer
	while((str[0] = fgetc(fp)) != EOF)
	{
		strcat(buffer, str);
	}

	printf("File %s sending started...\n", fileName);
	// send file data buffer to server
	int count = write(dataSocket, buffer, sizeof(buffer));

	printf("File %s has been sent successfully\n", fileName);
	fclose(fp);
	return;
}

// allows the data transfer between server and client 
// Here mode = 1 for receiving data and mode = 0 for sending data.
void dataTransfer(unsigned long int port, int requestSocket, char* dataPortStr, char* name, int mode)
{
	// variables to store server and client address resp
	SOCK serverAddr, clientAddr;
	// variable to store ip address of client
	struct ifaddrs *id;
	char buff[MAXLEN];

	// calculate IP of host
	struct ifaddrs* ifAddrStruct = NULL;
    	struct ifaddrs* ifa = NULL;
    	void* tmpAddrPtr = NULL;
	getifaddrs(&ifAddrStruct);
	char ip[20];

	// listen port for new data connection 
	int clientListenPort = socket(AF_INET, SOCK_STREAM, 0);
	// for loop to calculate IP address of host
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
	{
		if(!ifa->ifa_addr) continue;

		if (ifa->ifa_addr->sa_family == AF_INET)
		{
		    tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
		    char addressBuffer[INET_ADDRSTRLEN];
		    inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
		    if(strncmp(ifa->ifa_name, "wlp", 3) == 0) strcpy(ip, addressBuffer);
		}
    	}

	// ipAddr stores ip address in the required network format 
	unsigned long int  ipAddr = inet_addr(ip);
	// if connection could not be established with server 
	if(clientListenPort == -1) 
	{
		printf("Error opening the Listen port for Data transfer.\n");
		return;
	}
	// else, if the data connection is established
	bzero(&clientAddr, sizeof(clientAddr));

	// Open listen socket to open data trasfer socket
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_addr.s_addr = ipAddr;
	clientAddr.sin_port = port;
	printf("Listen Port %ld has been opened to listen for data Connection requests.\n", port);

	// binds the ip address and port number to the socket 
	int bindStatus = bind(clientListenPort, (struct sockaddr*) &clientAddr, sizeof(clientAddr));
	// bind error 
	if(bindStatus==-1)
	{
		printf("Listen Socket: bind error\n");
		return;
	}

	bzero(buff, MAXLEN);

	// send listen port to server
	int count = write(requestSocket, dataPortStr, sizeof(dataPortStr));

	int listenStatus = listen(clientListenPort, LISTENQ);
	// listen error
	if(listenStatus == -1)
	{
		printf("Listen Socket: Listen Error.\n");
		return;
	}

	// accept and create a new data socket
	int len = sizeof(serverAddr);
	int dataSocket = accept(clientListenPort, (struct sockaddr*) &serverAddr, &len);

	printf("DATA connection from ip %s, port %d is established.", inet_ntop(AF_INET, &serverAddr.sin_addr, buff, sizeof(buff)), ntohs(serverAddr.sin_port));
	printf("Data Socket is opened, Sharing will begin now...\n");

	// close the listen port
	close(clientListenPort);
	// mode = 0 for sending the data 
	if(mode == 0) sendFile(dataSocket, name);
	// mode = 1 for receiving the data 
	else clientreceiveFile(dataSocket, name);

	// close the data socket created
	close(dataSocket);
	return;
}


// to send the current file (with file_name = name) to the server 
void putFile(int requestSocket, char *name, char* strinp) {
	char buffer[MAXLEN];
	char buff[MAXLEN];

	char path[100] = "client_homeDir/";
	char str[MAXLEN];
	int j = 0;

	// calculate the file path
	int len = strlen(path);
	for(j=0; j<strlen(name); j++)
	{
		path[len+j] = name[j];
	}

	bzero(buffer, MAXLEN);
	fp = fopen(path, "r");
	// client does not have the current file
	if(fp == NULL)
	{
		printf("File %s not present.\n", name);
		return;
	}

	// file name to be sent along with command
	strcat(buffer, strinp);
	strcat(buffer, name);
	strcat(buffer, "#");

	int count = write(requestSocket, buffer, sizeof(buffer));

	bzero(buffer, MAXLEN);
	bzero(buff, MAXLEN);

	// wait for server to reply if file is present in server or not
	int c = read(requestSocket, buff, sizeof(buff));

	// 1 signifies that the server doesn't have the sent file already 
	if(buff[0] == '1')
	{
		// calculate the ephimeral port to listen the data connection request from server
		int x = rand() % 1000;
		unsigned long int dataPort = htons(x);
		char dataPortStr[50] = "\0";
		sprintf(dataPortStr, "%ld\n", dataPort);
		// to allow the data transfer between server and client
		// here 0 is sent as mode for sending data
		dataTransfer(dataPort, requestSocket, dataPortStr, name, 0);
	}
	// else 0 is received from server signifying that the server already has a file with the same name
	else
	{
		printf("File %s already present on server.\nDo you want to overwrite the file?\n Reply '1' for yes and '0' for no: ", name);
		int reply;
		scanf("%d", &reply);
		// if the user wants to overwrite the file on server, 1 is sent and data transfer occurs
		if(reply == 1)
		{
			char replyBuff[MAXLEN]= "1";
			printf("Overwrite command sent.\n");
			// reply to server to overwrite the file
			int c = write(requestSocket, replyBuff, sizeof(replyBuff));

			// calculate the ephimeral port to listen the data connection request from server
			int x = rand() % 1000;
			unsigned long int dataPort = htons(x);
			char dataPortStr[50] = "\0";
			sprintf(dataPortStr, "%ld\n", dataPort);
			// to allow the data transfer between server and client.
			// here 0 is sent as mode for sending data.
			dataTransfer(dataPort, requestSocket, dataPortStr, name, 0);
		}
		// if the user doesn't want to overwrite the file on server, 0 is sent 
		else
		{
			char replyBuff[MAXLEN] = "0";
			int c = write(requestSocket, replyBuff, sizeof(replyBuff));
		}
	}
}

// to get the current file (with file_name = name) from the server 
void getFile(int requestSocket, char* name, char* strinp) {
	char buffer[MAXLEN];
	char buff[MAXLEN];
	char str[100];

	bzero(buffer, MAXLEN);

	// file name to be sent along with command
	strcat(buffer, strinp);
	strcat(buffer, name);
	strcat(buffer, "#");
	// send the command name := GET and the file_name := name to the server 
	int count = write(requestSocket, buffer, sizeof(buffer));

	bzero(buffer, MAXLEN);
	bzero(buff, MAXLEN);

	// Wait for the status of file on the server and receive the data in buff 
	int c = read(requestSocket, buff, sizeof(buff));
	// 0 signifies that the server doesn't have the requested file 
	if(buff[0] == '0')
	{
		printf("The requested file %s is not found on the server.\n", name);
	}
	// else 1 is received which signifies that the server has the requested file 
	else
	{
		printf("Receiving started...\n");

		// calculate the ephimeral port to listen the data connection request from server
		int x = rand() % 1000;
		// dataPort stores the new port for data connection
		unsigned long int dataPort = htons(x);
		char dataPortStr[50] = "\0";
		sprintf(dataPortStr, "%ld\n", dataPort);
		// to allow the data transfer between server and client.
		// here 1 is sent as mode for receiving data.
		dataTransfer(dataPort, requestSocket, dataPortStr, name, 1);
	}
}

int main(int argc, char* argv[]) {

	// SOCK := struct sockaddr_in 
	SOCK serverAddr;

	// requestSocket is the control socket for control information.
	// datasocket is the socket used for data transfer.
	int requestSocket, datasocket;
	char ctos_buff[MAXLEN];
	char stoc_buff[MAXLEN];
	time_t t;
	srand((unsigned)time(&t));

	// client creates a control socket to send control informaton to server
	requestSocket = socket(AF_INET, SOCK_STREAM, 0);

	if(requestSocket == -1)
	{
		printf("Error opening the Control Socket.\n");
		return 0;
	}

	bzero(&serverAddr, sizeof(serverAddr));

	// ip here refers to the ip address of the server taken from command line input 
	unsigned long int ip = inet_addr(argv[1]);

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ip;

	// port value is assigned from command line input.
	serverAddr.sin_port = htons(atoi(argv[2]));

	// connect request is sent to the server 
	int connectStatus = connect(requestSocket, (struct sockaddr*) &serverAddr, sizeof(serverAddr));

	// if connection could not be established with the server 
	if (connectStatus < 0)
	{
		printf("Control Socket: Connection Error.\n");
		return 0;
	}

	// else, when the connection is established 
	printf("Connection to the server %s on port %s has been established.\n", argv[1], argv[2]);
	while(1)
	{
		// name stores the name of the file 
		char name[50];
		// strinp stores the method to be run by client
		char strinp[50] = "\0";
		printf("Enter the command: ");
		scanf("%s %s", strinp, name);

		if(strcmp(strinp, "put:") == 0)
		{
			// send the current file to server
			putFile(requestSocket, name, strinp);
		}
		else if (strcmp(strinp, "get:") == 0)
		{
			// get the current file from server
			getFile(requestSocket, name, strinp);
		}
		else if (strcmp(strinp, "mput:") == 0)
		{
			DIR *d;
			struct dirent *dir;
			d = opendir("client_homeDir/");
			if (!d) printf("Directory cannot be opened.\n");
			else
			{
				// loops over all the files in the directory 
				while ((dir = readdir(d)) != NULL)
				{
					// file stores the file name
					char *file = dir->d_name;
					if(file[strlen(file)-1] == name[strlen(name)-1])
					{
						// send the current file to server 
						putFile(requestSocket, file, "put:");
					}
				}
        		closedir(d);
			}
		}
		else if (strcmp(strinp, "mget:") == 0)
		{
			// here, ctos_buff stores the data to be sent to server from client 
			char ctos_buff[MAXLEN];
			// here, stoc_buff stores the data to be received from server for the client 
			char stoc_buff[MAXLEN];
			bzero(ctos_buff, MAXLEN);
			char str[100];

			strcat(ctos_buff,strinp);
			strcat(ctos_buff,name);
			strcat(ctos_buff,"#");

			// write is used to send command to the server.
			// Here, it is used to send the name of the command used and name of file
			int count = write(requestSocket, ctos_buff, sizeof(ctos_buff));

			bzero(ctos_buff, sizeof(ctos_buff));
			bzero(stoc_buff, sizeof(stoc_buff));

			// flg variable is used to denote when the server stops sending the files it has.
			// flg='1' means it is still sending; flg='0' means sending has been completed.
			char flg = '1';
			// while sender has a file to send, getFile is called each time
			while(flg != '0')
			{
				// read is used to receive data sent from server.
				// Here, read is used to check if server has a file to send or not
				int c = read(requestSocket, stoc_buff, sizeof(stoc_buff));
				flg = stoc_buff[0];

				if(stoc_buff[0] == '1')
				{
					int j = 0;
					char fileName[MAXLEN];
					for (j=2; stoc_buff[j] != '#'; j++)
					{
						fileName[j-2] = stoc_buff[j];
					}
					fileName[j-2] = '\0';
					// get the current file from server 
					getFile(requestSocket, fileName, "get:");
					char recBuff[MAXLEN] = "1";
					// reply when file is received successfully
					count = write(requestSocket, recBuff, sizeof(recBuff));
				}
			}
			// char recBuff[MAXLEN] = "1";
			// Reply for synchronization
			// count = write(requestSocket, recBuff, sizeof(recBuff));

		}
		else
		{
			printf("Invalid Command: Try again.\n");
		}

	}
	return 0;
}
