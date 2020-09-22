// ftpServer.c

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
#include <sys/wait.h>
#include<time.h>
#include <ifaddrs.h>
#include<fcntl.h>
#include <dirent.h>

#define SOCK struct sockaddr_in 
#define  bufflen 100
#define pendingQ 100
#define pid_t unsigned int
FILE *fp;
int zomproc = 0;

// server sends the file in the filepath to the client over datasocket created
void serverSendFile(int dataSocket, char *filepath) {
	int j = 0;
	char buffer[bufflen];

	fp = fopen(filepath, "r");

	if(fp == NULL)
	{
		printf("Requested file is not found in the sever.\n");
		return;
	}

	char str[bufflen];
	bzero(str, bufflen);
	bzero(buffer, bufflen);

	//Buffer is loaded with contents of the file and sent through datsocket
	while((str[0] = fgetc(fp)) != EOF)
	{
		strcat(buffer, str);
	}

	printf("File sending started...\n");

	int count = write(dataSocket, buffer, sizeof(buffer));
	printf("File sent successfully!\n");

	fclose(fp);
	return;
}

// for receiving file to store at filepath over datasocket from client
int recieveFile(int dataSocket, char *filepath) {
	char buff[bufflen];
	bzero(buff, bufflen);
	printf("File receiving started...\n");

	// read buffer on datasocket into the filepath mentioned
	int c = read(dataSocket, buff, sizeof(buff));

	fp = fopen(filepath, "w");
	fprintf(fp, "%s\n", buff);
	printf("File recieved successfully!\n");

	fclose(fp);
	return c;
}


// create datasocket for all transfers for get or put operations
void datatrans(SOCK clientAddr, unsigned long int port, char* filepath, int mode) {
	SOCK clientAddr2;

	// creating socket for data transfers
	int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(dataSocket == -1)
	{
		printf("Error creating datasocket!\n");
		return;
	}

 	bzero(&clientAddr2, sizeof(clientAddr2));

	clientAddr2.sin_family = AF_INET;
	clientAddr2.sin_addr.s_addr = clientAddr.sin_addr.s_addr;
	clientAddr2.sin_port = port;
	
	// connect datasocket with the listening port
	int connectStatus = connect(dataSocket, (struct sockaddr*) &clientAddr2, sizeof(clientAddr2));

	if (connectStatus < 0)
	{
		printf("Connection Error!\n");
		return;
	}

	// mode = 1 implies put
	// mode = 0 implies get
	if (mode == 1) recieveFile(dataSocket, filepath);
	else serverSendFile(dataSocket, filepath);

	// closing non-persistent connection 
	close(dataSocket);
}

// compute filepath of file requested and calls datatransfer to open datatransfer
void getfile_server(int serviceSocket, SOCK clientAddr, char* fileName) {
	int i;
	char filepath[100] = "server_homeDir/";
 
	char buff[bufflen];
	bzero(buff, bufflen);

	int len = strlen(filepath);

	for (i = 0; i < strlen(fileName); i++)
	{
		filepath[i+len] = fileName[i];
	}

	
	fp = fopen(filepath, "r");
	// check if requested file is present in server_homeDir
	if(fp == NULL)
	{
		printf("File %s not found\n", fileName);
		char replyBuff[bufflen] = "0";
		int c = write(serviceSocket, replyBuff, sizeof(replyBuff));
	}
	else
	{
		char replyBuff[bufflen] = "1";

		// file is found and is ready to send it to client
		int c = write(serviceSocket, replyBuff, sizeof(replyBuff));
		char buff[bufflen] = "\0";
		bzero(buff, bufflen);

		// listening port is changed to new port informed by client
		c = read(serviceSocket, buff, sizeof(buff));
		unsigned long int port = atoi(buff);
		datatrans(clientAddr, port, filepath, 0);
	}
}

