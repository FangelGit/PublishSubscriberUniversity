#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>

#ifndef CLIENTLIB_H
#define CLIENTLIB_H

#define MAX_MESSAGE_LENGTH 255
#define MAX_RETRIES 5
#define SLEEP_TIME 100000

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
} Client;


int non_blocking_send(int sockfd, const char* message, size_t length);
int non_blocking_recv(int sockfd, char* buffer, size_t max_length);

int client_init(Client* client, const char* ip, int port);
int create_queue(const Client* client, const char* queue_name, int max_size, int message_ttl);
int send_message(const Client* client, const char* queue_name, const char* message);
int open_queue(const Client* client, const char* queue_name, char* msg);
void client_close(const Client* client);

#endif //CLIENTLIB_H
