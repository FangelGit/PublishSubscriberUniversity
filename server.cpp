#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <cerrno>
#include <ctime>

#define MAX_POLLS 10
#define MAX_QUEUES 15
#define MAX_MESSAGE_LENGTH 255
#define QUEUE_CAPACITY 200
#define MAX_CLIENTS_PER_QUEUE 10
#define MAX_RETRIES 50
#define SLEEP_TIME 100000


typedef struct {
    char message[MAX_MESSAGE_LENGTH];
    time_t timestamp;
} Message;

typedef struct {
    char* name;
    Message messages[QUEUE_CAPACITY];
    int message_count;
    int max_size;
    int message_ttl;
    int clients[MAX_CLIENTS_PER_QUEUE];
    int client_count;
} MessageQueue;

MessageQueue* queues[MAX_QUEUES];
int queue_count = 0;


int make_socket_non_blocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        perror("fcntl");
        return -1;
    }
    return 0;
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
            }
            perror("send");
            return -1;
        }
        total_sent += sent;
    }

    if (attempts == MAX_RETRIES) {
        perror("Failed after max retries");
        return -1;
    }
    return 0;
}

int non_blocking_recv(int sockfd, char* buffer, size_t max_length) {
    ssize_t total_received = 0;
    int attempts = 0;

    while (true) {
        ssize_t received = recv(sockfd, buffer + total_received, max_length - total_received, 0);
        if (received != -1) {
            if (received == 0) {
                printf("Client closed connection\n");
                return -1;
            }
            total_received += received;
            if (buffer[total_received - 1] == '\n') {
                break;
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (++attempts >= MAX_RETRIES) {
                    perror("Failed after max retries");
                    return -1;
                }
                usleep(SLEEP_TIME);
            } else {
                perror("recv");
                return -1;
            }
        }
    }

    buffer[total_received] = '\0';
    return 0;
}

MessageQueue* find_queue(const char* name) {
    for (int i = 0; i < queue_count; i++) {
        if (strcmp(queues[i]->name, name) == 0) {
            return queues[i];
        }
    }
    return NULL;
}

int create_new_queue(const char* name, int max_size, int message_ttl) {
    if (queue_count >= MAX_QUEUES) {
        return -1;
    }
    if (find_queue(name)) {
        return -1;
    }
    MessageQueue* queue = static_cast<MessageQueue *>(malloc(sizeof(MessageQueue)));
    queue->name = strdup(name);
    queue->message_count = 0;
    queue->max_size = max_size;
    queue->message_ttl = message_ttl;
    queues[queue_count++] = queue;
    return 0;
}


int send_old_messages(const char* queue_name, int client_socket) {
    MessageQueue* queue = find_queue(queue_name);
    if (!queue) return -1;
    for (int i = 0; i < queue->message_count; i++) {
        if (non_blocking_send(client_socket, queue->messages[i].message, strlen(queue->messages[i].message)) == -1) {
            perror("send");
            continue;
        }
        if (non_blocking_send(client_socket, "\n", 1) == -1) {
            perror("send delimiter");
        }
    }
    return 0;
}


int open_queue(const char* queue_name, int client_socket) {
    MessageQueue* queue = find_queue(queue_name);
    if (!queue) return -1;
    if (queue->client_count < MAX_CLIENTS_PER_QUEUE) {
        queue->clients[queue->client_count++] = client_socket;
        return 0;
    }
    return -1;
}




int send_message_to_queue(const char* queue_name, const char* message) {
    MessageQueue* queue = find_queue(queue_name);
    if (!queue) return -1;
    if (queue->message_count >= queue->max_size) {
        return -1;
    }
    int index = queue->message_count++;
    strncpy(queue->messages[index].message, message, MAX_MESSAGE_LENGTH-1);
    queue->messages[index].message[MAX_MESSAGE_LENGTH - 1] = '\0';
    queue->messages[index].timestamp = time(NULL);


    for (int i = 0; i < queue->client_count; i++) {
        int client_socket = queue->clients[i];
        if (non_blocking_send(client_socket, queue->messages[index].message, strlen(queue->messages[index].message)) == -1) {
            perror("send");
            return -1;
        }
    }

    return 0;
}

