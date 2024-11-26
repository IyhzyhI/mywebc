#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>


//  获取监听描述符
int getsoc(int port)
{
	int sfd = socket(AF_INET,SOCK_STREAM,0);

	int po = 1;
	int le = sizeof(po);
	setsockopt(sfd,SOL_SOCKET,SO_REUSEPORT|SO_REUSEADDR,&po,le);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	bind(sfd,(struct sockaddr*)&addr,sizeof(addr));

	listen(sfd,128);
	return sfd;
}



typedef struct mfile
{
	int cfd;

}mf;


//  线程处理函数

//  线程回复函数
void * rese(void * mfi)
{

	mf * mcfd = (mf *)mfi;
	while(1)
	{
		char re[1024];
		memset(re,0,sizeof(re));
		int e = recv(mcfd->cfd,re,sizeof(re),0);
		if(e == 0)
		{
			fputs("链接断开",stdout);
			break;
		}
		else if(e < 0)
		{
			perror("读取失败");
			break;
		}
		else
		{
			printf("%s\n",re);
			char wr[1024] = "高并发多线程服务器";
			send(mcfd->cfd,wr,sizeof(wr),0);
		}
	}
 	close(mcfd->cfd);	
	pthread_exit(0);
}



mf mfunc[1024];

int main(int argc,char*argv[])
{

	int i;
	mf * p;
	struct sockaddr_in add;
	int addlen = sizeof(add);
	int sfd = getsoc(5091);
	int len = sizeof(mfunc)/sizeof(mf);
	for(i = 0;i < len; ++i)
	{
		mfunc[i].cfd = -1;
	}


	while(1)
	{
		for(i = 0 ; i < len ; i++)
		{
			if(mfunc[i].cfd == -1)
			{
				int cfd  = accept(sfd,(struct sockaddr *)&add,&addlen);
				mfunc[i].cfd = cfd;
				p = &mfunc[i];
				printf("cfdr%d\n",cfd);
				printf("cfdrr%d\n",mfunc[i].cfd);
				break;
			}
		}

		pthread_t pth;
		pthread_create(&pth,NULL,rese,p);	
		pthread_detach(pth);
	}

	return 0;
}
