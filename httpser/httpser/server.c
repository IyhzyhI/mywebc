#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>


int initListenFd(int port)
{
	//创建套接字
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1) {
		perror("socket error");
		return -1;
	}
	//设置地址复用
	int optval = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
		perror("setsockopt error");
		close(listenfd);
		return -1;
	}
	//绑定地址
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if (bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		perror("bind error");
		close(listenfd);
		return -1;
	}
	//监听
	if (listen(listenfd, 128) == -1) {
		perror("listen error");
		close(listenfd);
		return -1;
	}	
	return listenfd;
}

int epollRun(int port)
{
	//创建监听套接字
	int lfd = initListenFd(port);
	if (lfd == -1) {
		return -1;
	}
	//创建epoll句柄
	int epfd = epoll_create(1024);
	if (epfd == -1) {
		perror("epoll_create error");
		close(lfd);
		return -1;
	}
	//添加监听套接字到epoll句柄
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (ret == -1) {
		perror("epoll_ctl error");
		return -1;
	}
	
	//创建epoll事件数组
	struct epoll_event evs[1024];
	int size = sizeof(evs)/sizeof(evs[0]);
	int flag = 0;  //是否有文件发生变化的标志
	//循环等待事件
	while (1) 
	{
		if (flag) {
			break;
		}
		int num = epoll_wait(epfd, evs, size, -1);
		//处理事件
		for (int i = 0; i < num; i++)
		{
			int fd = evs[i].data.fd;
			if (fd == lfd)
			{
				//处理监听套接字事件
				int ret = acceptConn(lfd, epfd);
				printf("accept a connection\n");
				if (ret == -1) {
					int flag = 1;
					break;
				}
				
			}
			else
			{
				//处理其他套接字事件
				printf("fd:%d\n", fd);
				int ret = recvHttpRequest(fd, epfd);
			}
		}
	}
	
	return 0;
}

int acceptConn(int lfd, int epfd)
{
	//接受连接
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);
	int cfd = accept(lfd, (struct sockaddr*)&client_addr, &len);
	if ( cfd == -1) {
		perror("accept error");
		return -1;
	}
	//设置非阻塞
	int flags = fcntl(cfd, F_GETFL);
	flags |= O_NONBLOCK;
	int ret = fcntl(cfd, F_SETFL, flags);
	if (ret == -1) {
		perror("fcntl error");
		close(cfd);
		return -1;
	}
	
	//添加到epoll句柄
	struct epoll_event ev;
	ev.events = EPOLLIN|EPOLLET;
	ev.data.fd = cfd;

	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (ret == -1) {
		perror("epoll_ctl error");
		close(cfd);
		return -1;
	}
	return 0;
}

int recvHttpRequest(int cfd, int epfd)
{
	char tmp[1024];  //接收缓冲区
    char buf[4096];  //读取缓冲区
	int len, total = 0;	 //读取长度, 总长度
	while ((len = recv(cfd, tmp, 1024, 0)) > 0)
	{
		if(total + len < sizeof(buf))
		{
			memcpy(buf + total, tmp, len);
		}
		total += len;
	}
	if (len == -1 && errno == EAGAIN)
	{
		//将请求头从接收的数据中拿出来
		//http请求头的结束标志是\r\n
		char *pt = strstr(buf, "\r\n");
		int reqlen = pt - buf;  //请求头的长度
		buf[reqlen] = '\0';  //将请求头字符串以'\0'结尾
		//解析请求行
		printf("recv request head\n");
		parseRequestline(cfd, buf);
		return 0;
	}
	else if (len == 0)
	{ 

		//客户端关闭连接
		printf("clientclose\n");
		disconnect(cfd, epfd);
	}
	else
	{
		perror("recv error");
		return -1;
	}

	return 0;
}

int parseRequestline(int cfd ,const char* reqline)
{
	//解析请求行
	//请求行格式：method uri version
	//例如：GET /index.html HTTP/1.1
	// 拆开请求行, 得到method, uri, version

	char method[10], path[100], version[10];
	int n = sscanf(reqline, "%[^ ] %[^ ] %[^ ]", method, path, version);
	if (n != 3)
	{
		printf("parse request line error\n");
		//请求行格式错误
		return -1;
	}


	//处理请求
	if (strcasecmp(method, "GET") != 0)
	{
		printf("method not support\n");
		//不支持的方法
		return -1;
	}
		//处理GET请求
		//判断是不是目录
	char* file = NULL;
	decodeMsg(path, path); //解码url,防止路径中有中文,防止解码后路径变短
	if(strcmp(path, "/") == 0)
	{
			//处理目录请求
		file = "./";
	}
	else
	{
			//处理文件请求
		file = path + 1;  //  去掉path第一个字符 '/'   ./hello/world.html => hello/world.html,不用往前面加.
	}
		
		//属性判断
		struct stat st;
		int ret = stat(file, &st);
		if (ret == -1)
		{
			//发送给客户端404错误页面
			//文件不存在
			sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
			sendFile(cfd, "404.html");
			return -1;
		}
		if (S_ISDIR(st.st_mode))
		{
			sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
			sendDir(cfd, file);
			//是目录
			//将目录遍历发送给客户端
		}
		else
		{
			sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
			sendFile(cfd, file);
			//是文件
			//发送文件给客户端
		}

	return 0;
}

int disconnect(int cfd, int epfd)
{
	//从epoll句柄中删除
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (ret == -1) {
		close(cfd);
		perror("epoll_ctl error");
		return -1;
	}
	//关闭连接
	close(cfd);

	return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int len)
{
	//发送消息头

	// 发送HTTP/1.1 200 OK
	char buf[4096];
	// http/1.1 200 ok
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	// content-type: text/html
	sprintf(buf+strlen(buf), "Content-Type: %s\r\n", type);
	// content-length: 12345
	sprintf(buf+strlen(buf), "Content-Length: %d\r\n\r\n", len);
	// 发送空行
	//发送消息头
	send(cfd, buf, strlen(buf), 0);
	return 0;
}

