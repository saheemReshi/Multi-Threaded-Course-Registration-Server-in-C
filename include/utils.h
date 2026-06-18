#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>

typedef struct
{
	int fd;
	size_t sz;
	char *buf;
	char **ln;
	int n;
} File;

ssize_t sendString(int sockfd, const char *buf);
ssize_t recvString(int sockfd, char *buf, size_t maxlen);

int fileLock(int fd, int lock_type);
int fileUnlock(int fd);

#endif 
