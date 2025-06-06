#ifndef ARGPARSER_HPP
#define ARGPARSER_HPP

#include <string>
#include <vector>
#include <map>
// #include <variant> // Removed
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <type_traits> // For std::is_same_v

class ArgumentParser;

class ArgumentConfigurator {
public:
    ArgumentParser& parser_ref_;
    size_t argument_index_;

    ArgumentConfigurator(ArgumentParser& parser, size_t arg_idx);

    ArgumentConfigurator& help(const std::string& h);
    ArgumentConfigurator& required(bool req = true);
    ArgumentConfigurator& set_default(const std::string& val);
    ArgumentConfigurator& set_default(int val);
     ArgumentConfigurator& set_default(long int val);
    ArgumentConfigurator& set_default(double val);
    ArgumentConfigurator& set_default(bool val);
    ArgumentConfigurator& action_store_true();
    ArgumentConfigurator& action_store_false();
    ArgumentConfigurator& type_string();
    ArgumentConfigurator& type_int();
    ArgumentConfigurator& type_long_int();
    ArgumentConfigurator& type_double();
};

class ArgumentParser {
public:
    struct Argument {
        std::string dest_name;
        std::vector<std::string> flags;
        std::string help_text;
        bool is_required = false;
        bool is_positional = false;
        bool is_flag_action = false;
        enum class Type { STRING, INT, LONG_INT, DOUBLE, BOOL };
        Type arg_type = Type::STRING;

        // Removed std::variant, using separate members
        std::string string_default_value;
        int int_default_value = 0;
        long int long_int_default_value = 0;
        double double_default_value = 0.0;
        bool bool_default_value = false;

        std::string string_current_value;
        int int_current_value = 0;
        long int long_int_current_value = 0;
        double double_current_value = 0.0;
        bool bool_current_value = false;

        bool has_default = false;
        bool value_is_set = false;

        Argument(std::string d_name, bool pos);
    };

    std::string program_description_;
    std::string program_name_;
    std::vector<Argument> arguments_;
    std::map<std::string, size_t> arg_map_by_dest_;
    std::map<std::string, size_t> arg_map_by_flag_;
    std::vector<size_t> positional_arg_indices_;

    ArgumentParser(std::string description = "");

    ArgumentConfigurator add_argument(const std::string& dest_name);
    ArgumentConfigurator add_flag(const std::vector<std::string>& arg_flags);

    void parse_args(int argc, char* argv[]);

    template<typename T>
    T get(const std::string& dest_name) const;

    bool is_present(const std::string& dest_name) const;
    void print_help() const;

private:
    void add_default_help_argument();
    std::string derive_dest_name(const std::vector<std::string>& arg_flags) const;
    void initialize_default_values();
    void handle_parsed_value(size_t arg_idx, const std::string& val_str);
    void process_boolean_flag(size_t arg_idx, bool val_if_present);
};

template<typename T>
T ArgumentParser::get(const std::string& dest_name) const {
    auto it = arg_map_by_dest_.find(dest_name);
    if (it == arg_map_by_dest_.end()) throw std::runtime_error("Arg not defined: " + dest_name);
    const auto& arg = arguments_[it->second];

    if (!arg.value_is_set) {
         if (arg.has_default) { // Use default value
            if constexpr (std::is_same_v<T, std::string>) {
                if (arg.arg_type == Argument::Type::STRING) return arg.string_default_value;
            } else if constexpr (std::is_same_v<T, int>) {
                if (arg.arg_type == Argument::Type::INT) return arg.int_default_value;
            } else if constexpr (std::is_same_v<T, double>) {
                if (arg.arg_type == Argument::Type::DOUBLE) return arg.double_default_value;
            } else if constexpr (std::is_same_v<T, bool>) {
                if (arg.arg_type == Argument::Type::BOOL) return arg.bool_default_value;
            }
            throw std::runtime_error("Type mismatch for default value of argument '" + dest_name + "'");
         }
         throw std::runtime_error("Arg not provided, no default: " + dest_name);
    }

    // Use current value
    if constexpr (std::is_same_v<T, std::string>) {
        if (arg.arg_type == Argument::Type::STRING) return arg.string_current_value;
    } else if constexpr (std::is_same_v<T, int>) {
        if (arg.arg_type == Argument::Type::INT) return arg.int_current_value;
    } else if constexpr (std::is_same_v<T, double>) {
        if (arg.arg_type == Argument::Type::DOUBLE) return arg.double_current_value;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (arg.arg_type == Argument::Type::BOOL) return arg.bool_current_value;
    }
    throw std::runtime_error("Type mismatch for argument '" + dest_name + "'");
}

#endif // ARGPARSER_HPP
