#include <stdio.h>
#include "server.h"


int main(int argc, char* argv[])
{

	if (argc < 3) {
		printf("Usage: %s <port>\n", argv[0]);
		return 1;
	}
	int l = chdir(argv[2]);
	if (l != 0) {
		printf("Invalid directory: %s\n", argv[2]);
		return 1;
	}
	int port = atoi(argv[1]);
	if (port <= 0 || port > 65535) {
		printf("Invalid port number: %d\n", port);
		return 1;
	}
	epollRun(port);
	return 0;
}