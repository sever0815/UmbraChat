#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include "config.h"

std::atomic<bool> g_running(true);
std::string current_input = "";
int term_rows = 24;
int term_cols = 80;

void update_terminal_size() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        term_rows = w.ws_row;
        term_cols = w.ws_col;
    }
}

void enter_alt_screen() {
    std::cout << "\033[?1049h\033[H" << std::flush;
    update_terminal_size();
}

void exit_alt_screen() {
    std::cout << "\033[?1049l" << std::flush;
}

void redraw_prompt() {
    update_terminal_size();
    // Переходим на строку ввода (предпоследняя строка терминала)
    std::cout << "\033[" << (term_rows - 1) << ";1H\033[2K> " << current_input << std::flush;
}

void receive_messages(int sock) {
    char buffer[4096];
    while (g_running) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) break;

        // Сохраняем текущую позицию курсора, скроллим/выводим сообщение выше строки ввода, возвращаем курсор
        std::cout << "\033[s"; // Сохранить курсор
        // Переходим в область чата (выше строки ввода)
        std::cout << "\033[" << (term_rows - 2) << ";1H\n\033[2K" << buffer;
        std::cout << "\033[u" << std::flush; // Восстановить курсор и обновить строку ввода
        redraw_prompt();
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

    enter_alt_screen();
    std::cout << "\033[1;1H=== Umbra Chat Secure Session (" << endpoint << ") ===\n";
    std::cout << "[!] Zero-Logs активен. Нажмите Enter для отправки, /exit для выхода.\n";
    
    // Рисуем разделительную линию над строкой ввода
    update_terminal_size();
    std::string line(term_cols, '-');
    std::cout << "\033[" << (term_rows - 3) << ";1H" << line << std::flush;

    g_running = true;
    std::thread rx_thread(receive_messages, sock);

    current_input.clear();
    redraw_prompt();

    char c;
    while (g_running && read(STDIN_FILENO, &c, 1) > 0) {
        if (c == '\n') {
            if (current_input == "/exit") {
                g_running = false;
                break;
            }
            if (!current_input.empty()) {
                // Передаем никнейм и текст сообщения
                std::string full_msg = "[" + username + "]: " + current_input + "\n";
                send(sock, full_msg.c_str(), full_msg.size(), 0);
                
                // Выводим свое сообщение в зону чата
                std::cout << "\033[s";
                std::cout << "\033[" << (term_rows - 2) << ";1H\n\033[2K[" << username << "]: " << current_input;
                std::cout << "\033[u" << std::flush;

                current_input.clear();
            }
            redraw_prompt();
        } else if (c == 127 || c == 8) {
            if (!current_input.empty()) {
                current_input.pop_back();
                redraw_prompt();
            }
        } else if (c >= 32 && c < 127) {
            current_input += c;
            redraw_prompt();
        }
    }

    g_running = false;
    close(sock);
    if (rx_thread.joinable()) {
        rx_thread.join();
    }
    exit_alt_screen();
    std::cout << "[*] Сеанс завершен. Буфер очищен.\n";
}

int main() {
    ConfigManager cfg;
    int choice;

    do {
        std::cout << "\n=== Umbra Chat ===\n"
                  << "(1) Выбрать Сервер\n"
                  << "(2) Подключиться (разово)\n"
                  << "(0) Добавить сервер\n"
                  << "------------------\n"
                  << "Выберите пункт: ";
        
        if (!(std::cin >> choice)) break;
        std::cin.ignore();

        switch (choice) {
            case 1: {
                auto servers = cfg.load_servers();
                if (servers.empty()) {
                    std::cout << "[-] Нет сохраненных серверов в ~/.umbra/\n";
                    break;
                }

                std::cout << "\nСохраненные серверы:\n";
                for (size_t i = 0; i < servers.size(); ++i) {
                    std::cout << "(" << i + 1 << ") " << servers[i].name << " [" << servers[i].endpoint << "]\n";
                }
                std::cout << "Выберите номер: ";
                
                size_t s_idx;
                std::cin >> s_idx;
                std::cin.ignore();

                if (s_idx > 0 && s_idx <= servers.size()) {
                    const auto& s = servers[s_idx - 1];
                    start_chat_session(s.endpoint, s.key, s.username);
                } else {
                    std::cout << "[-] Неверный выбор.\n";
                }
                break;
            }
            case 2: {
                std::string endpoint, key, username;
                std::cout << "ip:port: "; std::getline(std::cin, endpoint);
                std::cout << "key: "; std::getline(std::cin, key);
                std::cout << "username: "; std::getline(std::cin, username);

                start_chat_session(endpoint, key, username);
                break;
            }
            case 0: {
                ServerConfig s;
                std::cout << "Name Server: "; std::getline(std::cin, s.name);
                std::cout << "ip:port: "; std::getline(std::cin, s.endpoint);
                std::cout << "key: "; std::getline(std::cin, s.key);
                std::cout << "username: "; std::getline(std::cin, s.username);

                cfg.save_server(s);
                std::cout << "[+] Сервер '" << s.name << "' успешно сохранен в ~/.umbra/!\n";
                // Программа больше не завершается, цикл продолжается и выводит меню заново
                break;
            }
            default:
                std::cout << "[-] Неверный пункт меню.\n";
        }
    } while (choice != 0);

    return 0;
}
