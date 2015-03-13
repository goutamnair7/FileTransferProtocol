#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>
#include <dirent.h>

struct file
{
    char name[1024];
    char time[1024];
    int size;
    unsigned char md5[100];
};

char recvCommand[5][100];
char command[5][100], s[1024];
struct file fs[1024];
struct file cfs[1024];

int fileCount = 0;

void parseCommand(char *buffer)
{
    int i, k=0, ct=0;
    for(i=0;i<strlen(buffer);i++)
    {
        if (buffer[i] == ' ')
        {
            recvCommand[ct][k++] = '\0';
            ct++;
            k = 0; 
            continue;
        }
        recvCommand[ct][k] = buffer[i];
        k++;
    }
    recvCommand[ct][k++] = '\0';
    ct++;
}

void getCommand()
{
    int ct=0, k=0, i, j=0;
    char c;

    scanf("%c", &c);
    while(c!='\n')
    {
        s[j++] = c;
        scanf("%c", &c);
    }
    s[j] = '\0';

    for (i=0;i<strlen(s); i++) 
    {    
        if (s[i] == ' ') 
        {
            command[ct][k++] = '\0';
            ct++;
            k = 0; 
            continue;
        }
        command[ct][k] = s[i];
        k++;
    }    
    command[ct][k++] = '\0';
    ct++;
}

void getMD5(char *filename, char *s)
{
    int i;
    FILE *f = fopen (filename, "r");
    char data[1024], c[1024];
    int bytes;

    MD5_CTX mdContext;
    MD5_Init(&mdContext);
    while ((bytes = fread(data, 1, 1024, f)) != 0)
        MD5_Update(&mdContext, data, bytes);
    MD5_Final(c,&mdContext);
    for(i=0;i<100;i++) 
        s[i] = c[i];
    s[i+1] = '\0';
    fclose(f);
}

void checkFiles()
{
    unsigned char c[MD5_DIGEST_LENGTH];
    DIR *dp;
    int i = 0, a;
    struct dirent *ep;
    dp = opendir ("./");
    struct stat fileStat;

    if (dp != NULL)
    {
        while (ep = readdir (dp))
        {
            if(stat(ep->d_name,&fileStat) < 0)
                return;
            else
            {
                strcpy(fs[i].name, ep->d_name);
                struct stat details;
                stat(ep->d_name, &details);

                int size = details.st_size;
                fs[i].size = size;
                strcpy(fs[i].time, ctime(&details.st_mtime));

                FILE *inFile = fopen (fs[i].name, "r");
                MD5_CTX mdContext;
                int bytes;
                unsigned char data[1024];

                MD5_Init (&mdContext);
                while ((bytes = fread (data, 1, 1024, inFile)) != 0)
                    MD5_Update (&mdContext, data, bytes);

                MD5_Final (c,&mdContext);

                for(a = 0; a < 100; a++)
                    fs[i].md5[a] = c[a];

                fclose (inFile);
    /*        printf("Name: %s\n", fs[i].name);
            printf("Size: %d\n", fs[i].size);
            printf("Time: %s\n", fs[i].time);
            printf("MD5: %02x\n", fs[i].md5);*/
                i++;
            }
        }
        fileCount = i;
    }
    else
        printf("Couldn't open the directory");
}

