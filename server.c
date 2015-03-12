#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <string.h>

#define MAX_CN 30

typedef struct
{
    int sock;
    struct sockaddr address;
    int addr_len;
    int cn;
} connection_t;

void * process(void * ptr)
{
    char * buffer;
    char exit[] = "exit";
    int len, exit_code = 0;
    connection_t * conn;
    long addr = 0;

    if (!ptr) pthread_exit(0);
    conn = (connection_t *)ptr;

    while (1) {
        /* read length of message */
        read(conn->sock, &len, sizeof(int));
        if (len > 0) {
            addr = (long)((struct sockaddr_in *)&conn->address)->sin_addr.s_addr;
            buffer = (char *)malloc((len+1)*sizeof(char));
            buffer[len] = 0;

            /* read message */
            read(conn->sock, buffer, len);

            /* print message */
            printf("%d::%d.%d.%d.%d: %s\n",
                conn->cn,
                (int)((addr      ) & 0xff),
                (int)((addr >>  8) & 0xff),
                (int)((addr >> 16) & 0xff),
                (int)((addr >> 24) & 0xff),
                buffer);
            if (len == 4 && !strcmp(buffer, exit))
                exit_code = 1;
            free(buffer);
        }
        if (exit_code == 1) {
            break;
        }
    }

    /* close socket and clean up */
    close(conn->sock);
    free(conn);
    printf("pthread exit\n");
    pthread_exit(0);
}

int main(int argc, char ** argv)
{
    int sock = -1;
    struct sockaddr_in address;
    int port;
    connection_t * connection;
    pthread_t thread;
    int cn_table[MAX_CN], i, cur = 0;
    memset(&cn_table, 0, sizeof(cn_table));

    /* check for command line arguments */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        return -1;
    }

    /* obtain port number */
    if (sscanf(argv[1], "%d", &port) <= 0)
    {
        fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
        return -2;
    }

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0)
    {
        fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
        return -3;
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
    {
        fprintf(stderr, "%s: error: cannot bind socket to port %d\n", argv[0], port);
        return -4;
    }

    /* listen on port */
    if (listen(sock, 5) < 0)
    {
        fprintf(stderr, "%s: error: cannot listen on port\n", argv[0]);
        return -5;
    }

    printf("%s: ready and listening\n", argv[0]);

    while (1)
    {
        /* accept incoming connections */
        connection = (connection_t *)malloc(sizeof(connection_t));
        connection->sock = accept(sock, &connection->address, &connection->addr_len);
        if (connection->sock <= 0)
        {
            free(connection);
        }
        else
        {
            read(connection->sock, &connection->cn, sizeof(int));
            for (i = 0; i < cur; i++) {
                if (cn_table[i] && cn_table[i] == connection->cn)
                    break;
            }
            if (i == cur) {
                cn_table[cur++] = connection->cn;
            }

            /* start a new thread but do not wait for it */
            pthread_create(&thread, 0, process, (void *)connection);
            pthread_detach(thread);
        }
    }

    return 0;
}
