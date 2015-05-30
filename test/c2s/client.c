#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "mw/client.h"

int main(int argc, char ** argv)
{
    int port;
    int sock = -1;
    struct sockaddr_in address;
    struct hostent * host;
    int len;
    static char exit[] = "exit";
    char input[20];

    /* checking commandline parameter */
    if (argc != 3) {
        printf("usage: %s hostname port\n", argv[0]);
        return -1;
    }

    /* obtain port number */
    if (sscanf(argv[2], "%d", &port) <= 0) {
        fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
        return -2;
    }


    /* create socket */
    sock = mw_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0) {
        fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
        return -3;
    }

    /* connect to server */
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    host = gethostbyname(argv[1]);
    if (!host) {
        fprintf(stderr, "%s: error: unknown host %s\n", argv[0], argv[1]);
        return -4;
    }
    memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
    if (mw_connect(sock, (struct sockaddr *)&address, sizeof(address))) {
        fprintf(stderr, "%s: error: cannot connect to host %s\n", argv[0], argv[1]);
        return -5;
    }


    while (1) {
        printf(">");
        memset(input, 0, 20);
        scanf("%20s",input);
        len = strlen(input);
        if (mw_send(sock, input, len, 0) == -1) {
            printf("send data error\n");
            break;
        }
        if (len == 4 && !strcmp(input, exit))
            break;
    }

    /* close socket */
    mw_close(sock);

    return 0;
}
