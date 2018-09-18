
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>	/* for definition of errno */
#include <time.h>  

#define BUFFSIZE 65536
#define SCREEN_WIDTH 50

int proxy_cmd_port;
int accept_cmd_port;
int connect_cmd_port;
int proxy_data_port;
int accept_data_port;
int connect_data_port;
int passive_server_port;
int active_client_port;

char proxyBuff[BUFFSIZE] = {0};
char acceptBuff[BUFFSIZE] = {0};
char serverBuff[BUFFSIZE] = {0};
char listBuff[BUFFSIZE] = {0};
char getBuff[BUFFSIZE] = {0};
char putBuff[BUFFSIZE] = {0};

int mode; //1 is passive mode, 2 is active mode
int event; //1 is list, 2 is get, 3 is put

struct sockaddr_in serv_addr, client_addr;

int proxyCmdSocket();
int proxyDataSocket();
int acceptCmdSocket(int sockfd);
int connectCmdSocket();
int acceptDataSocket(int sockfd);
int rand_local_port();
void strtosrv(char *str, int *transferPortInt);
fd_set master_set, working_set;  //文件描述符集合
struct timeval timeout;          //select 参数中的超时结构体

int proxy_data_socket;

int main(int argc, const char *argv[])
{
    int proxy_cmd_socket    = -1;     //proxy listen控制连接
    int accept_cmd_socket   = -1;     //proxy accept客户端请求的控制连接
    int connect_cmd_socket  = -1;     //proxy connect服务器建立控制连接
    proxy_data_socket   = -1;     //proxy listen数据连接
    int accept_data_socket  = -1;     //proxy accept得到请求的数据连接（主动模式时accept得到服务器数据连接的请求，被动模式时accept得到客户端数据连接的请求）
    int connect_data_socket = -1;     //proxy connect建立数据连接 （主动模式时connect客户端建立数据连接，被动模式时connect服务器端建立数据连接）
    int selectResult = 0;     //select函数返回值
    int select_sd = 20;    //select 函数监听的最大文件描述符
    
    FD_ZERO(&master_set);   //清空master_set集合
    bzero(&timeout, sizeof(timeout));
    proxy_cmd_socket = proxyCmdSocket();  //开启proxy_cmd_socket、bind（）、listen操作
    //proxy_data_socket = proxyDataSocket(); //开启proxy_data_socket、bind（）、listen操作
	printf("Please connect to %d!\n", proxy_cmd_port);
    
    FD_SET(proxy_cmd_socket, &master_set);  //将proxy_cmd_socket加入master_set集合
    FD_SET(proxy_data_socket, &master_set);//将proxy_data_socket加入master_set集合
    
    timeout.tv_sec = 12000000;    //Select的超时结束时间
    timeout.tv_usec = 0;    //ms
    
    char filename[100];
    while (1) {
        FD_ZERO(&working_set); //清空working_set文件描述符集合
        memcpy(&working_set, &master_set, sizeof(master_set)); //将master_set集合copy到working_set集合
        
        //select循环监听 这里只对读操作的变化进行监听（working_set为监视读操作描述符所建立的集合）,第三和第四个参数的NULL代表不对写操作、和误操作进行监听
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);
        
        // fail
        if (selectResult < 0) {
            perror("select() failed\n");
            exit(1);
        }
        
        // timeout
        if (selectResult == 0) {
            printf("select() timed out.\n");
            continue;
        }
        
        // selectResult > 0 时 开启循环判断有变化的文件描述符为哪个socket
        int i = 0;
        for (i = 0; i < select_sd; i++) {
            //判断变化的文件描述符是否存在于working_set集合
            if (FD_ISSET(i, &working_set)) {
				printf("proxy_cmd_socket:%d\n",proxy_cmd_socket);
				printf("accept_cmd_socket:%d\n",accept_cmd_socket);
				printf("connect_cmd_socket:%d\n",connect_cmd_socket);
				printf("proxy_data_socket:%d\n",proxy_data_socket);
				printf("accept_data_socket:%d\n",accept_data_socket);
				printf("connect_data_socket:%d\n",connect_data_socket);
				printf("passive_server_port:%d\n", passive_server_port);
				printf("active_client_port:%d\n",active_client_port);
                if (i == proxy_cmd_socket) {
                    accept_cmd_socket = acceptCmdSocket(proxy_cmd_socket);  //执行accept操作,建立proxy和客户端之间的控制连接
                    connect_cmd_socket = connectCmdSocket(); //执行connect操作,建立proxy和服务器端之间的控制连接
                    
                    //将新得到的socket加入到master_set结合中
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);
                }
                
                if (i == accept_cmd_socket) {
                    printf("accept_cmd_socket\n");
                    memset(acceptBuff, 0, BUFFSIZE);
                   
                    if (recv(i, acceptBuff, BUFFSIZE,0) == 0) {
                        close(accept_cmd_socket); //如果接收不到内容,则关闭Socket
                        close(connect_cmd_socket);
						accept_cmd_socket = -1;
						connect_cmd_socket = -1;
                        
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(accept_cmd_socket, &master_set);
                        FD_CLR(connect_cmd_socket, &master_set);

                    } else {
                        printf("%s\n",acceptBuff);
                        //如果接收到内容,则对内容进行必要的处理，之后发送给服务器端（写入connect_cmd_socket）
                            
                        
                        //处理客户端发给proxy的request，部分命令需要进行处理，如PORT、RETR、STOR                        
                        
                        
                        //PORT
                        if(strncmp(acceptBuff,"PORT",4)==0)
                        {
							mode = 2;
                            strtosrv(acceptBuff, &active_client_port); //transfer string in acceptBuff to port in int
							proxy_data_socket = proxyDataSocket(); //开启proxy_data_socket、bind（）、listen操作
							FD_SET(proxy_data_socket, &master_set);//将proxy_data_socket加入master_set集合
                            sprintf(acceptBuff, "PORT 192,168,56,101,%d,%d\n", proxy_data_port >>8, proxy_data_port&0xff);
							//send PORT proxy server port information to the real server
                        }
                        //PASV
                        else if(strncmp(acceptBuff,"PASV",4)==0)
                        {
                            mode = 1;
                        }
                        //MLSD
                        else if(strncmp(acceptBuff,"MLSD",4)==0)
                        {
                            event = 1;
                        }
                        //RETR
                        else if(strncmp(acceptBuff,"RETR",4)==0)
                        {
							strncpy(filename, acceptBuff+5,80);
							filename[strlen(filename)-2] = '\0';
                            event = 2;

                        }
                        //STOR
                        else if(strncmp(acceptBuff,"STOR",4)==0)
                        {
                            strncpy(filename, acceptBuff+5,80);
							filename[strlen(filename)-2] = '\0';	//append end up character
                            event = 3;

                        }
                        
                        
                        //写入proxy与server建立的cmd连接,除了PORT之外，直接转发buff内容
                        if(write(connect_cmd_socket, acceptBuff, BUFFSIZE)<0)
                            printf("write to connect_cmd_socket error");
                    }
                }
                
                if (i == connect_cmd_socket) {
					printf("connect_cmd_socket\n");
                    memset(serverBuff,0,BUFFSIZE);
                    //处理服务器端发给proxy的reply，写入accept_cmd_socket
                    if(recv(connect_cmd_socket, serverBuff, BUFFSIZE,0)>0)
                    {
                        //PASV收到的端口 227 （port）
                        if(strncmp(serverBuff,"227",3)==0)
                        {    
                            strtosrv(serverBuff, &passive_server_port);
							proxy_data_socket = proxyDataSocket(); //开启proxy_data_socket、bind（）、listen操作
							FD_SET(proxy_data_socket, &master_set);//将proxy_data_socket加入master_set集合
                            sprintf(serverBuff,"227 Entering Passive Mode (192,168,56,101,%d,%d)\n", proxy_data_port >>8, proxy_data_port&0xff);
							//send 227 with proxy server port information to the client
                        }
                        printf("%s\n",serverBuff);
                        if(write(accept_cmd_socket, serverBuff, BUFFSIZE)<0)
                            printf("write to accept_cmd_socket error\n");
                    }	
                }
                
                if (i == proxy_data_socket) {
                    printf("proxy_data_socket\n");
					if(mode==1)
					{
					connect_data_socket = connectDataSocket(); 
					FD_SET(connect_data_socket, &master_set);
                    accept_data_socket = acceptDataSocket(proxy_data_socket);
                    FD_SET(accept_data_socket, &master_set);

					}
					else if(mode==2)
					{
					accept_data_socket = acceptDataSocket(proxy_data_socket);
                    FD_SET(accept_data_socket, &master_set);
					connect_data_socket = connectDataSocket(); 
					FD_SET(connect_data_socket, &master_set);

					}
                    //建立data连接(accept_data_socket、connect_data_socket)

                }
                
                if (i == accept_data_socket) {
                    printf("accept_data_socket\n");
                    if(mode==1)
                    {
						if(event==3)	//STOR in passive mode
						{
							ftpGet(accept_data_socket,1,filename);
							accept_data_socket = -1;
							ftpPut(connect_data_socket,2,filename);
							connect_data_socket = -1;
						}
                    }
                    else if(mode==2)
                    {
                        if(event==1)	//MLSD in active mode
                        {
                            ftpListGet(accept_data_socket,1);
							accept_data_socket = -1;
							ftpListPut(connect_data_socket,2);
							connect_data_socket = -1;

                        }
						else if(event==2)	//RETR in active mode
						{
							FILE *p=fopen(filename,"r"); //check whether in cache
                            if (p==NULL)
                            {
								ftpGet(accept_data_socket,1,filename);
								accept_data_socket = -1;
								ftpPut(connect_data_socket,2,filename);
								connect_data_socket = -1;
							}
							else
							{
								fclose(p);
								close(accept_data_socket);
								FD_CLR(accept_data_socket, &master_set);
								close(proxy_data_socket);
								FD_CLR(proxy_data_socket, &master_set);
								accept_data_socket = -1;
								ftpPut(connect_data_socket,2,filename);
								connect_data_socket = -1;
							}
							
						}
                    }
                    //判断主被动和传输方式（上传、下载）决定如何传输数据
                }
                
                if (i == connect_data_socket) {
					printf("connect_data_socket\n");
                    if(mode==1)
                    {
                         if(event==1)	//MLSD in passive mode
                        {
                            ftpListGet(connect_data_socket,2);
							connect_data_socket = -1;
							ftpListPut(accept_data_socket,1);
							accept_data_socket = -1;
                        }
                        else if(event==2)	//RETR in passive mode
                        {
                            FILE *p=fopen(filename,"r"); //check whether is cache
                            if (p==NULL)
                            {
								ftpGet(connect_data_socket,2,filename);
								connect_data_socket = -1;
								ftpPut(accept_data_socket,1,filename);
								accept_data_socket = -1;
							}
							else{
								fclose(p);
								close(connect_data_socket);
								FD_CLR(connect_data_socket, &master_set);
								connect_data_socket = -1;
								ftpPut(accept_data_socket,2,filename);
								accept_data_socket = -1;
							}
							
                        }
                    }
                    else if(mode==2)
                    {
						if(event==3)	//STOR in passive mode
                        {
							ftpGet(connect_data_socket,2,filename);
							connect_data_socket = -1;
                            ftpPut(accept_data_socket,1,filename);
							accept_data_socket = -1;
                        }
                    }
                    //判断
                    //判断主被动和传输方式（上传、下载）决定如何传输数据
                }
            }
        }
    }
    
    return 0;
}

