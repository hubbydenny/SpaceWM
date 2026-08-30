#pragma once

#include <unordered_map>
#include <vector>
#include <deque>
#include <functional>
#include <string>
#include <iostream>
#include <chrono>
#include <iomanip>

enum class LogLevel { 
  Info, Warn, Error, Debug 
};

class Config {
public:
    void addBinding(const std::string& key, const std::function<void()>& action) {
        single[key] = action;
    }

    void addCombo(const std::vector<std::string>& keys, const std::function<void()>& action) { 
      combos.emplace_back(keys, action);
    }

    void handleKey(const std::string& key) {
        auto it = single.find(key);
        if (it != single.end()) it->second();
        history.push_back(key);
        const std::size_t maxLen = 10;
        if (history.size() > maxLen) history.pop_front();
        for (const auto& combo : combos) {
            const auto& seq = combo.first;
            if (seq.size() > history.size()) continue;
            bool match = true;
            for (std::size_t i = 0; i < seq.size(); ++i) {
                if (history[history.size() - seq.size() + i] != seq[i]) {
                    match = false; break;
                }
            }
            if (match) {
                combo.second();
                history.clear();
                break;
            }
        }
    }

    void Print(const std::string& msg) const { 
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::cout << '[' << std::put_time(std::localtime(&t), "%F %T") << "] " << msg << '\n';
    }

    void Log(const std::string& msg, LogLevel level = LogLevel::Info) const {
        const char* lvl = "INFO";
        switch (level) {
            case LogLevel::Warn:  lvl = "WARN"; break;
            case LogLevel::Error: lvl = "ERROR"; break;
            case LogLevel::Debug: lvl = "DEBUG"; break; 
            default: break;
        }
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::cout << '[' << std::put_time(std::localtime(&t), "%F %T") << "] [" << lvl << "] " << msg << '\n';
    }

private:
    std::unordered_map<std::string, std::function<void()>> single;
    std::vector<std::pair<std::vector<std::string>, std::function<void()>>> combos;
    std::deque<std::string> history;
};
