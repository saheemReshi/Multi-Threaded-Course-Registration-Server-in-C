#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../include/common.h"
#include "../include/utils.h"
#include <signal.h>

int sockfd = -1;  // Make socket global so the handler can close it

void handle_sigint(int sig) {
    printf("\n[Client] Caught signal %d, closing connection.\n", sig);
    if (sockfd >= 0)
        close(sockfd);
    exit(0);
}

static int is_prompt_line(const char *s)
{
    size_t i = strlen(s);
    while (i && (s[i-1]==' ' || s[i-1]=='\t' || s[i-1]=='\r' || s[i-1]=='\n'))
        --i;                               /* skip trailing white-space      */
    return i && (s[i-1]==':' || s[i-1]=='>');
}

int main(void)
{
    signal(SIGINT, handle_sigint);
    struct sockaddr_in servaddr;
    char sendbuf[MAX_LINE], recvbuf[MAX_LINE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(PORT);
    servaddr.sin_addr.s_addr = htonl(0x7f000001);  //Hardcoded IP address (localhost)        

    if (connect(sockfd,(void*)&servaddr,sizeof servaddr) < 0){
        perror("connect");  exit(1);
    }

    
    while (1) {
        ssize_t n = recvString(sockfd, recvbuf, sizeof recvbuf);
        if (n <= 0) break;
        fputs(recvbuf, stdout);
        // printf(recvbuf);
        fflush(stdout);
        if (is_prompt_line(recvbuf)) {      
            if (fgets(sendbuf, sizeof sendbuf, stdin) == NULL) break;
            sendString(sockfd, sendbuf);
        }
    }
    close(sockfd);
    return 0;
}
