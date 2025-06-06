#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>    // For timestamps
#include <iomanip>   // For std::put_time and std::setfill, std::setw
#include <sstream>   // For std::ostringstream to build messages
#include <mutex>     // For std::mutex and std::lock_guard (thread safety)
#include <memory>    // For std::unique_ptr to manage ofstream

// Enum for log levels, similar to spdlog
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6 // To disable all logging
};

// Helper to convert LogLevel to its string representation
inline const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return "TRACE";
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARN:     return "WARNING"; // spdlog uses WARNING
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::OFF:      return "OFF";
        default:                 return "UNKNOWN";
    }
}
// Helper function to convert a string to uppercase
inline std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}

// Converts a string to its corresponding LogLevel enum value
inline LogLevel string_to_level(const std::string& level_str) {
    std::string upper_level_str = to_upper(level_str);

    if (upper_level_str == "TRACE") {
        return LogLevel::TRACE;
    } else if (upper_level_str == "DEBUG") {
        return LogLevel::DEBUG;
    } else if (upper_level_str == "INFO") {
        return LogLevel::INFO;
    } else if (upper_level_str == "WARN" || upper_level_str == "WARNING") { // Handles both "WARN" and "WARNING"
        return LogLevel::WARN;
    } else if (upper_level_str == "ERROR") {
        return LogLevel::ERROR;
    } else if (upper_level_str == "CRITICAL") {
        return LogLevel::CRITICAL;
    } else if (upper_level_str == "OFF") {
        return LogLevel::OFF;
    } else {
        // Default for an unrecognized string
        // You might want to log a warning here if the string is unknown
        return LogLevel::INFO; 
        // Alternatively, you could throw an exception:
        // throw std::invalid_argument("Unknown log level string: " + level_str);
    }
}

class Logger {
public:
    // Constructor for console logging
    Logger(std::string name, LogLevel level = LogLevel::INFO)
        : logger_name_(std::move(name)),
          min_level_(level),
          output_stream_(&std::cout), // Default to cout
          is_file_logger_(false) {}

    // Constructor for file logging
    Logger(std::string name, const std::string& filename, LogLevel level = LogLevel::INFO, bool append = true)
        : logger_name_(std::move(name)),
          min_level_(level),
          is_file_logger_(true) {
        file_output_stream_ = std::make_unique<std::ofstream>();
        file_output_stream_->open(filename, append ? std::ios_base::app : std::ios_base::trunc);
        
        if (file_output_stream_ && file_output_stream_->is_open()) {
            output_stream_ = file_output_stream_.get();
        } else {
            // Fallback to std::cerr if file opening fails
            output_stream_ = &std::cerr;
            is_file_logger_ = false; // Effectively a console logger now (on cerr)
            // Log an internal error message directly to cerr
            // (avoiding call to log() to prevent potential recursion if cerr itself fails)
            std::cerr << "[" << get_current_timestamp_() << "] [" << logger_name_ << "] [INTERNAL ERROR] Failed to open log file: " << filename << ". Falling back to stderr." << std::endl;
        }
    }



    ~Logger() {
        // std::unique_ptr will automatically close the file stream if it's open.
    }

    // Prevent copying and assignment to avoid issues with resource management (mutex, file stream)
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Set the minimum log level that this logger will output
    void set_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        min_level_ = level;
    }

    LogLevel get_level() const {
        std::lock_guard<std::mutex> lock(log_mutex_); // const method but needs lock if min_level_ can change
        return min_level_;
    }

    // --- Logging API methods (variadic templates) ---
    template<typename... Args>
    void trace(const Args&... args) {
        log_(LogLevel::TRACE, args...);
    }

    template<typename... Args>
    void debug(const Args&... args) {
        log_(LogLevel::DEBUG, args...);
    }

    template<typename... Args>
    void info(const Args&... args) {
        log_(LogLevel::INFO, args...);
    }

    template<typename... Args>
    void warn(const Args&... args) {
        log_(LogLevel::WARN, args...);
    }

    template<typename... Args>
    void error(const Args&... args) {
        log_(LogLevel::ERROR, args...);
    }

    template<typename... Args>
    void critical(const Args&... args) {
        log_(LogLevel::CRITICAL, args...);
    }

private:
    // Generates a timestamp string (e.g., "YYYY-MM-DD HH:MM:SS.milliseconds")
    std::string get_current_timestamp_() const {
        auto now = std::chrono::system_clock::now();
        auto now_as_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        // Use std::localtime (be mindful of its thread-safety if not C++17's localtime_s or platform equivalent)
        // For simplicity in a single class, std::localtime is often used, relying on system's behavior.
        // Most modern systems have thread-safe enough versions or use thread-local storage.
        oss << std::put_time(std::localtime(&now_as_time_t), "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    // Helper to build the user message from variadic arguments by concatenating them with spaces
    template<typename... Args>
    std::string format_user_message_(const Args&... args) const {
        std::ostringstream oss;
        // C++11 way to expand parameter pack and insert spaces.
        // This adds a space after each argument.
        using expander = int[]; // Dummy array
        (void)expander{0, (void(oss << args << " "), 0)...};
        
        std::string S = oss.str();
        // Remove the trailing space if any arguments were provided and message is not empty
        if (!S.empty() && S.back() == ' ') {
            S.pop_back();
        }
        return S;
    }
    
    // Overload for when no arguments are passed to a log function (e.g., logger.info();)
    // spdlog allows empty messages, so we should too.
    std::string format_user_message_() const {
        return ""; // Empty message
    }

    // The core logging function
    template<typename... Args>
    void log_(LogLevel msg_level, const Args&... args) {
        // Check level before acquiring the lock for a minor optimization,
        // but must re-check after acquiring lock if level can change concurrently.
        // For simplicity and correctness, lock first.
        std::lock_guard<std::mutex> lock(log_mutex_);

        if (msg_level < min_level_ || min_level_ == LogLevel::OFF) {
            return;
        }

        std::ostream* target_stream = output_stream_;

        // For console loggers (not file loggers):
        // If the default output is std::cout, redirect WARNING level and above to std::cerr.
        if (!is_file_logger_ && output_stream_ == &std::cout && msg_level >= LogLevel::WARN) {
            target_stream = &std::cerr;
        }
        
        if (!target_stream || !target_stream->good()) {
            // If stream is bad (e.g., file closed unexpectedly), try to report to cerr.
            // This is a last resort.
            std::cerr << "[" << get_current_timestamp_() << "] [" << logger_name_ << "] [INTERNAL ERROR] Output stream is not valid." << std::endl;
            return;
        }

        // Construct the full log line: [timestamp] [logger_name] [LEVEL] user_message
        std::ostringstream complete_message_stream;
        complete_message_stream << "[" << get_current_timestamp_() << "] ";
        complete_message_stream << "[" << logger_name_ << "] ";
        complete_message_stream << "[" << level_to_string(msg_level) << "] ";
        complete_message_stream << format_user_message_(args...); // Add user's message parts

        *target_stream << complete_message_stream.str() << std::endl;
    }

    std::string logger_name_;
    LogLevel min_level_;
    
    std::ostream* output_stream_; // Points to std::cout, std::cerr, or file_output_stream_.get()
    std::unique_ptr<std::ofstream> file_output_stream_; // Manages the file stream if created
    bool is_file_logger_;

    mutable std::mutex log_mutex_; // Mutable to allow locking in const methods if they were to access shared mutable state.
                                   // Here, it's primarily for log_() and set_level()/get_level().
};


