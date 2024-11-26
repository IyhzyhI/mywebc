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
#include <string.h>



//  获取socket通信文件
int getsoc(int port)
{
	int sfd = socket(AF_INET,SOCK_STREAM,0);
	int opt = 1;
	setsockopt(sfd,SOL_SOCKET,SO_REUSEPORT|SO_REUSEADDR,&opt,sizeof(opt));
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	bind(sfd,(struct sockaddr *)&sockaddr,sizeof(sockaddr));
	listen(sfd,128);
	return sfd;
}


//  子进程读取数据函数
void child(int cfd)
{
		while(1)
		{
			char a[1024];
			memset(a,0,sizeof(a));
			int e = recv(cfd,a,sizeof(a),0);
			if(e == 0)
			{
				printf("客户端断开链接");
				break;
			}
			else if(e < 0)
			{
				perror("server child pid error");
				break;
			}
			else
			{	
				printf("%s\n",a);
				char b[1024] = "高并发多进程服务器";
        		send(cfd,b,sizeof(b),0);
			}
		}
}


// 父进程回收函数
void recycle(int sfd,siginfo_t *sig,void *see)
{
	while(1)
	{
		int a;
		pid_t pid = waitpid(-1,&a,WNOHANG);
		if(pid > 0)
		{
			printf("回收成功,pid: %d",pid);
		}
		else
		{
			break;
		}
	}
}

//  多进程高并发通信
int main(int argc,char *argv[])
{

	int sfd = getsoc(5090);

	struct sigaction sig;
	sigemptyset(&sig.sa_mask);
	sig.sa_f lags = SA_SIGINFO;
	sig.sa_sigaction = recycle;
	sigaction(SIGCHLD,&sig,NULL);

	struct sockaddr_in socka;
	int le = sizeof(socka);

	while(1)
	{
		int cfd = accept(sfd,(struct sockaddr *)&socka,&le);
		if(cfd == -1)
		{
			perror("链接失败");
			continue;
		}
		pid_t pid = fork();
		if(pid == 0)
		{
			close(sfd);
			child(cfd);
			_exit(0);
		}
		else if(pid > 0)
		{
			close(cfd);
		}

	}
	close(sfd);
	return 0;
}