//listen to proxy_cmd_socket
int proxyCmdSocket()
{
    int sockfd, newsockfd;
    socklen_t clilen;
    char buffer[256];
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
       perror("ERROR opening proxyCmdSocket\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    proxy_cmd_port = rand_local_port();
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(proxy_cmd_port);
    while (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {   
        proxy_cmd_port = rand_local_port();
        serv_addr.sin_port = htons(proxy_cmd_port);
        perror("proxyCmdSocket bind error");
    }
    listen(sockfd,5);
    return sockfd; 
}

//listen to proxyDataSocket
int proxyDataSocket()
{
    int sockfd, newsockfd;
    socklen_t clilen;
    char buffer[256];
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
       perror("ERROR opening proxyDataSocket\n");
    bzero((char *) &client_addr, sizeof(client_addr));
    proxy_data_port = rand_local_port();
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(proxy_data_port);
    while(bind(sockfd, (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0)
            {
                proxy_data_port = rand_local_port();
                client_addr.sin_port = htons(proxy_data_port);
				perror("proxyDataSocket bind error");
            }            
    printf("proxy data socket ready!\n");        
    listen(sockfd,5);
    return sockfd; 
}

//accept from proxy_cmd_socket and create acceptCmdSocket
int acceptCmdSocket(int sockfd)
{
    int newsockfd;
    int temp = sizeof(serv_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&serv_addr, &temp);
    return newsockfd; 
}

//connect to connectCmdSocket
int connectCmdSocket()
{
    struct sockaddr_in destAddr;
    struct hostent *dest;
    int sockfd = socket(AF_INET, SOCK_STREAM , 0);   
    if(sockfd < 0) 
        {
        	perror("ERROR opening connectCmdSocket\n");
        	return -1;
    	}
    char str[100];	
    dest = gethostbyname("192.168.56.1");
    connect_cmd_port = 21;
    destAddr.sin_port = htons(connect_cmd_port);
    destAddr.sin_family = AF_INET; 
    int ret= inet_pton(AF_INET,inet_ntop(AF_INET, dest->h_addr, str, sizeof(str)),&destAddr.sin_addr.s_addr);
    if ( ret <=0 )
	{
        printf("\033[01;33mYour ip address is wrong!\033[0m\n");
        return -1;
    }	    
    ret = connect(sockfd,(struct sockaddr*) &destAddr,sizeof(destAddr));//destAddr: struct servaddr_in
    if(ret < 0)
   	{
       	perror("Data connection is failed!\n");
       	return -1;
   	}
    printf("\033[01;33mConnect successfully!\033[0m\n");
	return sockfd;
}

//accept from proxy_data_socket and create acceptDataSocket
int acceptDataSocket(int sockfd)
{
        int newsockfd;
        int temp = sizeof(client_addr);
		int try = 0;
		newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &temp);
		printf("acceptDataSocket connect successfully\n");
        return newsockfd;
}

//connect to connectDataSocket
int connectDataSocket()
{
    if(mode==1) //passive
    {
        struct sockaddr_in destAddr;
        struct hostent *dest;
        int sockfd = socket(AF_INET, SOCK_STREAM , 0);   
        if(sockfd < 0) 
            {
                perror("ERROR opening connectCmdSocket\n");
                return -1;
            }
        char str[100];	
        dest = gethostbyname("192.168.56.1");
        connect_data_port = passive_server_port;
		printf("passive_server_port:%d\n",passive_server_port);
        destAddr.sin_port = htons(connect_data_port);
        destAddr.sin_family = AF_INET; 
        int ret= inet_pton(AF_INET,inet_ntop(AF_INET, dest->h_addr, str, sizeof(str)),&destAddr.sin_addr.s_addr);
        if ( ret <=0 )
        {
            printf("\033[01;33mYour ip address is wrong!\033[0m\n");
            return -1;
        }	    
        ret = connect(sockfd,(struct sockaddr*) &destAddr,sizeof(destAddr));//destAddr: struct servaddr_in
        if(ret < 0)
        {
            perror("passive connect failed!\n");
            return -1;
        }
        printf("\033[01;33m passive connect to dest successfully!\033[0m\n");
        return sockfd;
    }
    if(mode==2) //active
    {
        struct sockaddr_in destAddr;
        struct hostent *dest;
        int sockfd = socket(AF_INET, SOCK_STREAM , 0);   
        if(sockfd < 0) 
            {
                perror("ERROR opening connectCmdSocket\n");
                return -1;
            }
        char str[100];	
        dest = gethostbyname("192.168.56.1");
        connect_data_port = active_client_port; 
        destAddr.sin_port = htons(connect_data_port);
        destAddr.sin_family = AF_INET; 
        int ret= inet_pton(AF_INET,inet_ntop(AF_INET, dest->h_addr, str, sizeof(str)),&destAddr.sin_addr.s_addr);
        if ( ret <=0 )
        {
            printf("\033[01;33mYour ip address is wrong!\033[0m\n");
            return -1;
        }	    
        ret = connect(sockfd,(struct sockaddr*) &destAddr,sizeof(destAddr));//destAddr: struct servaddr_in
        if(ret < 0)
        {
            perror("active connect  is failed!\n");
            return -1;
        }
        printf("\033[01;33m active connect to dest successfully!\033[0m\n");
        return sockfd;
    }
}

//Randomly generate port
int rand_local_port()	
{
	int local_port;
	srand((unsigned)time(NULL));
	local_port = 20000 + (rand() % 10000);
	return local_port;
}

//Get directory informaion
int	ftpListGet(int sck, int ifProxy)
{
	int nread;
	while(1)
	{
		if ( (nread = recv(sck, listBuff, BUFFSIZE, 0)) < 0)
		{
			printf("\033[01;33mftpListGet error\033[0m\n");
			break;
		}
		else if (nread == 0)
			break;
	}
	if (close(sck) < 0)
		printf("\033[01;33mclose error\033[0m\n");
	FD_CLR(sck, &master_set);
	if(ifProxy==1)
	{
		close(proxy_data_socket);
		FD_CLR(proxy_data_socket, &master_set);
	}
}

//send directory informaion
int ftpListPut(int sck, int ifProxy)
{
	int nwrite;
	if ( (nwrite = send(sck, listBuff, BUFFSIZE,0)) < 0)
		printf("\033[01;33mftpListPut error\033[0m\n");
	printf("put %s\n",listBuff);
	memset(listBuff, 0, BUFFSIZE);
	if (close(sck) < 0)
		printf("\033[01;33mclose error\033[0m\n");
	FD_CLR(sck, &master_set);
	if(ifProxy==1)
	{
		close(proxy_data_socket);
		FD_CLR(proxy_data_socket, &master_set);
	}
}

//get file from input socket
int	ftpGet(int sck, int ifProxy, char *filename)
{
	printf("ftpGet!\n");
	int		nread;
	FILE *p=fopen(filename,"w");
	perror("ftpGet fopen");
	while(1)
	{
		if ( (nread = recv(sck, getBuff, BUFFSIZE, 0)) < 0)
		{
			printf("\033[01;33mrecive error\033[0m\n");
		}
		else if (nread == 0)
		{
			break;
		}
		fwrite(getBuff,sizeof(char),nread,p);
		memset(getBuff,0,BUFFSIZE);
	}
	fclose(p);
	if (close(sck) < 0)
		printf("\033[01;33mclose error\033[0m\n");
	FD_CLR(sck, &master_set);
	if(ifProxy==1)
	{
		close(proxy_data_socket);
		FD_CLR(proxy_data_socket, &master_set);
	}
}

//send file to input socket
int ftpPut (int sck, int ifProxy, char *filename)
{
	printf("ftpPut!\n");
	int r,r2=0,r3=0,r4=0,bsnum=0,brnum=0,wi=1;
	int resvMsgSize;
	char temp[BUFFSIZE];
    FILE *fs,*fc;
    long size,curr=0;
    char *find;
    char buffer[SCREEN_WIDTH+1];
    int perc;
    double per,preper;
	printf("filename:%s\n",filename);
	FILE *fp=fopen(filename,"rb");	
	perror("ftpPut fopen");
	if(fp==NULL)
	{
		printf("Please check your input file name!\n*****************************************************************************\nFTP> ");
		close(sck);
		FD_CLR(sck, &master_set);
		return 0;
	}
		//read data from file
    fseek(fp,0L,SEEK_END);
    size=ftell(fp);
	rewind(fp);
	while(1)
	{
		memset(putBuff,0,BUFFSIZE);
		r=fread(putBuff,sizeof(char),BUFFSIZE,fp);
		if(r==0)
			break;
		brnum=brnum+r;
		r3=r;
		do
		{
			r2=send(sck,putBuff,r3,0);
			curr=ftell(fp);
        	per=(double)curr*SCREEN_WIDTH/size;
        	for(perc=0;perc<SCREEN_WIDTH;perc++)
        	{
           		if(perc<=(int)per)
               		buffer[perc]='=';
            	else
               		buffer[perc]=' ';
        	}
			if(r2<0){
				printf("send file error!");
				close(sck);
				FD_CLR(sck, &master_set);
				return -1;
			}
		bsnum=bsnum+r2;
		r3=r3-r2;
		}while(brnum>bsnum);//split send file
	}
	fclose(fp);
    printf("send file %s:",filename);
	if(close(sck)<0)
		printf("\033[01;33mclose error\033[0m\n");
	FD_CLR(sck, &master_set);
	if(ifProxy==1)
	{
		close(proxy_data_socket);
		FD_CLR(proxy_data_socket, &master_set);
	}
}


//Extract IP and port
void strtosrv(char *str, int *transferPortInt)
{
	int addr[6];
	char temp[100];
	if(mode == 1)
		sscanf(str, "%*[^(](%d,%d,%d,%d,%d,%d)", &addr[0],&addr[1],&addr[2],&addr[3],&addr[4],&addr[5]);
	else
		sscanf(str, "PORT %d,%d,%d,%d,%d,%d", &addr[0],&addr[1],&addr[2],&addr[3],&addr[4],&addr[5]);
    *transferPortInt = 256*addr[4]+addr[5];	//Get the port
}

