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
    int fake_fd;        //key 1
    int fd;
    int client_id;      //key 2
    int is_droped;

    UT_hash_handle hh;
};

typedef struct client client_t;

client_t *cli_fd_table = NULL;  // fake_fd as key
client_t *cli_id_table = NULL;  // client_id as key
//使用双hash表实现可以两个不同的值作为key，修改hash表时需要同时修改两个表
//该方法不高效但是能用。



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
    int fake_fd;

    srand(time(NULL));

    recv(c_fd, &client_id, sizeof(int), MSG_NOSIGNAL);

    HASH_FIND_INT(cli_id_table, &client_id, client);
    if (client == NULL) {
        client = (client_t *)malloc(sizeof(client_t));

        fake_fd = rand() & 0x1000000;
        client->fake_fd = fake_fd;
        client->fd = c_fd;
        client->is_droped = 0;
        client->client_id = client_id;

        HASH_ADD_INT(cli_id_table, client_id, client);
        HASH_ADD_INT(cli_fd_table, fake_fd, client);
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


