#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
//#include <netinet/in.h>
#include <sys/wait.h>
//#include <sys/socket.h>

#include "mw/server.h"
#include <pthread.h>


#define PORT 5002 // The port which is communicate with server
#define BACKLOG 10
#define LENGTH 512 // Buffer length

void *process(void *fd) {
            int nsockfd = *(int *)fd;
            char* f_name = "send.txt";
            char sdbuf[LENGTH]; // Send buffer
            printf("[server] send %s to the client...", f_name);
            FILE *fp = fopen(f_name, "r");
            if(fp == NULL)
            {
                printf("ERROR: File %s not found.\n", f_name);
                exit(1);
            }
            bzero(sdbuf, LENGTH);
            int f_block_sz;
            while((f_block_sz = fread(sdbuf, sizeof(char), LENGTH, fp))>0)
            {
                if(mw_send(nsockfd, sdbuf, f_block_sz, 0) < 0)
                {
                    printf("ERROR: Failed to send file %s.\n", f_name);
                    break;
                }
                bzero(sdbuf, LENGTH);
            }
            printf("ok!\n");
            mw_shutdown(nsockfd, 2);
            printf("[server] connection closed.\n");
        }

int main ()
{
    int sockfd; // Socket file descriptor
    int asockfd; // New Socket file descriptor
    int num;
    int sin_size; // to store struct size
    struct sockaddr_in addr_local;
    struct sockaddr_in addr_remote;
    pthread_t thread;

    /* Get the Socket file descriptor */
    if( (sockfd = mw_socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        printf ("ERROR: Failed to obtain Socket Descriptor.\n");
        return (0);
    }
    else printf ("[server] obtain socket descriptor successfully.\n");
    /* Fill the local socket address struct */
    addr_local.sin_family = AF_INET; // Protocol Family
    addr_local.sin_port = htons(PORT); // Port number
    addr_local.sin_addr.s_addr = INADDR_ANY; // AutoFill local address
    bzero(&(addr_local.sin_zero), 8); // Flush the rest of struct
    /* Bind a special Port */
    if( mw_bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
    {
        printf ("ERROR: Failed to bind Port %d.\n",PORT);
        return (0);
    }
    else printf("[server] bind tcp port %d in addr 0.0.0.0 sucessfully.\n",PORT);
    /* Listen remote connect/calling */
    if(mw_listen(sockfd,BACKLOG) == -1)
    {
        printf ("ERROR: Failed to listen Port %d.\n", PORT);
        return (0);
    }
    else printf ("[server] listening the port %d sucessfully.\n", PORT);
    while(1)
    {
        sin_size = sizeof(struct sockaddr_in);
        /* Wait a connection, and obtain a new socket file despriptor for single connection */
        if ((asockfd = mw_accept(sockfd, (struct sockaddr *)&addr_remote, &sin_size)) == -1)
            printf ("ERROR: Obtain new Socket Despcritor error.\n");
        else printf ("[server] server has got connect.\n");
        /* Child process */
        pthread_create(&thread, 0, (void *)process, (void *)&asockfd);
        pthread_detach(thread);
    }
}