void server(int port, char type[])
{
    int sock = 0, connectionSocket = 0;
    struct sockaddr_in serv_addr, client_addr;
    int connected, bytesReceived;
    char buffer[1024];

    sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0)
    {   
        printf("Error while creating socket\n");
        return;
    }
    
    bzero((char *) &serv_addr,sizeof(serv_addr));

    int portno = port;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    if(bind(sock,(struct sockaddr * )&serv_addr,sizeof(serv_addr)) < 0)
        printf("Error while binding socket\n");

    if(listen(sock, 10) == -1)
        printf("[SERVER] Failed to establish listen\n\n");

    while(1)
    {
        connected = accept(sock, (struct sockaddr_in *)NULL, NULL);

        while(1)
        {
            bytesReceived = recv(connected, buffer, 1024, 0);
            buffer[bytesReceived] = '\0';

            if(bytesReceived == 0)
            {
                close(connected);
                break;
            }
            if(strcmp(buffer, "exit") == 0)
            {
                printf("Connection Closed\n");
                close(connected);
                break;
            }
            else
            {
                printf("\nRequest: %s\n\n", buffer);
                parseCommand(buffer);
                if(strcmp(recvCommand[0], "FileDownload") == 0)
                {
                    if(access(recvCommand[1], F_OK) != -1)
                    {
                        send(connected, "file exists", 1024, 0);
                        char c;
                        FILE *f;
                        int count = 0;
                        f = fopen(recvCommand[1], "r");
                        while(fscanf(f, "%c", &c) != EOF)
                        {
                            count = 0;
                            char sendBuffer[1024];
                            sendBuffer[count++] = c;

                            while(count < 1024 && fscanf(f, "%c", &c)!=EOF)
                                sendBuffer[count++] = c;
                            
                            send(connected, &count, sizeof(int), 0);
                            send(connected, sendBuffer, 1024, 0);
                        }
                        int len = strlen("end of file");
                        send(connected, &len, sizeof(int), 0);
                        send(connected, "end of file", 1024, 0);
                    }
                    else
                    {
                        send(connected, "file does not exist", 1024, 0);
                    }
                }
                else if(strcmp(recvCommand[0], "IndexGet") == 0)
                {
                    int i = 0;
                    checkFiles();
                
                    if (!strcmp(type,"tcp"))
                        send(connected, &fileCount, sizeof(int), 0);

                    for (i=0; i<fileCount; i++)
                    {
                            send(connected, fs[i].name, 1024, 0);
                        
                            send(connected, &fs[i].size, sizeof(int), 0);

                            send(connected, fs[i].time, 1024, 0);
                    }
                }

                else if(strcmp(recvCommand[0], "FileHash") == 0)
                {
                    checkFiles();
                    if (!strcmp(type, "tcp"))
                        send(connected, &fileCount, sizeof(int), 0);
                    else
                        sendto(sock, &fileCount, sizeof(int), 0,(struct sockaddr *)&client_addr, sizeof(struct sockaddr));
                    
                    int i;
                    for (i=0; i<fileCount ; i++)
                    {
                        if (!strcmp(type, "tcp"))
                            send(connected, fs[i].name, 1024, 0);

                        if (!strcmp(type, "tcp"))
                            send(connected, &fs[i].size, sizeof(int), 0);

                        if (!strcmp(type, "tcp"))
                            send (connected, fs[i].time, 1024, 0);

                        if (!strcmp(type, "tcp"))
                            send (connected, fs[i].md5, 1024, 0);
                    }
                }
            }
        }
    }
    close(sock);
    return;
}

