/*************************************************************************
  > File Name: server.c
  > Author: 雨后紫阳花
  > Mail: yh2628477129@outlook.com 
  > Created Time: Wed 26 Jun 2024 05:37:21 PM UTC
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <sys/wait.h>

//  获取socket通信文件
int getsoc(int port)
{
	int sfd = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	inet_pton(AF_INET,"192.168.72.11",&sockaddr.sin_addr.s_addr);
	int len = sizeof(sockaddr);
	connect(sfd,(struct sockaddr *)&sockaddr,len);
	return sfd;
}



//  子进程读取数据函数
void child(int sfd)
{
	if(sfd < 0)
	{
		perror("sfd error");
	}
	else
	{
		while(1)
		{
			char a[1024] = "多线程客户端";
			sleep(1);
			send(sfd,a,sizeof(a),0);
			char b[1024];
			recv(sfd,b,sizeof(b),0);
			printf("%s\n",b);
		}
	}
}


//  多进程高并发通信
int main(int argc,char *argv[])
{
	int sfd = getsoc(5091);
	while(1)
	{
		child(sfd);
	}
	close(sfd);
	return 0;
}
