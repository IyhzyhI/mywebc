/*************************************************************************
  > File Name: test.c
  > Author: 雨后紫阳花
  > Mail: yh2628477129@outlook.com 
  > Created Time: Sat 29 Jun 2024 09:19:11 PM UTC
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/epoll.h>

//  sfd 获取函数

int getsoc(int port)
{
	int sfd = socket(AF_INET,SOCK_STREAM,0);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	bind(sfd,(struct sockaddr *)&addr,sizeof(addr));

	listen(sfd,128);
	return sfd;
}







int main(int argc,char *argv[])
{
	struct sockaddr_in addrr;
	int len = sizeof(addrr);

	int sfd = getsoc(5093);
	int epfd = epoll_create(100);

	struct epoll_event eevt;
	eevt.events = EPOLLIN;
	eevt.data.fd = sfd;
	epoll_ctl(epfd,EPOLL_CTL_ADD,sfd,&eevt);

	struct epoll_event ev[1024];
	int lene = sizeof(ev)/sizeof(ev[0]);
	while(1)
	{
		int num = epoll_wait(epfd,ev,lene,-1);
		for(int i = 0;i < num;++i)
		{
			int fd = ev[i].data.fd;
			if(fd == sfd)
			{
				int cfd = accept(fd,(struct sockaddr *)&addrr,&len);
				eevt.events = EPOLLIN;
				eevt.data.fd = cfd;
				epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&eevt);
			}
			else
			{
					sleep(1);
				char a[1024];
				memset(a,0,sizeof(a));
				int e = recv(fd,a,sizeof(a),0);

				if(e == 0)
				{
					epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
					close(i);
					printf("程序退出\n");
				}
				else if(e < 0)
				{
					perror("cfd 错误");
					_exit(0);
				}
				else
				{
					printf("%s\n",a);
					char b[1024] = "IO多路epoll服务器\n";
					if (send(fd, b, sizeof(b), 0) < 0) {
						perror("send 错误");

					}
				}
			}

     	}	}

		close(sfd);
		return 0;
	}
