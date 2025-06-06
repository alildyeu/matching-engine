#pragma once


#include <string>
#include <vector>
#include "argparse.hpp" // Assuming argparse.h is in the include path

class AppConfig {
public:
    // Constructor: sets up the argument parser with program description
    AppConfig(const std::string& program_description);

    // Parses command-line arguments.
    // Returns true on successful parsing, false if help was requested or an error occurred.
    // Note: The underlying ArgumentParser exits on --help, so this might not return false for help.
    // The primary role here is to populate members or handle errors.
    bool parse(int argc, char* argv[]);

    // Getter methods for the parsed configuration values
    const std::string& get_log_level() const;
    const std::string& get_log_file() const; // Empty string means log to stdout
    bool is_log_to_stdout() const;
    const std::string& get_order_input_file() const;
    const std::string& get_order_result_output_file() const;
    long int get_queue_size() const;

private:
    ArgumentParser parser_; // The argument parser instance

    // Member variables to store the parsed values
    std::string log_level_;
    std::string log_file_; // An empty string will represent "none" / stdout
    std::string order_input_file_;
    std::string order_result_output_file_;
    long int queue_size_;

    // Flag to indicate if parsing was successful and values are populated
    bool successfully_parsed_ = false;
};