int sendFile(int cfd, const char* filename)
{
	//发送文件
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror("open error");
		return -1;
	}
	while (1)
	{
		char buf[4096] = {0};
		int len = read(fd, buf, sizeof(buf));
		if (len > 0) {
			send(cfd, buf, len, 0);
			// 发送的数据不能太快, 防止客户端接收缓冲区溢出
			usleep(100);
		}
		else if (len == 0) {
			break;
		}
		else {
			perror("read error\n");
			return -1;
		}
	}
	close(fd);
	return 0;
}

int sendDir(int cfd, const char* dirname)
{
	//遍历目录,并发送给客户端
	//发送目录
	struct dirent **namelist;
	char buf[4096];
	//发送目录
	sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirname);

	//遍历目录
	int num = scandir(dirname, &namelist, NULL, alphasort);
	if (num == -1) {
		perror("scandir error");
		return -1;
	}
	//目录
	for (int i = 0; i < num; i++)
	{
		char* name = namelist[i]->d_name;
		char subpath[1024];
		sprintf(subpath, "%s/%s", dirname, name);
		struct stat st;
		stat(subpath, &st);

		if(S_ISDIR(st.st_mode))
		{
			//是目录
			sprintf(buf+strlen(buf), 
				"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		else
		{
			//是文件
			sprintf(buf+strlen(buf), 
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		//发送数据,防止buf溢出
		send(cfd, buf, strlen(buf)+1, 0);

		//清空buf
		memset(buf, 0, sizeof(buf));

		//释放资源
		free(namelist[i]);
	}

	//发送结束标记
	strcat(buf, "</table></body></html>");
	send(cfd, buf, strlen(buf)+1, 0);
	//释放namelist资源
	free(namelist);

	return 0;
}

const char* getFileType(const char* name)
{	
	//获取文件类型
	const char* dot = strrchr(name, '.');
	if (dot == NULL)
	{
		return "text/plain; charset=utf-8";
	}
	else if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm"))
	{
		return "text/html; charset=utf-8";
	}
	else if (strcmp(dot, ".css") == 0)
	{
		return "text/css; charset=utf-8";
	}
	else if (strcmp(dot, ".js") == 0)
	{
		return "text/javascript; charset=utf-8";
	}
	else if (strcmp(dot, ".png") == 0)
	{
		return "image/png; charset=utf-8";
	}
	else if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg"))
	{
		return "image/jpeg; charset=utf-8";
	}
	else if (strcmp(dot, ".gif") == 0)
	{
		return "image/gif; charset=utf-8";
	}
	else if (strcmp(dot, ".ico") == 0)
	{
		return "image/x-icon; charset=utf-8";
	}
	else if (strcmp(dot, ".txt") == 0)
	{
		return "text/plain; charset=utf-8";
	}
	else if (strcmp(dot, ".zip") == 0)
	{
		return "application/zip; charset=utf-8";
	}
	else if (strcmp(dot, ".rar") == 0)
	{
		return "application/rar; charset=utf-8";
	}	
	else if (strcmp(dot, ".pdf") == 0)
	{
		return "application/pdf; charset=utf-8";
	}	
	else if (strcmp(dot, ".swf") == 0)
	{
		return "application/x-shockwave-flash; charset=utf-8";
	}
	else if (strcmp(dot, ".mp3") == 0)
	{
		return "audio/mp3; charset=utf-8";
	}
	else if (strcmp(dot, ".mp4") == 0)
	{
		return "video/mp4; charset=utf-8";
	}
	else if (strcmp(dot, ".avi") == 0)
	{
		return "video/avi; charset=utf-8";
	}	
	else if (strcmp(dot, ".wav") == 0)
	{
		return "audio/wav; charset=utf-8";
	}
	else if (strcmp(dot, ".flv") == 0)
	{
		return "video/x-flv; charset=utf-8";
	}	
	else if (strcmp(dot, ".xml") == 0)
	{
		return "text/xml; charset=utf-8";
	}	
	else if (strcmp(dot, ".json") == 0)
	{
		return "application/json; charset=utf-8";
	}	
	else if (strcmp(dot, ".doc") == 0)
	{
		return "application/msword; charset=utf-8";
	}
	else if (strcmp(dot, ".docx") == 0)
	{
		return "application/vnd.openxmlformats-officedocument.wordprocessingml.document; charset=utf-8";
	}
	else if (strcmp(dot, ".xls") == 0)
	{
		return "application/vnd.ms-excel; charset=utf-8";
	}
	else if (strcmp(dot, ".ppt") == 0)
	{
		return "application/vnd.ms-powerpoint; charset=utf-8";
	}
	else if (strcmp(dot, ".c") == 0)
	{
		return "text/plain; charset=utf-8";
	}
	else if (strcmp(dot, ".cpp") == 0)
	{
		return "text/plain; charset=utf-8";
	}
	else if (strcmp(dot, ".h") == 0)
	{
		return "text/plain; charset=utf-8";
	}
	else
	{
		return "text/plain; charset=utf-8";
	}
	
}		

int hexit(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}

    return 0;
}

void decodeMsg(char* to, char* from)
{
	//解码消息
	for (; *from != '\0'; ++to, ++from)
	{
		if(from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			*to = (hexit(from[1]) << 4) + hexit(from[2]);
			from += 2;
		}
		else
		{
			*to = *from;
		}
	}
	*to = '\0';
}
