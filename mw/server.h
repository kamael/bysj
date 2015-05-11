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

    int cli_port;
    long cli_ip;

    UT_hash_handle hh;
};

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


void mw_init_send_heart(void *p)
{
    struct sockaddr_in address;
    int sock;

    char buf[20];

    client_t *client, *tmp_item;


    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock <= 0) {
        fprintf(stderr, "errer: cannot create socket\n");
    }

    address.sin_family = AF_INET;

    while(1) {
        sleep(2);

        HASH_ITER(hh, cli_fd_table, client, tmp_item) {
            address.sin_addr.s_addr = client->cli_ip;
            address.sin_port = htons(client->cli_port);
            memcpy(buf, (char *)&(client->client_id), sizeof(int));
            buf[sizeof(int)] = 'H';
            sendto(sock, buf, 20, 0,
                (struct sockaddr *)&address, sizeof(struct sockaddr_in));
            printf("debug: send heart");
        }
    }

}



int mw_socket(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    return fd;
}

int mw_bind(int fd, struct sockaddr *addr, socklen_t addr_len)
{
    return bind(fd, addr, addr_len);
}

int mw_listen(int fd, int n) {
    return listen(fd, n);
}

int mw_accept(int fd, struct sockaddr *addr, socklen_t *addr_len)
{
    int c_fd = accept(fd, addr, addr_len);
    int client_id;
    client_t *client;
    id_fd_t *id_key;
    int fake_fd;

    pthread_t thread;
    void *p = NULL;

    if (!heart_on) {
        pthread_create(&thread, 0, (void *)mw_init_send_heart, p);
        pthread_detach(thread);
        heart_on = 1;
    }


    srand(time(NULL));

    recv(c_fd, &client_id, sizeof(int), MSG_NOSIGNAL);

    HASH_FIND_INT(id_fd_table, &client_id, id_key);
    HASH_FIND_INT(cli_fd_table, &id_key->fd, client);
    if (client == NULL) {
        client = (client_t *)malloc(sizeof(client_t));

        fake_fd = rand() % 10000000 + 10000000;
        client->fake_fd = fake_fd;
        client->fd = c_fd;
        client->is_droped = 0;
        client->client_id = client_id;

        client->cli_port = 8992;
        client->cli_ip = \
            (long)(((struct sockaddr_in *)&addr)->sin_addr.s_addr);



        id_key = (id_fd_t *)malloc(sizeof(id_fd_t));
        id_key->id = client_id;
        id_key->fd = c_fd;

        HASH_ADD_INT(cli_fd_table, fake_fd, client);
        HASH_ADD_INT(id_fd_table, id, id_key);
    } else {
        client->fd = c_fd;
        client->is_droped = 0;
    }

    return client->fake_fd;
}

ssize_t mw_recv(int fake_fd, void *buf, size_t n, int flags)
{
    client_t *current;
    HASH_FIND_INT(cli_fd_table, &fake_fd, current);

    printf("debug::recv from id: %d\n", current->client_id);

    return recv(current->fd, buf, n, flags);
}


