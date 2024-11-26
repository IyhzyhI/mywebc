/*************************************************************************
    > File Name: test.c
    > Author: 雨后紫阳花
    > Mail: yh2628477129@outlook.com 
    > Created Time: Sun 24 Nov 2024 06:25:21 AM UTC
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>

void read_cb(struct bufferevent* bev, void* ba)
{
	char i[1024];
	memset(i,-1,sizeof(i));
	bufferevent_read(bev,i,sizeof(i));
	printf("%s\n",i);
}

void write_cb(struct bufferevent* bev, void* ba)
{
}

void event_cb(struct bufferevent* bev, short event, void* ba)
{
	if(event & BEV_EVENT_ERROR)
	{
		perror("连接失败\n");
	}
	else if(event & BEV_EVENT_EOF)
	{
		printf("连接断开\n");
	}
	struct event_base* base = (struct event_base*)ba;
	event_base_loopbreak(base);
}

void send_cb(evutil_socket_t fd, short event, void* buff)
{
	char o[1024];
	memset(o,0,sizeof(o));
	read(fd,o,sizeof(o));
	struct bufferevent* bev = (struct bufferevent*)buff;
	bufferevent_write(bev,o,sizeof(o));
}

void listener_cb(struct evconnlistener* lev, evutil_socket_t est, struct sockaddr* addr, int len, void* ba)
{
	struct event_base* base = (struct event_base*)ba;
	struct bufferevent* bev = bufferevent_socket_new(ba, est, BEV_OPT_CLOSE_ON_FREE);
	struct event* ev = event_new(base, STDIN_FILENO, EV_READ|EV_PERSIST, send_cb, bev);
	event_add(ev,NULL);
	bufferevent_setcb(bev, read_cb, write_cb, event_cb, base);
	bufferevent_enable(bev,EV_READ|EV_WRITE|EV_ET);

}

int main(int argc,char* argv[])
{
	
	struct event_base* base = event_base_new();

	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(5095);
	saddr.sin_addr.s_addr = INADDR_ANY;

	struct evconnlistener* lev = evconnlistener_new_bind(
			base, listener_cb, base,
			LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
			(struct sockaddr*)&saddr, sizeof(saddr)
			);
	
	event_base_dispatch(base);
	evconnlistener_free(lev);
	event_base_free(base);

	return 0;
}
