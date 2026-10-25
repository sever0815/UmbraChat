#include <iostream>
#include <string>
#include <vector>
#include "config.h"

void clear_screen() {
    std::cout << "\033[2J\033[1H";
}

// Заглушка функции подключения и обмена сообщениями в памяти терминала
void start_chat_session(const std::string& endpoint, const std::string& key, const std::string& username) {
    clear_screen();
    std::cout << "=== Сеанс связи с " << endpoint << " (Пользователь: " << username << ") ===\n";
    std::cout << "[!] История сообщений не сохраняется. При выходе буфер будет очищен.\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << "Введите сообщение (или /exit для выхода):\n\n";

    std::string input;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, input)) break;
        if (input == "/exit") break;

        // Здесь происходит отправка и вывод сообщения в stdout терминала
        if (!input.empty()) {
            std::cout << "\033[A\33[2K"; // Стираем строку ввода для чистоты вывода чата
            std::cout << "[" << username << "]: " << input << "\n";
        }
    }
    
    std::cout << "\n[*] Отключение... Буфер очищен.\n";
}

int main() {
    ConfigManager cfg;
    int choice;

    do {
        std::cout << "\n=== cchat (Anonymous Edition) ===\n"
                  << "(1) Выбрать Сервер\n"
                  << "(2) Подключиться (разово)\n"
                  << "(0) Добавить сервер\n"
                  << "--------------------------------\n"
                  << "Выберите пункт: ";
        
        if (!(std::cin >> choice)) break;
        std::cin.ignore(); // Очистка буфера после ввода числа

        switch (choice) {
            case 1: {
                auto servers = cfg.load_servers();
                if (servers.empty()) {
                    std::cout << "[-] Нет сохраненных серверов в ~/.cchate/\n";
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
                std::cout << "ip:domain: "; std::getline(std::cin, endpoint);
                std::cout << "key: "; std::getline(std::cin, key);
                std::cout << "username: "; std::getline(std::cin, username);

                start_chat_session(endpoint, key, username);
                break;
            }
            case 0: {
                ServerConfig s;
                std::cout << "Name Server: "; std::getline(std::cin, s.name);
                std::cout << "ip:domain: "; std::getline(std::cin, s.endpoint);
                std::cout << "key: "; std::getline(std::cin, s.key);
                std::cout << "username: "; std::getline(std::cin, s.username);

                cfg.save_server(s);
                std::cout << "[+] Сервер '" << s.name << "' успешно сохранен в ~/.cchate/\n";
                break;
            }
            default:
                std::cout << "Неверный пункт меню.\n";
        }
    } while (choice != 0);

    return 0;
}
