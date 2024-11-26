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
	//�����׽���
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1) {
		perror("socket error");
		return -1;
	}
	//���õ�ַ����
	int optval = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
		perror("setsockopt error");
		close(listenfd);
		return -1;
	}
	//�󶨵�ַ
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
	//����
	if (listen(listenfd, 128) == -1) {
		perror("listen error");
		close(listenfd);
		return -1;
	}	
	return listenfd;
}

int epollRun(int port)
{
	//���������׽���
	int lfd = initListenFd(port);
	if (lfd == -1) {
		return -1;
	}
	//����epoll���
	int epfd = epoll_create(1024);
	if (epfd == -1) {
		perror("epoll_create error");
		close(lfd);
		return -1;
	}
	//��Ӽ����׽��ֵ�epoll���
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (ret == -1) {
		perror("epoll_ctl error");
		return -1;
	}
	
	//����epoll�¼�����
	struct epoll_event evs[1024];
	int size = sizeof(evs)/sizeof(evs[0]);
	int flag = 0;  //�Ƿ����ļ������仯�ı�־
	//ѭ���ȴ��¼�
	while (1) 
	{
		if (flag) {
			break;
		}
		int num = epoll_wait(epfd, evs, size, -1);
		//�����¼�
		for (int i = 0; i < num; i++)
		{
			int fd = evs[i].data.fd;
			if (fd == lfd)
			{
				//��������׽����¼�
				int ret = acceptConn(lfd, epfd);
				printf("accept a connection\n");
				if (ret == -1) {
					int flag = 1;
					break;
				}
				
			}
			else
			{
				//���������׽����¼�
				printf("fd:%d\n", fd);
				int ret = recvHttpRequest(fd, epfd);
			}
		}
	}
	
	return 0;
}

int acceptConn(int lfd, int epfd)
{
	//��������
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);
	int cfd = accept(lfd, (struct sockaddr*)&client_addr, &len);
	if ( cfd == -1) {
		perror("accept error");
		return -1;
	}
	//���÷�����
	int flags = fcntl(cfd, F_GETFL);
	flags |= O_NONBLOCK;
	int ret = fcntl(cfd, F_SETFL, flags);
	if (ret == -1) {
		perror("fcntl error");
		close(cfd);
		return -1;
	}
	
	//��ӵ�epoll���
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
	char tmp[1024];  //���ջ�����
    char buf[4096];  //��ȡ������
	int len, total = 0;	 //��ȡ����, �ܳ���
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
		//������ͷ�ӽ��յ��������ó���
		//http����ͷ�Ľ�����־��\r\n
		char *pt = strstr(buf, "\r\n");
		int reqlen = pt - buf;  //����ͷ�ĳ���
		buf[reqlen] = '\0';  //������ͷ�ַ�����'\0'��β
		//����������
		printf("recv request head\n");
		parseRequestline(cfd, buf);
		return 0;
	}
	else if (len == 0)
	{ 

		//�ͻ��˹ر�����
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
	//����������
	//�����и�ʽ��method uri version
	//���磺GET /index.html HTTP/1.1
	// ��������, �õ�method, uri, version

	char method[10], path[100], version[10];
	int n = sscanf(reqline, "%[^ ] %[^ ] %[^ ]", method, path, version);
	if (n != 3)
	{
		printf("parse request line error\n");
		//�����и�ʽ����
		return -1;
	}


	//��������
	if (strcasecmp(method, "GET") != 0)
	{
		printf("method not support\n");
		//��֧�ֵķ���
		return -1;
	}
		//����GET����
		//�ж��ǲ���Ŀ¼
	char* file = NULL;
	decodeMsg(path, path); //����url,��ֹ·����������,��ֹ�����·�����
	if(strcmp(path, "/") == 0)
	{
			//����Ŀ¼����
		file = "./";
	}
	else
	{
			//�����ļ�����
		file = path + 1;  //  ȥ��path��һ���ַ� '/'   ./hello/world.html => hello/world.html,������ǰ���.
	}
		
		//�����ж�
		struct stat st;
		int ret = stat(file, &st);
		if (ret == -1)
		{
			//���͸��ͻ���404����ҳ��
			//�ļ�������
			sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
			sendFile(cfd, "404.html");
			return -1;
		}
		if (S_ISDIR(st.st_mode))
		{
			sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
			sendDir(cfd, file);
			//��Ŀ¼
			//��Ŀ¼�������͸��ͻ���
		}
		else
		{
			sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
			sendFile(cfd, file);
			//���ļ�
			//�����ļ����ͻ���
		}

	return 0;
}

int disconnect(int cfd, int epfd)
{
	//��epoll�����ɾ��
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (ret == -1) {
		close(cfd);
		perror("epoll_ctl error");
		return -1;
	}
	//�ر�����
	close(cfd);

	return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int len)
{
	//������Ϣͷ

	// ����HTTP/1.1 200 OK
	char buf[4096];
	// http/1.1 200 ok
	sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
	// content-type: text/html
	sprintf(buf+strlen(buf), "Content-Type: %s\r\n", type);
	// content-length: 12345
	sprintf(buf+strlen(buf), "Content-Length: %d\r\n\r\n", len);
	// ���Ϳ���
	//������Ϣͷ
	send(cfd, buf, strlen(buf), 0);
	return 0;
}

int sendFile(int cfd, const char* filename)
{
	//�����ļ�
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
			// ���͵����ݲ���̫��, ��ֹ�ͻ��˽��ջ��������
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
	//����Ŀ¼,�����͸��ͻ���
	//����Ŀ¼
	struct dirent **namelist;
	char buf[4096];
	//����Ŀ¼
	sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirname);

	//����Ŀ¼
	int num = scandir(dirname, &namelist, NULL, alphasort);
	if (num == -1) {
		perror("scandir error");
		return -1;
	}
	//Ŀ¼
	for (int i = 0; i < num; i++)
	{
		char* name = namelist[i]->d_name;
		char subpath[1024];
		sprintf(subpath, "%s/%s", dirname, name);
		struct stat st;
		stat(subpath, &st);

		if(S_ISDIR(st.st_mode))
		{
			//��Ŀ¼
			sprintf(buf+strlen(buf), 
				"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		else
		{
			//���ļ�
			sprintf(buf+strlen(buf), 
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				name, name, (long)st.st_size);
		}
		//��������,��ֹbuf���
		send(cfd, buf, strlen(buf)+1, 0);

		//���buf
		memset(buf, 0, sizeof(buf));

		//�ͷ���Դ
		free(namelist[i]);
	}

	//���ͽ������
	strcat(buf, "</table></body></html>");
	send(cfd, buf, strlen(buf)+1, 0);
	//�ͷ�namelist��Դ
	free(namelist);

	return 0;
}

const char* getFileType(const char* name)
{	
	//��ȡ�ļ�����
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
	//������Ϣ
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
