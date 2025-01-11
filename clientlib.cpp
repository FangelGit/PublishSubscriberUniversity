#include "clientlib.h"


int make_socket_non_blocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
}


int non_blocking_send(int sockfd, const char* message, size_t length) {
    int attempts = 0;
    size_t total_sent = 0;

    while (total_sent < length && attempts < MAX_RETRIES) {
        ssize_t sent = send(sockfd, message + total_sent, length - total_sent, 0);
        if (sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                attempts++;
                usleep(SLEEP_TIME);
                continue;
            } else {
                perror("send");
                return -1;
            }
        }
        total_sent += sent;
    }

    if (attempts == MAX_RETRIES) {
        perror("Failed send message after retries");
        return -1;
    }
    return 0;
}


int non_blocking_recv(int sockfd, char* buffer, size_t max_length) {
    while (true) {
        ssize_t received = recv(sockfd, buffer, max_length, 0);
        if (received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(SLEEP_TIME);
                continue;
            }
            perror("recv");
            return -1;
        }
        if (received == 0) {
            perror("Server closed connection");
            return -1;
        }
        buffer[received] = '\0';
        break;
    }
    return 0;
}


int client_init(Client* client, const char* ip, int port) {
    if ((client->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }


    if (make_socket_non_blocking(client->sockfd) == -1) {
        close(client->sockfd);
        return -1;
    }

    memset(&(client->server_addr), 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &client->server_addr.sin_addr) <= 0) {
        close(client->sockfd);
        return -1;
    }

    int result = connect(client->sockfd, reinterpret_cast<struct sockaddr *>(&client->server_addr), sizeof(client->server_addr));
    if (result < 0) {
        if (errno != EINPROGRESS) {
            close(client->sockfd);
            return -1;
        }
    }

    return 0;
}


int create_queue(const Client* client, const char* queue_name, int max_size, int message_ttl) {
    char buffer[MAX_MESSAGE_LENGTH];
    snprintf(buffer, sizeof(buffer), "CREATE %s %d %d\n", queue_name, max_size, message_ttl);

    if (non_blocking_send(client->sockfd, buffer, strlen(buffer)) == -1) {
        perror("Send");
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (non_blocking_recv(client->sockfd, buffer, sizeof(buffer)) == -1) {
        perror("recv");
        return -1;
    }

    return strcmp(buffer, "OK\n") == 0 ? 0 : -1;
}


int send_message(const Client* client, const char* queue_name, const char* message) {
    char buffer[MAX_MESSAGE_LENGTH];
    snprintf(buffer, sizeof(buffer), "SEND %s %s\n", queue_name, message);

    if (non_blocking_send(client->sockfd, buffer, strlen(buffer)) == -1) {
        perror("Send");
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (non_blocking_recv(client->sockfd, buffer, sizeof(buffer)) == -1) {
        perror("recv");
        return -1;
    }

    return strcmp(buffer, "OK\n") == 0 ? 0 : -1;
}


int open_queue(const Client* client, const char* queue_name, char* msg) {
    char buffer[MAX_MESSAGE_LENGTH] = {};
    char data_accumulator[MAX_MESSAGE_LENGTH * 10] = {};
    bool is_ok = false;

    snprintf(buffer, sizeof(buffer), "OPEN %s\n", queue_name);

    if (non_blocking_send(client->sockfd, buffer, strlen(buffer)) == -1) {
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));
        if (non_blocking_recv(client->sockfd, buffer, sizeof(buffer)) == -1) {
            return -1;
        }
        strncat(data_accumulator, buffer, sizeof(data_accumulator) - strlen(data_accumulator) - 1);

        char* pos;
        while ((pos = strchr(data_accumulator, '\n')) != NULL) {
            *pos = '\0';
            if (strcmp(data_accumulator, "OK") == 0) {
                is_ok = true;
            } else {
                strncat(msg, data_accumulator, MAX_MESSAGE_LENGTH - strlen(msg) - 1);
                strncat(msg, "\n", MAX_MESSAGE_LENGTH - strlen(msg) - 1);
            }
            memmove(data_accumulator, pos + 1, strlen(pos + 1) + 1);
        }

    return is_ok == true? 0 : -1 ;
}


void client_close(const Client* client) {
    close(client->sockfd);
}

