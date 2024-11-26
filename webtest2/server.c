/*************************************************************************
  > File Name: server.c
  > Author: 雨后紫阳花
  > Mail: yh2628477129@outlook.com 
  > Created Time: Thu 27 Jun 2024 05:38:05 PM UTC
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>


//  获取监听文件描述符
int getsoc(int port)
{
	int sfd = socket(AF_INET,SOCK_STREAM,0);

	int po = 1;
	int len = sizeof(po);
	setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&po,len);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	bind(sfd,(struct sockaddr *)&addr,sizeof(addr));

	listen(sfd,128);
	return sfd;
}


int work(int i)
{
	
		char a[1024];	
		memset(a,0,sizeof(a));
		int e = recv(i,a,sizeof(a),0);
		if(e == 0)
		{
			perror("连接中断\n");
			return 0;
		}
		else if(e < 0)
		{
			perror("读取错误\n");
			return -1;
		}
		else
		{
			printf("%s\n",a);
			char b[1024] = "IO多路select服务器\n";
			send(i,b,sizeof(b),0);
		}
		return 1;
}



int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int len = sizeof(addr);
	int sfd = getsoc(5092);
	fd_set re,rcp;
	FD_ZERO(&re);
	FD_SET(sfd,&re);
	int usfd = sfd;

	while(1)
	{
		rcp = re;
		select(usfd+1,&rcp,NULL,NULL,NULL);
		for(int i = sfd;i <= usfd;++i)
		{
			if(i == sfd && FD_ISSET(sfd,&rcp))
			{
				int cfd = accept(sfd,(struct sockaddr *)&addr,&len);
				FD_SET(cfd,&re);
				usfd = usfd < cfd ?cfd : usfd;
			}
			else if(FD_ISSET(i,&rcp))
			{
				int j = work(i);
				if(j == -1)
				{
					break;
				}
				else if(j == 0)
				{
					close(i);
				}


			}
		}
	}
	close(sfd);
	return 0;
}
