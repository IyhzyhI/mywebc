/*************************************************************************
    > File Name: client.c
    > Author: 雨后紫阳花
    > Mail: yh2628477129@outlook.com 
    > Created Time: Tue 02 Jul 2024 01:26:11 PM UTC
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>


void read_cb(struct bufferevent * bev , void * ba)
{
	char a[1024];
	memset(a,-1,sizeof(a));
	bufferevent_read(bev,a,sizeof(a));
	printf("%s\n",a);
}

void write_cb(struct bufferevent * bev , void * ba)
{
}

void event_cb(struct bufferevent * bev , short event , void * ba)
{

	struct event_base * bas = (struct event_base *)ba;

	if(event & BEV_EVENT_EOF)
	{

		printf("服务器中断\n");
	}
	else if(event & BEV_EVENT_ERROR)
	{

		printf("服务器错误\n");
	}
	else if(event & BEV_EVENT_CONNECTED)
	{

		printf("连接成功\n");
		return;
	}
	event_base_loopbreak(bas);
}

	
void send_cb(evutil_socket_t fd, short event, void* ba)
{
	char o[1024];
	memset(o,0,sizeof(o));
	read(fd,o,sizeof(o));
	struct bufferevent* bev = (struct bufferevent*)ba;
	bufferevent_write(bev,o,sizeof(o));

}


int main()
{
	struct event_base * base = event_base_new();
	struct bufferevent * bev = bufferevent_socket_new(base,-1,BEV_OPT_CLOSE_ON_FREE);
	struct sockaddr_in add;

	add.sin_family = AF_INET;
	add.sin_port = htons(5095);
 	inet_pton(AF_INET,"192.168.72.11",&add.sin_addr.s_addr);
	
	bufferevent_setcb(bev,read_cb,write_cb,event_cb,base);
	bufferevent_socket_connect(bev,(struct sockaddr *)&add,sizeof(add));
	bufferevent_enable(bev,EV_READ|EV_WRITE);
	
	struct event* ev = event_new(base, STDIN_FILENO, EV_READ|EV_PERSIST, send_cb, bev);
	event_add(ev,NULL);
	
	event_base_dispatch(base);
	bufferevent_free(bev);
	event_base_free(base);


	return 0;
}
