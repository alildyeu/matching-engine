#include "app_config.hpp"
#include <iostream> // For std::cerr

// Constructor implementation
AppConfig::AppConfig(const std::string& program_description)
    : parser_(program_description) {
    // Define the command-line arguments using the parser_

    // Log level
    parser_.add_flag({"--log-level"})
        .help("Set the logging level (e.g., debug, info, warning, error).")
        .set_default(std::string("info")) // Default is "info"
        .type_string();

    // Log file
    parser_.add_flag({"--log-file"})
        .help("Path to the log file. If not specified or 'none', logs to stdout.")
        .set_default(std::string("")) // Default is empty string for stdout
        .type_string();

    // Order input file (positional)
    parser_.add_argument("order_input_file") // Destination name
        .help("Path to the order input file.")
        .required(true); // This is a required positional argument

    // Order result output file (positional)
    parser_.add_argument("order_result_output_file") // Destination name
        .help("Path for the order result output file.")
        .required(true); // This is a required positional argument

    // Number of jobs
    // parser_.add_flag({"-q", "--queue-size"})
    //     .help("maximum number of jobs in queue between parser and matcher (default: 1000)")
    //     .set_default(1000) // Default queue size is 1000
    //     .type_long_int();
}

// Parse method implementation
bool AppConfig::parse(int argc, char* argv[]) {
    try {
        // The ArgumentParser's parse_args will exit on -h/--help after printing help.
        // If it returns, it means arguments were parsed (or an error other than help occurred).
        parser_.parse_args(argc, argv);

        // If parse_args didn't exit (due to --help) and didn't throw, retrieve values.
        log_level_ = parser_.get<std::string>("log_level"); // dest name from --log-level
        log_file_ = parser_.get<std::string>("log_file");   // dest name from --log-file
        order_input_file_ = parser_.get<std::string>("order_input_file");
        order_result_output_file_ = parser_.get<std::string>("order_result_output_file");
        queue_size_ = 1000; // FIXME later parser_.get<long int>("queue_size");                  

        successfully_parsed_ = true;
        return true;

    } catch (const std::runtime_error& err) {
        // An error occurred during parsing (e.g., missing required arg, type mismatch)
        std::cerr << "Error parsing arguments: " << err.what() << std::endl;
        std::cerr << "-------------------------------------" << std::endl;
        parser_.print_help(); // Print help message on error
        successfully_parsed_ = false;
        return false;
    }
    // Note: If parse_args exits due to --help, this function won't reach here to return.
    // The requirement "print the help in case of error or help menu request" is met
    // by the ArgumentParser's behavior.
}

// Getter implementations
const std::string& AppConfig::get_log_level() const {
    if (!successfully_parsed_) throw std::runtime_error("AppConfig: Arguments not parsed or parsing failed.");
    return log_level_;
}

const std::string& AppConfig::get_log_file() const {
    if (!successfully_parsed_) throw std::runtime_error("AppConfig: Arguments not parsed or parsing failed.");
    return log_file_;
}

bool AppConfig::is_log_to_stdout() const {
    if (!successfully_parsed_) throw std::runtime_error("AppConfig: Arguments not parsed or parsing failed.");
    return log_file_.empty() || log_file_ == "none"; // Consider "none" also as stdout
}

const std::string& AppConfig::get_order_input_file() const {
    if (!successfully_parsed_) throw std::runtime_error("AppConfig: Arguments not parsed or parsing failed.");
    return order_input_file_;
}

const std::string& AppConfig::get_order_result_output_file() const {
    if (!successfully_parsed_) throw std::runtime_error("AppConfig: Arguments not parsed or parsing failed.");
    return order_result_output_file_;
}

long int AppConfig::get_queue_size() const {
    if (!successfully_parsed_) throw std::runtime_error("AppConfig: Arguments not parsed or parsing failed.");
    return queue_size_;
}

