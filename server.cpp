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

#define PORT 7777
#define MAX_CLIENTS 32
#define BUFFER_SIZE 4096

struct Client {
    int fd;
    std::string ip;
};

int main() {
    int server_fd;
    struct sockaddr_in address;
    
    // Создаем TCP сокет
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Настройка опций сокета (чтобы избежать ошибки "Address already in use")
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

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[*] cchat-server запущен на порту " << PORT << " (Zero-Logs Mode)\n";

    std::vector<struct pollfd> pfds;
    std::vector<Client> clients;

    // Добавляем серверный сокет в пул poll
    struct pollfd pfn;
    pfn.fd = server_fd;
    pfn.events = POLLIN;
    pfds.push_back(pfn);

    while (true) {
        int poll_count = poll(pfds.data(), pfds.size(), -1);
        if (poll_count < 0) break;

        // Проверяем новые подключения
        if (pfds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int new_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
            
            if (new_sock >= 0) {
                if (clients.size() < MAX_CLIENTS) {
                    struct pollfd cpfn;
                    cpfn.fd = new_sock;
                    cpfn.events = POLLIN;
                    pfds.push_back(cpfn);
                    
                    Client c;
                    c.fd = new_sock;
                    c.ip = inet_ntoa(client_addr.sin_addr);
                    clients.push_back(c);

                    std::cout << "[+] Подключился клиент с IP: " << c.ip << "\n";
                } else {
                    // Превышен лимит клиентов — сбрасываем соединение
                    close(new_sock);
                }
            }
        }

        // Проверяем данные от клиентов
        for (size_t i = 1; i < pfds.size(); ++i) {
            if (pfds[i].revents & POLLIN) {
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, BUFFER_SIZE);
                int valread = read(pfds[i].fd, buffer, BUFFER_SIZE);

                if (valread <= 0) {
                    // Клиент отключился
                    std::cout << "[-] Клиент отключился (" << clients[i - 1].ip << ")\n";
                    close(pfds[i].fd);
                    pfds.erase(pfds.begin() + i);
                    clients.erase(clients.begin() + (i - 1));
                    --i;
                } else {
                    // Ретрансляция сообщения всем остальным клиентам (Broadcast)
                    for (size_t j = 1; j < pfds.size(); ++j) {
                        if (j != i) { // Не отправляем обратно отправителю
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
