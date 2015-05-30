#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <pthread.h>
#include "uthash.h"


#if !defined(NDEBUG)
#define debug_log(...) do { fprintf(stdout, __VA_ARGS__); } while (0)
#else
#define debug_log(...) do {} while (0)
#endif

struct client {
    int fake_fd;        //key
    int fd;
    int client_id;
    int is_droped;
    int is_connected;

    int count;

    long server_ip;
    int server_port;

    struct sockaddr sock_addr;
    socklen_t sock_len;
    int domain, type, protocol;

/*
    time_t time;
*/

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
int count_num = 0;

void mw_init_send_heart(void *p)
{
    struct sockaddr_in address;
    int sock;

    struct sockaddr_in peer_addr;
    socklen_t peer_len;
    int r;
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;


    char buf[20];

    client_t *client, *tmp_item;


    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock <= 0) {
        fprintf(stderr, "errer: cannot create socket\n");
    }

    address.sin_family = AF_INET;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                (char *)&timeout, sizeof(timeout)) < 0)
                fprintf(stderr, "setsockopt failed\n");

    while(1) {
        sleep(2);

        HASH_ITER(hh, cli_fd_table, client, tmp_item) {
            address.sin_addr.s_addr = client->server_ip;
            address.sin_port = htons(client->server_port);
            memcpy(buf, (char *)&(client->client_id), sizeof(int));
            buf[sizeof(int)] = 'H';
            sendto(sock, buf, 20, 0,
                (struct sockaddr *)&address, sizeof(struct sockaddr_in));
            r = recvfrom(sock, buf, 20, 0,
                    (struct sockaddr *)&peer_addr, &peer_len);

            if (r > 0) {
                debug_log("debug: send heart\n");
                client->is_connected = 1;
            } else {
                debug_log("debug: heart error\n");
                client->is_droped = 1;
                client->is_connected = 0;
                shutdown(client->fd, 2);
            }

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
        pthread_create(&thread, 0, (void *)mw_init_send_heart, p);
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
    current->is_connected = 0;

    current->server_port = 8992;
/*
    current->time = 0;
*/

    current->count = -1;

    current->domain = domain;
    current->type = type;
    current->protocol = protocol;


    id_key = (id_fd_t *)malloc(sizeof(id_fd_t));
    id_key->id = current->client_id;
    id_key->fd = current->fake_fd;

    HASH_ADD_INT(cli_fd_table, fake_fd, current);
    HASH_ADD_INT(id_fd_table, id, id_key);

    debug_log("debug::id: %d\n", current->client_id);

    return current->fake_fd;
}

int mw_connect(int fake_fd, struct sockaddr *addr, socklen_t len)
{
    int r = -1;

    client_t *current;
    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    if (current->is_droped) {
        while (current->is_connected == 0) {
            sleep(2);
        }

        debug_log("debug::%d is re connect\n", current->client_id);
        r = -1;
        while(r == -1) {
            debug_log("debug:: start re connect\n");
            current->fd = socket(current->domain, current->type, current->protocol);
            sleep(2);
            r = connect(current->fd, addr, len);
        }
        current->is_droped = 0;

    } else {

        current->server_ip = (long)(((struct sockaddr_in *)addr)->sin_addr.s_addr);

        memcpy(&(current->sock_addr), addr, sizeof(struct sockaddr));
        memcpy(&(current->sock_len), &len, sizeof(socklen_t));

        r = connect(current->fd, addr, len);

    }

    send(   current->fd,
            &(current->client_id),
            sizeof(current->client_id),
            MSG_NOSIGNAL);

    return r;
}


ssize_t mw_send(int fake_fd, const void *buf, size_t n, int flags)
{
    client_t *current;
    int r;

    char *s_buf;
    char r_buf[20];

    struct timeval timeout;
    timeout.tv_usec = 0;

    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    s_buf = malloc(n + sizeof(int));
    memcpy(s_buf, (char *)&count_num, sizeof(int));
    memcpy(s_buf + sizeof(int), buf, n);
    count_num++;


    if (current->is_droped) {
        debug_log("debug::%d droped before send\n", current->client_id);
        mw_connect(fake_fd, &(current->sock_addr), current->sock_len);
    }

    debug_log("start send\n");

    while (1) {
        r = send(current->fd, s_buf, n + sizeof(int), flags | MSG_NOSIGNAL);

        if (r == 0)
            return 0;

        if (r < 0) {
            debug_log("debug::%d droped in send\n", current->client_id);
            current->is_droped = 1;
            mw_connect(fake_fd, &(current->sock_addr), current->sock_len);
        } else {
            debug_log("debug:: recv success log start\n");

            timeout.tv_sec = 4;
            setsockopt(current->fd, SOL_SOCKET, SO_RCVTIMEO,
                (char *)&timeout, sizeof(timeout));
            r = recv(current->fd, r_buf, 20, MSG_NOSIGNAL);
            timeout.tv_sec = 0;
            setsockopt(current->fd, SOL_SOCKET, SO_RCVTIMEO,
                (char *)&timeout, sizeof(timeout));

            if (r > 0)
                break;
            debug_log("debug:: recv log error\n");
        }
    }

    debug_log("debug:: send success\n");

    return r;
}

ssize_t mw_recv(int fake_fd, void *buf, size_t n, int flags)
{
    client_t *current;
    int r;

    int tmp_count;
    char *s_buf;
    char r_buf[20] = "00";

    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    s_buf = malloc(n + sizeof(int));
    memset(s_buf, 0, n + sizeof(int));

    if (current->is_droped) {
        debug_log("debug::%d droped before recv\n", current->client_id);
        mw_connect(fake_fd, &(current->sock_addr), current->sock_len);
    }

    while (1) {
        r = recv(current->fd, buf, n, flags | MSG_NOSIGNAL);

        if (r == 0)
            return 0;

        if (r < 0) {
            debug_log("debug::%d droped in recv\n", current->client_id);
            current->is_droped = 1;
            mw_connect(fake_fd, &(current->sock_addr), current->sock_len);
        } else {
            memcpy((char *)&tmp_count, s_buf, sizeof(int));
            if (tmp_count - current->count <= 0) {
                ;
            } else {
                current->count = tmp_count;
                memcpy(buf, s_buf + sizeof(int), n);
                send(current->fd, r_buf, 20, MSG_NOSIGNAL);
                break;
            }
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
    assert(current != NULL);
    HASH_FIND_INT(id_fd_table, &(current->client_id), id_key);
    assert(id_key != NULL);

    fd = current->fd;
    free(current);
    free(id_key);
    HASH_DEL(cli_fd_table, current);
    HASH_DEL(id_fd_table, id_key);

    return close(fd);
}