// reads the command and calls the functions accordingly
int readcommand(int serviceSocket, SOCK clientAddr) {
	int i = 0, j;
	int dataSocket;
	int k = 0, m = 0;
	char command[5] = "\0";
	char fileName[20] = "\0";
	char filepath[100] = "server_homeDir/";
	time_t t;
 	srand((unsigned)time(&t));

	char buff[bufflen];
	bzero(buff, bufflen);

	int c = read(serviceSocket, buff, sizeof(buff));

	// breaking the read buff into command and file names
	while(buff[i] != ':')
	{
		command[m++] = buff[i];
		i++;
	}

	for(j = i+1; buff[j] != '#'; j++)
	{
		fileName[k++] = buff[j];
	}

	int len = strlen(filepath);

	// determine filepath from filename
	for (i = 0; i < strlen(fileName); ++i)
	{
		filepath[i+len] = fileName[i];
	}

	fp = fopen(filepath, "r");

	// call functions according to commands
	if(strcmp(command, "put") == 0)
	{
		if(fp == NULL)
		{
			char dataBuff[bufflen] = "1";

			// file not found->send 1(no need to ask for permission to overwrite)
			int c = write(serviceSocket, dataBuff, sizeof(dataBuff));
			bzero(buff, bufflen);

			//read to get the listening port the client is sending from
			c = read(serviceSocket, buff, sizeof(buff));
			unsigned long int port = atoi(buff);

			//call datatransfer for recieve file using mode 1(put)
			datatrans(clientAddr, port, filepath, 1);
		}
		else
		{
			fclose(fp);
			char dataBuff[bufflen] = "0";

			// file already present(need to ask permission to overwrite before sending)
			int c = write(serviceSocket, dataBuff, sizeof(dataBuff));
			bzero(buff, bufflen);

			// read answer to know if clients wants to overwrite
			c = read(serviceSocket, buff, sizeof(buff));
			// accepted to overwrite
			if(buff[0] == '1')
			{
				bzero(buff, bufflen);
				c = read(serviceSocket, buff, sizeof(buff));
				unsigned long int port = atoi(buff);

				datatrans(clientAddr, port, filepath, 1);
			}

		}

	}
	else if(strcmp(command, "get") == 0)
	{
		getfile_server(serviceSocket, clientAddr, fileName);
	}
	else if(strcmp(command, "mget") == 0)
	{
		// request to send all files with an extension
		DIR *d;
		struct dirent *dir;
		d = opendir("server_homeDir/");
		if(!d)
		{
			printf("Directory cannot be opened.\n");
		}
		else
		{
				while ((dir = readdir(d)) != NULL)
				{
					// for all files inside the directory check if the extension satisfies the condition
					char *file = dir->d_name;
					if(file[strlen(file)-1] == fileName[strlen(fileName)-1])
					{
						char replyBuff[bufflen] = "1#";
						strcat(replyBuff, file);
						strcat(replyBuff, "#");

						// inform the client with the filenames with the same extension it asked for 
						int c = write(serviceSocket, replyBuff, sizeof(replyBuff));

						bzero(replyBuff, sizeof(replyBuff));

						// read clients request to send the file 
						c = read(serviceSocket, replyBuff, sizeof(replyBuff));
						getfile_server(serviceSocket, clientAddr, file);
						bzero(buff, bufflen);
						// check if the client is ready to recieve further
						c = read(serviceSocket, buff, sizeof(buff));
					}
				}
				// inform the client that no more files are there with the extension
				char replyBuff[bufflen] = "0#";
				int c = write(serviceSocket, replyBuff, sizeof(replyBuff));

    			closedir(d);
			}
		}
	return c;
}

// after establishing connection successfully serve the client
void serveclient(int serviceSocket, SOCK clientAddr) {

	while(readcommand(serviceSocket, clientAddr) != 0)
	{
		printf("The File transfer is completed successfully\n");
	}

}

int main(int argc, char* argv[]) {

	// sockaddr_in for sever and client to store their information
	SOCK serverAddr, clientAddr;

	int listenPort;
	pid_t pid;
	char buff[bufflen];

    	// variables to store the IP address of the machine on which server is running
	struct ifaddrs * ifAddrStruct=NULL;
	struct ifaddrs * ifa=NULL;
	void * tempaddress=NULL;
	getifaddrs(&ifAddrStruct);

	//create listening port
	listenPort = socket(AF_INET,SOCK_STREAM,0);
	
	if(listenPort==-1) 
	{
		printf("Error opening the listen port\n");
		return 0;
	}
	char ip[20];
	//get IP to store in sever socket address
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) 
	{
		if (!ifa->ifa_addr) continue;
		if (ifa->ifa_addr->sa_family == AF_INET) 
		{
		    tempaddress = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
		    char addressBuffer[INET_ADDRSTRLEN];
		    inet_ntop(AF_INET, tempaddress, addressBuffer, INET_ADDRSTRLEN);
		    if(strncmp(ifa->ifa_name, "wlp", 3) == 0)
		    {
		    	strcpy(ip, addressBuffer);
		    }
		}
	}

    	unsigned long int  ipAddr = inet_addr(ip);

	bzero(&serverAddr, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ipAddr;
	serverAddr.sin_port = htons(atoi(argv[1]));

	// bind socket with sever
	int bindStatus = bind(listenPort, (struct sockaddr*) &serverAddr, sizeof(serverAddr));

	if(bindStatus == -1)
	{
		printf("Bind Error\n");
		return 0;
	}

	printf("Server started on IP %s at port %s\n", ip, argv[1]);

	// listen for connecitons on the listening port
	int listenStatus = listen(listenPort, pendingQ);
	if(listenStatus == -1)
	{
		printf("Listen Error\n");
		return 0;
	}

	while(1)
	{
		int len = sizeof(clientAddr);

		// start tcp connection
		int acceptSocket = accept(listenPort, (struct sockaddr*) &clientAddr, &len);
		printf("connection is from %s, port %d\n", inet_ntop(AF_INET, &clientAddr.sin_addr, buff, sizeof(buff)), ntohs(clientAddr.sin_port));

		// create child to serve the client
		if ((pid = fork()) < 0) printf("Error in creating child\n");
		else if (pid == 0) 
		{
			// close listenport
			close(listenPort);
			serveclient(acceptSocket,clientAddr);
			printf("Connection from %s, port %d has been ended\n", inet_ntop(AF_INET, &clientAddr.sin_addr, buff, sizeof(buff)), ntohs(clientAddr.sin_port));

			// close socket after serving the client
			close(acceptSocket);
			exit(0);
		}
		close(acceptSocket);
		
		zomproc++;
		// killing zombie processes
		while(zomproc)
		{
			pid = waitpid((pid_t) -1, NULL, WNOHANG);
			if (pid < 0) printf("Error\n");
			else if (pid == 0) break;
			else zomproc--;
		}
	}
	return 0;
}