int client(int port, char type[])
{
    int sock = 0, bytesReceived, len;
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0)
    {   
        printf("Error while creating socket\n");
        return;
    }   

    int portno = port;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1)
    {
        printf("Could not connect to port\n");
        return;
    }
    else
        printf("[CONNECTED] to %d\n", port);

    char recvBuffer[1024];
    getCommand();

    while(strcmp(command[0], "exit") != 0)
    {
        if(strcmp(command[0], "FileDownload") == 0)
        {
            send(sock, s, strlen(s), 0);
            bytesReceived = recv(sock, recvBuffer, 1024, 0);
            recvBuffer[bytesReceived] = '\0';

            if(strcmp(recvBuffer, "file does not exist") != 0)
            {
                recv(sock, &len, sizeof(int), 0);
                bytesReceived = recv(sock, recvBuffer, 1024, 0);
                recvBuffer[bytesReceived] = '\0';

                FILE *f;
                f = fopen(command[1], "w");

                while(strcmp(recvBuffer, "end of file") != 0)
                {
                    int i;
                    for(i=0;i<len;++i)
                        fprintf(f, "%c", recvBuffer[i]);
                    recv(sock, &len, sizeof(int), 0);
                    bytesReceived = recv(sock, recvBuffer, 1024, 0);
                    recvBuffer[bytesReceived] = '\0';
                }
                printf("File Downloaded succesfully\n");
                printf("\n==============================================================\n");
                fclose(f);
            }
            else
                printf("\nNo such file exists\n\n");
        }
        else if(strcmp(command[0], "IndexGet") == 0)
        {
            if(strcmp(command[1], "--longlist") == 0)
            {
                int t, i, bytesRecieved;
                char recvData[1024];
                if (!strcmp(type,"tcp"))
                    send(sock, s,strlen(s),0);

                if (!strcmp(type, "tcp"))
                    recv(sock, &t, sizeof(int), 0);

                int fileCount = t;

                for(i=0; i<fileCount; i++)
                {
                    if (!strcmp(type,"tcp"))
                        bytesRecieved = recv(sock, recvData,1024, 0);
                    recvData[bytesRecieved]='\0';
                    strcpy(cfs[i].name, recvData);

                    if (!strcmp(type, "tcp"))
                        recv(sock, &t,sizeof(t), 0);
                    cfs[i].size = t;

                    if (!strcmp(type, "tcp"))
                        recv(sock, recvData, 1024, 0);
                    strcpy(cfs[i].time, recvData);
                }
                printf("\n\n");
                for (i =0; i<fileCount;i++)
                {
                    printf("\n=================================================\n\n");
                    printf("Name : %s\nSize : %d\nTime : %s\n",cfs[i].name,cfs[i].size,cfs[i].time);
                    printf("\n=================================================\n\n");
                }
            }
            else
                printf("\nPlease enter a valid flag('longlist')\n\n");
        }

        else if(strcmp(command[0], "FileHash") == 0)
        {
            int t, i, bytesRecieved;
            char recvData[1024];
            send(sock, s, strlen(s), 0);
            recv(sock, &t, sizeof(int), 0);
            for (i=0; i<t; i++)
            {
                if (!strcmp(type, "tcp"))
                    bytesRecieved = recv(sock, recvData, 1024, 0);
                recvData[bytesRecieved] = '\0';
                strcpy(cfs[i].name, recvData);

                if (!strcmp(type, "tcp"))
                    recv(sock, &cfs[i].size, sizeof(int), 0);
                
                if (!strcmp(type, "tcp"))
                    bytesRecieved = recv(sock, recvData, 1024, 0);
                recvData[bytesRecieved] = '\0';
                strcpy(cfs[i].time,recvData);

                if (!strcmp(type, "tcp"))
                    recv(sock, cfs[i].md5, 1024, 0);
            }

            if (!strcmp(command[1],"--verify"))
            {
                int i;
                for (i=0; i<t; i++)
                {
                    if (!strcmp(command[2], cfs[i].name))
                    {
                        printf("\n==============================================================\n");
                        printf("\nFile : %s\nSize: %d\nLast-Edited : %s\nMD5sum : %02x\n",cfs[i].name, cfs[i].size, cfs[i].time, cfs[i].md5);
                        printf("\n==============================================================\n");
                        break;
                    }
                }
                if (i==t)
                    printf ("\n Error : no such file\n");
            }
            else if (!strcmp(command[1], "--checkall"))
            {
                int i;
                for (i=0; i<t; i++)
                {
                    printf("\n==============================================================\n");
                    printf("\nFile : %s\nSize: %d\nLast-Edited : %s\nMD5sum : %05x\n",cfs[i].name, cfs[i].size, cfs[i].time, cfs[i].md5);
                    printf("\n==============================================================\n");
                }
            }
            else
                printf("\nPlease enter a valid flag('verify'/'checkall')\n\n");
        }
        getCommand();
    }
    close(sock);
    return 0;
}

int main()
{
    int serverPort, clientPort;
    char *type = "tcp";

    printf("Port to bind to: ");
    scanf("%d", &serverPort);

    printf("Port to send data: ");
    scanf("%d", &clientPort);

    pid_t pid;
    pid = fork();
    if(pid == -1)
    {
        printf("Error. Big time!!!\n");
        return 0;
    }
    else if(pid == 0)
    {
        server(serverPort, type);
    }
    else
    {
        while(1)
        {
            int f = client(clientPort, type);
            if(f <= 0)
                break;
            sleep(1);
        }
    }
    return 0;
}
