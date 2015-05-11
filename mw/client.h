#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <pthread.h>
#include "uthash.h"

struct client {
    int fake_fd;        //key
    int fd;
    int client_id;
    int is_droped;

    struct sockaddr *sock_addr;
    socklen_t *sock_len;
    int domain, type, protocol;

    time_t time;

    UT_hash_handle hh;
};

//注意：一个hash表只能有一个key
struct id_fd {
    int id;             //key
    int fd;
    UT_hash_handle hh;
};

typedef struct client client_t;
typedef struct id_fd id_fd_t;

client_t *cli_fd_table = NULL;
id_fd_t *id_fd_table = NULL;

int heart_on = 0;



void mw_init_recv_heart(void *p)
{
    struct sockaddr_in address;
    struct sockaddr_in peer_addr;
    socklen_t peer_len;
    int port = 8992;
    int sock;

    char buf[20];
    int client_id;
    client_t *current, *tmp_item;
    id_fd_t *id_key;
    time_t last_time;
    time_t cur_time;
    double passed_time;

    //use for timeout
    struct timeval timeout;
    timeout.tv_sec = 6;
    timeout.tv_usec = 0;


    /* create socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock <= 0) {
        fprintf(stderr, "error: cannot create socket\n");
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr, "error: cannot bind socket\n");
    }

    //设置超时
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        fprintf(stderr, "setsockopt failed\n");

    printf("heart start\n");

    time(&last_time);
    while (1) {
        //接收心跳包 ID + 'H'
        memset(buf, 0, 20);
        recvfrom(sock, buf, 20, 0,
                (struct sockaddr *)&peer_addr, &peer_len);
        if (buf[sizeof(int)] == 'H') {
            buf[sizeof(int)] = 0;
            memcpy(&client_id, buf, sizeof(int));
            HASH_FIND_INT(id_fd_table, &client_id, id_key);
            if (id_key)
                HASH_FIND_INT(cli_fd_table, &(id_key->fd), current);
            if (id_key != NULL && current != NULL) {
                time(&(current->time));
            }
        }

        time(&cur_time);
        passed_time = difftime(cur_time, last_time);
        printf("debug::heart: passed %f\n", passed_time);
        if (passed_time >= 6) {
            printf("start iter\n");
            //遍历hash表，检查每个客户端的连接是否存活
            HASH_ITER(hh, cli_fd_table, current, tmp_item) {
                if (current->time != 0 && \
                        difftime(cur_time, current->time) > 8) {
                    printf("id %d is droped\n", current->client_id);
                    current->is_droped = 1;
                    shutdown(current->fd, 2);
                }
            }
            last_time = cur_time;
        }
    }


}




int mw_socket(int domain, int type, int protocol)
{
    pthread_t thread;

    int fd;
    client_t *current;
    id_fd_t *id_key;
    void *p = NULL;

    //心跳包开始工作
    if (!heart_on) {
        pthread_create(&thread, 0, (void *)mw_init_recv_heart, p);
        pthread_detach(thread);
        heart_on = 1;
    }

    fd = socket(domain, type, protocol);

    srand(time(NULL));
    current = (client_t *)malloc(sizeof(client_t));
    current->fake_fd = rand() % 10000000 + 10000000;
    current->fd = fd;
    current->client_id = rand();
    current->is_droped = 0;
    current->time = 0;

    current->domain = domain;
    current->type = type;
    current->protocol = protocol;

    id_key = (id_fd_t *)malloc(sizeof(id_fd_t));
    id_key->id = current->client_id;
    id_key->fd = current->fake_fd;

    HASH_ADD_INT(cli_fd_table, fake_fd, current);
    HASH_ADD_INT(id_fd_table, id, id_key);

    //DEBUG
    printf("debug::id: %d\n", current->client_id);

    return current->fake_fd;
}

int mw_connect(int fake_fd, struct sockaddr *addr, socklen_t len)
{
    int r = -1;

    client_t *current;
    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    if (current->is_droped) {
        printf("debug::%d is re connect\n", current->client_id);
        current->fd = socket(current->domain, current->type, current->protocol);

        while(r == -1) {
            sleep(2);
            r = connect(current->fd, addr, len);
        }
        current->is_droped = 0;

    } else {

        current->sock_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
        current->sock_len = (socklen_t *)malloc(sizeof(socklen_t));
        memcpy(current->sock_addr, addr, sizeof(struct sockaddr));
        memcpy(current->sock_len, &len, sizeof(socklen_t));

        r = connect(current->fd, addr, len);

    }

    time(&current->time);
    send(   current->fd,
            &(current->client_id),
            sizeof(current->client_id),
            MSG_NOSIGNAL);

    return r;
}


ssize_t mw_send(int fake_fd, const void *buf, size_t n, int flags)
{
    client_t *current;
    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    if (current->is_droped) {
        printf("debug::%d droped before send\n", current->client_id);
        mw_connect(fake_fd, current->sock_addr, *current->sock_len);
    }

    int r = send(current->fd, buf, n, flags);
    while (r == -1) {
        printf("debug::%d droped in send\n", current->client_id);
        current->is_droped = 1;
        mw_connect(fake_fd, current->sock_addr, *current->sock_len);
        r = send(current->fd, buf, n, flags);
    }

    return r;
}

ssize_t mw_recv(int fake_fd, void *buf, size_t n, int flags)
{
    client_t *current;
    int r;
    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    if (current->is_droped) {
        printf("debug::%d droped before recv\n", current->client_id);
        mw_connect(fake_fd, current->sock_addr, *current->sock_len);
    }

    while (1) {
        r = recv(current->fd, buf, n, flags);
        if (r == 0) {
        printf("debug::%d droped in recv\n", current->client_id);
            mw_connect(fake_fd, current->sock_addr, *current->sock_len);
        } else {
            break;
        }
    }

    return r;
}

int mw_close(int fake_fd)
{
    client_t *current;
    id_fd_t *id_key;
    int fd;

    HASH_FIND_INT(cli_fd_table, &fake_fd, current);
    HASH_FIND_INT(id_fd_table, &(current->client_id), id_key);

    fd = current->fd;
    free(current->sock_addr);
    free(current->sock_len);
    free(current);
    free(id_key);
    HASH_DEL(cli_fd_table, current);
    HASH_DEL(id_fd_table, id_key);

    return close(fd);
}


