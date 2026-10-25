#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

// === НАСТРОЙКИ СЕРВЕРА UMBRA ===
#define PORT 7777
#define SERVER_KEY "super_secret_key_123" // Твой ключ для подключения
#define MAX_CLIENTS 32
#define BUFFER_SIZE 4096

struct Client {
    int fd;
    std::string ip;
};

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[*] Umbra Relay Server запущен на порту " << PORT << " (Zero-Logs Mode)\n";

    std::vector<struct pollfd> pfds;
    std::vector<Client> clients;

    struct pollfd pfn;
    pfn.fd = server_fd;
    pfn.events = POLLIN;
    pfds.push_back(pfn);

    while (true) {
        int poll_count = poll(pfds.data(), pfds.size(), -1);
        if (poll_count < 0) break;

        if (pfds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int new_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
            
            if (new_sock >= 0) {
                std::string client_ip = inet_ntoa(client_addr.sin_addr);

                char auth_buf[256];
                memset(auth_buf, 0, sizeof(auth_buf));
                int bytes_read = read(new_sock, auth_buf, sizeof(auth_buf) - 1);

                std::string received_key = "";
                if (bytes_read > 0) {
                    received_key = trim(auth_buf);
                }

                if (received_key != SERVER_KEY) {
                    std::cout << "[-] Отклонен клиент " << client_ip << " (неверный ключ: '" << received_key << "')\n";
                    close(new_sock);
                } else if (clients.size() >= MAX_CLIENTS) {
                    std::cout << "[-] Отклонен клиент " << client_ip << " (сервер полон)\n";
                    close(new_sock);
                } else {
                    struct pollfd cpfn;
                    cpfn.fd = new_sock;
                    cpfn.events = POLLIN;
                    pfds.push_back(cpfn);
                    
                    Client c;
                    c.fd = new_sock;
                    c.ip = client_ip;
                    clients.push_back(c);

                    std::cout << "[+] Успешная авторизация клиента: " << c.ip << "\n";
                    
                    std::string ok_msg = "[+] Авторизация успешна!\n";
                    send(new_sock, ok_msg.c_str(), ok_msg.size(), 0);
                }
            }
        }

        for (size_t i = 1; i < pfds.size(); ++i) {
            if (pfds[i].revents & POLLIN) {
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                int valread = read(pfds[i].fd, buffer, BUFFER_SIZE);

                if (valread <= 0) {
                    std::cout << "[-] Клиент отключился (" << clients[i - 1].ip << ")\n";
                    close(pfds[i].fd);
                    pfds.erase(pfds.begin() + i);
                    clients.erase(clients.begin() + (i - 1));
                    --i;
                } else {
                    for (size_t j = 1; j < pfds.size(); ++j) {
                        if (j != i) {
                            send(pfds[j].fd, buffer, valread, 0);
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
