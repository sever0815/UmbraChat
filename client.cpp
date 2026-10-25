#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "config.h"

std::atomic<bool> g_running(true);
std::string current_input = "";

void clear_screen() {
    std::cout << "\033[2J\033[1H" << std::flush;
}

// Поток приема сообщений от сервера
void receive_messages(int sock) {
    char buffer[4096];
    while (g_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cout << "\n[-] Соединение с сервером разорвано.\n";
            g_running = false;
            break;
        }

        // Очищаем текущую строку ввода, выводим сообщение, возвращаем строку ввода
        std::cout << "\33[2K\r";
        std::cout << buffer;
        std::cout << "> " << current_input << std::flush;
    }
}

void start_chat_session(const std::string& endpoint, const std::string& key, const std::string& username) {
    size_t colon = endpoint.find(':');
    if (colon == std::string::npos) {
        std::cout << "[-] Неверный формат адреса. Используйте IP:PORT\n";
        return;
    }
    std::string ip = endpoint.substr(0, colon);
    int port = std::stoi(endpoint.substr(colon + 1));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cout << "[-] Некорректный IP адрес.\n";
        close(sock);
        return;
    }

    std::cout << "[*] Подключение к " << endpoint << "...\n";
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Авторизация
    std::string auth_payload = key + "\n";
    send(sock, auth_payload.c_str(), auth_payload.size(), 0);

    char auth_resp[256];
    memset(auth_resp, 0, sizeof(auth_resp));
    int resp_len = recv(sock, auth_resp, sizeof(auth_resp) - 1, 0);
    
    if (resp_len <= 0 || std::string(auth_resp).find("[+]") == std::string::npos) {
        std::cout << "[-] Ошибка авторизации: неверный ключ.\n";
        close(sock);
        return;
    }

    clear_screen();
    std::cout << "=== Umbra Chat Secure Session (" << endpoint << ") ===\n";
    std::cout << "[!] Zero-Logs активен. Введите /exit для выхода.\n";
    std::cout << "--------------------------------------------------\n\n";
    std::cout << "> " << std::flush;

    g_running = true;
    std::thread rx_thread(receive_messages, sock);

    current_input.clear();
    char c;
    while (g_running && read(STDIN_FILENO, &c, 1) > 0) {
        if (c == '\n') {
            if (current_input == "/exit") {
                g_running = false;
                break;
            }
            if (!current_input.empty()) {
                std::string full_msg = "[" + username + "]: " + current_input + "\n";
                send(sock, full_msg.c_str(), full_msg.size(), 0);
                
                // Локальный вывод своего сообщения
                std::cout << "\33[2K\r[" << username << "]: " << current_input << "\n";
                current_input.clear();
            }
            std::cout << "> " << std::flush;
        } else if (c == 127 || c == 8) {
            if (!current_input.empty()) {
                current_input.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        } else if (c >= 32 && c < 127) {
            current_input += c;
            std::cout << c << std::flush;
        }
    }

    g_running = false;
    close(sock);
    if (rx_thread.joinable()) {
        rx_thread.join();
    }
    clear_screen();
    std::cout << "[*] Сеанс завершен. Буфер очищен.\n";
}

int main() {
    ConfigManager cfg;
    int choice;

    do {
        clear_screen();
        std::cout << "=== Umbra Chat ===\n"
                  << "(1) Выбрать Сервер\n"
                  << "(2) Подключиться (разово)\n"
                  << "(0) Добавить сервер\n"
                  << "------------------\n"
                  << "Выберите пункт: ";
        
        if (!(std::cin >> choice)) break;
        std::cin.ignore();

        switch (choice) {
            case 1: {
                clear_screen();
                auto servers = cfg.load_servers();
                if (servers.empty()) {
                    std::cout << "[-] Нет сохраненных серверов в ~/.umbra/\n";
                    std::cout << "\nНажмите Enter для возврата в меню...";
                    std::string dummy;
                    std::getline(std::cin, dummy);
                    break;
                }

                std::cout << "Сохраненные серверы:\n";
                for (size_t i = 0; i < servers.size(); ++i) {
                    std::cout << "(" << i + 1 << ") " << servers[i].name << " [" << servers[i].endpoint << "]\n";
                }
                std::cout << "\nВыберите номер: ";
                
                size_t s_idx;
                std::cin >> s_idx;
                std::cin.ignore();

                if (s_idx > 0 && s_idx <= servers.size()) {
                    const auto& s = servers[s_idx - 1];
                    start_chat_session(s.endpoint, s.key, s.username);
                } else {
                    std::cout << "[-] Неверный выбор. Возврат в меню...\n";
                    sleep(1);
                }
                break;
            }
            case 2: {
                clear_screen();
                std::string endpoint, key, username;
                std::cout << "ip:port: "; std::getline(std::cin, endpoint);
                std::cout << "key: "; std::getline(std::cin, key);
                std::cout << "username: "; std::getline(std::cin, username);

                start_chat_session(endpoint, key, username);
                break;
            }
            case 0: {
                clear_screen();
                ServerConfig s;
                std::cout << "Name Server: "; std::getline(std::cin, s.name);
                std::cout << "ip:port: "; std::getline(std::cin, s.endpoint);
                std::cout << "key: "; std::getline(std::cin, s.key);
                std::cout << "username: "; std::getline(std::cin, s.username);

                cfg.save_server(s);
                std::cout << "\n[+] Сервер '" << s.name << "' успешно сохранен в ~/.umbra/!\n";
                std::cout << "Нажмите Enter для возврата в меню...";
                std::string dummy;
                std::getline(std::cin, dummy);
                break;
            }
            default:
                std::cout << "[-] Неверный пункт меню.\n";
                sleep(1);
        }
    } while (choice != 0);

    clear_screen();
    return 0;
}
