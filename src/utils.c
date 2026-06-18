#include "utils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>


int fileLock(int fd, int lock_type){
    struct flock fl = {0};
    fl.l_type = lock_type;   // F_RDLCK or F_WRLCK
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;            // entire file
    return fcntl(fd, F_SETLKW, &fl);
}

ssize_t sendString(int sockfd, const char *buf){
    size_t len = strlen(buf);
    return send(sockfd, buf, len, 0);
}

int fileUnlock(int fd){
    struct flock fl = {0};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return fcntl(fd, F_SETLK, &fl);
}

ssize_t recvString(int sockfd, char *buf, size_t maxlen){
    ssize_t n, rc;
    char c;
    for (n = 0; n < maxlen-1; ){
        if ((rc = recv(sockfd, &c, 1, 0)) == 1){
            buf[n++] = c;//reading char by char to avoid reading future data too early- TCP is stream based
            if (c=='\n') break;
        } else if(rc==0){
            break;
        } else return -1;
    }
    buf[n] = '\0';
    return n;
}

