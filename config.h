#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <sys/stat.h>

namespace fs = std::filesystem;

struct ServerConfig {
    std::string name;
    std::string endpoint;
    std::string key;
    std::string username;
};

class ConfigManager {
private:
    std::string config_dir;
    std::string config_file;

public:
    ConfigManager() {
        const char* home = getenv("HOME");
        std::string home_dir = home ? home : ".";
        config_dir = home_dir + "/.cchate";
        config_file = config_dir + "/config.conf";
        init_dir();
    }

    void init_dir() {
        if (!fs::exists(config_dir)) {
            fs::create_directories(config_dir);
            chmod(config_dir.c_str(), 0700); // Строгие права доступа
        }
    }

    std::vector<ServerConfig> load_servers() {
        std::vector<ServerConfig> servers;
        std::ifstream file(config_file);
        if (!file.is_open()) return servers;

        std::string line;
        ServerConfig s;
        while (std::getline(file, line)) {
            if (line.rfind("NAME=", 0) == 0) s.name = line.substr(5);
            else if (line.rfind("ENDPOINT=", 0) == 0) s.endpoint = line.substr(9);
            else if (line.rfind("KEY=", 0) == 0) s.key = line.substr(4);
            else if (line.rfind("USER=", 0) == 0) {
                s.username = line.substr(5);
                servers.push_back(s);
            }
        }
        return servers;
    }

    void save_server(const ServerConfig& s) {
        std::ofstream file(config_file, std::ios::app);
        if (file.is_open()) {
            file << "NAME=" << s.name << "\n"
                 << "ENDPOINT=" << s.endpoint << "\n"
                 << "KEY=" << s.key << "\n"
                 << "USER=" << s.username << "\n---\n";
            chmod(config_file.c_str(), 0600);
        }
    }
};
