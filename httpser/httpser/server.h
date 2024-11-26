#ifndef SERVER_H
#define SERVER_H
// 服务器业务逻辑
// epoll 事件驱动

int initListenFd(int port);

int epollRun(int port);

int acceptConn(int lfd, int epfd);

int recvHttpRequest(int cfd, int epfd);

int parseRequestline(int cfd, const char* reqline);

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int len);

int sendFile(int cfd, const char* filename);

int sendDir(int cfd, const char* dirname);

int disconnect(int cfd, int epfd);

const char* getFileType(const char* name);

int hexit(char c);

void decodeMsg(char* to, char* from);

#endif // 