void check_message_lifetime(MessageQueue* queue) {
    time_t current_time = time(NULL);

    int shift_count = 0;
    for (int i = 0; i < queue->message_count; i++) {
        double age = difftime(current_time, queue->messages[i].timestamp);
        if (age > queue->message_ttl) {
            shift_count++;
        } else {
            if (shift_count > 0) {
                queue->messages[i - shift_count] = queue->messages[i];
            }
        }
    }
    queue->message_count -= shift_count;
}


int process_request(int client_socket) {
    char buffer[MAX_MESSAGE_LENGTH];
    char response[MAX_MESSAGE_LENGTH];


    if (non_blocking_recv(client_socket, buffer, sizeof(buffer)) == -1) {
        close(client_socket);
        perror("recv");
        return -1;
    }

    char queue_name[MAX_MESSAGE_LENGTH];
    char message[MAX_MESSAGE_LENGTH];
    int arg1, arg2;
    bool is_connect_queue = false;

    if (sscanf(buffer, "CREATE %s %d %d", queue_name, &arg1, &arg2) == 3) {
        if (create_new_queue(queue_name, arg1, arg2) == 0) {
            strcpy(response, "OK\n");
        } else {
            strcpy(response, "ERROR\n");
        }
    } else if (sscanf(buffer, "SEND %s %[^\n]", queue_name, message) == 2) {
        if (send_message_to_queue(queue_name, message) == 0) {
            strcpy(response, "OK\n");
        } else {
            strcpy(response, "ERROR\n");
        }
    } else if (sscanf(buffer, "OPEN %s", queue_name) == 1) {
        if (open_queue(queue_name, client_socket) == 0) {
            strcpy(response, "OK\n");
            is_connect_queue = true;
        } else {
            strcpy(response, "ERROR\n");
        }
    } else {
        strcpy(response, "INVALID COMMAND\n");
    }
    if (non_blocking_send(client_socket, response, strlen(response)) == -1) {
        close(client_socket);
        return -1;
    }

    if (is_connect_queue) {
        send_old_messages(queue_name, client_socket);
    }

    return 1;
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        perror("Usage: <IP> <port>");
        return 1;
    }
    int port = atoi(argv[2]);
    if (port <= 0) {
        perror("Invalid port");
        return 1;
    }

    sockaddr_in server_addr;
    pollfd fds[MAX_POLLS];
    int nfds = 1;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    make_socket_non_blocking(server_socket);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (bind(server_socket, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Listening...\n");
    fds[0].fd = server_socket;
    fds[0].events = POLLIN | POLLOUT;

    while (true) {
        int poll_count = poll(fds, nfds, -1);
        if (poll_count == -1) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (int q = 0; q < queue_count; q++) {
            check_message_lifetime(queues[q]);
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_socket) {
                    while (true) {
                        struct sockaddr_in client_addr;
                        socklen_t client_addr_len = sizeof(client_addr);
                        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);

                        if (client_socket == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            }
                            perror("accept");
                            break;
                        }

                        make_socket_non_blocking(client_socket);
                        if (nfds < MAX_POLLS) {
                            fds[nfds].fd = client_socket;
                            fds[nfds].events = POLLIN | POLLOUT;
                            nfds++;
                            printf("New client connected\n");
                        } else {
                            perror("Poll limit");
                            close(client_socket);
                        }
                    }
                } else {
                    int status = process_request(fds[i].fd);
                    if (status == -1) {
                        close(fds[i].fd);
                        for (int j = i; j < nfds - 1; j++) {
                            fds[j] = fds[j + 1];
                        }
                        nfds--;
                        i--;
                    }
                }
            }
        }
    }
}