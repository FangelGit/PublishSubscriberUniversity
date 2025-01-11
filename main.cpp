#include <iostream>
#include <string>
#include "clientlib.h"

void print_menu() {
    std::cout << "Choose an option:\n";
    std::cout << "1. Create Queue\n";
    std::cout << "2. Send Message\n";
    std::cout << "3. Open Queue\n";
    std::cout << "4. Exit\n";
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        perror("Parameters <IP> <port>");
        return 1;
    }

    int port = atoi(argv[2]);
    if (port <=0) {
        perror("port");
        return 1;
    }

    Client client;


    if (client_init(&client, argv[1], port) != 0) {
        perror("client init");
        return 1;
    }

    int choice;
    std::string queue_name, message;

    while (true) {
        print_menu();
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        switch (choice) {
            case 1:
                std::cout << "Enter queue name: ";
                std::cin >> queue_name;
                {
                    int max_size, ttl;
                    std::cout << "Enter max size of the queue: ";
                    std::cin >> max_size;
                    std::cout << "Enter message TTL (seconds): ";
                    std::cin >> ttl;
                    if (create_queue(&client, queue_name.c_str(), max_size, ttl) == 0) {
                        std::cout << "Queue created successfully.\n";
                    } else {
                        std::cerr << "Failed to create queue.\n";
                    }
                }
                break;

            case 2:
                std::cout << "Enter queue name: ";
                std::cin >> queue_name;
                std::cout << "Enter message: ";
                std::cin.ignore();
                std::getline(std::cin, message);
                if (send_message(&client, queue_name.c_str(), message.c_str()) == 0) {
                    std::cout << "Message sent successfully.\n";
                } else {
                    std::cerr << "Failed to send message.\n";
                }
                break;

            case 3:
                std::cout << "Enter queue name to open: ";
                std::cin >> queue_name;
                char buffer[MAX_MESSAGE_LENGTH];
                memset(buffer, 0, sizeof(buffer));
                if (open_queue(&client, queue_name.c_str(),buffer) == 0) {
                    std::cout << "Queue opened successfully. Waiting for messages...\n";
                    bool connected = true;
                    std::cout << "Received: " << buffer << std::endl;
                    while (connected) {
                        memset(buffer, 0, sizeof(buffer));
                        if (non_blocking_recv(client.sockfd, buffer, sizeof(buffer)) == -1) {
                            std::cerr << "Error receiving message or connection closed.\n";
                            client_close(&client);
                            connected = false;
                            continue;
                        }
                        std::cout << "Received: " << buffer << std::endl;
                    }
                    std::cout << "Disconnected from queue.\n";
                } else {
                    std::cerr << "Failed to open queue.\n";
                }
                break;
            case 4:
                client_close(&client);
                std::cout << "Exiting...\n";
                return 0;

            default:
                std::cerr << "Invalid choice!\n";
                break;
        }
    }
}