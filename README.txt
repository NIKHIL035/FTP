ftpServer.c => FTP server file
ftpClient.c => FTP client file

/**********************************************************************************************************/

Steps to run the server:

1. gcc -o ftpServer ftpServer.c
2. sudo ./ftpServer <Port Number>

Steps to run the client:

1. gcc -o ftpClient ftpClient.c
2. sudo ./ftpClient <Server IP> <Server Port Number>


