#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char ** argv)
{
    int port;
    int sock = -1;
    ssize_t send_return;
    struct sockaddr_in address;
    struct hostent * host;
    int len;
    static char exit[] = "exit";
    char input[20];
    int cn;

    /* checking commandline parameter */
    if (argc != 4)
    {
        printf("usage: %s hostname port cn\n", argv[0]);
        return -1;
    }

    /* obtain port number */
    if (sscanf(argv[2], "%d", &port) <= 0)
    {
        fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
        return -2;
    }

    if (sscanf(argv[3], "%d", &cn) <= 0)
    {
        fprintf(stderr, "%s: error: wrong parameter: cn\n", argv[0]);
        return -3;
    }

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0)
    {
        fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
        return -3;
    }

    /* connect to server */
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    host = gethostbyname(argv[1]);
    if (!host)
    {
        fprintf(stderr, "%s: error: unknown host %s\n", argv[0], argv[1]);
        return -4;
    }
    memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)))
    {
        fprintf(stderr, "%s: error: cannot connect to host %s\n", argv[0], argv[1]);
        return -5;
    }

    if (send(sock, &cn, sizeof(int), MSG_NOSIGNAL) == -1) {
        printf("send cn error\n");
    }

    while (1) {
        printf(">");
        memset(input, 0, 20);
        scanf("%20s",input);
        len = strlen(input);
        if (send(sock, &len, sizeof(int), MSG_NOSIGNAL) == -1) {
            printf("send length error\n");
            break;
        }
        if (send(sock, input, len, MSG_NOSIGNAL) == -1) {
            printf("send data error\n");
            break;
        }
        if (len == 4 && !strcmp(input, exit))
            break;
    }

    /* close socket */
    close(sock);

    return 0;
}
