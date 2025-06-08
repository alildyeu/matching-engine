#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <algorithm>
#include <iomanip> 
#include <chrono>
#include "logger.hpp"
#include <atomic>

class TimingManager {
public:
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& operation_name, Logger& logger)
            : operation_name_(operation_name), logger_(logger), 
              start_time_(std::chrono::high_resolution_clock::now()) {}
        
        ~ScopedTimer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time_);
            logger_.info(operation_name_, " took ", duration.count(), " milliseconds");
        }

    private:
        std::string operation_name_;
        Logger& logger_;
        std::chrono::high_resolution_clock::time_point start_time_;
    };

    static ScopedTimer measure(const std::string& operation_name, Logger& logger) {
        return ScopedTimer(operation_name, logger);
    }
};