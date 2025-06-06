#include "argparse.hpp"

ArgumentConfigurator::ArgumentConfigurator(ArgumentParser& parser, size_t arg_idx) : parser_ref_(parser), argument_index_(arg_idx) {}

ArgumentConfigurator& ArgumentConfigurator::help(const std::string& h) { parser_ref_.arguments_[argument_index_].help_text = h; return *this; }
ArgumentConfigurator& ArgumentConfigurator::required(bool req) { parser_ref_.arguments_[argument_index_].is_required = req; if(parser_ref_.arguments_[argument_index_].is_positional) parser_ref_.arguments_[argument_index_].is_required = true; return *this; }
ArgumentConfigurator& ArgumentConfigurator::set_default(const std::string& val) {
    auto& arg = parser_ref_.arguments_[argument_index_];
    arg.string_default_value = val;
    arg.string_current_value = val; 
    arg.has_default = true;
    arg.arg_type = ArgumentParser::Argument::Type::STRING;
    return *this;
}
ArgumentConfigurator& ArgumentConfigurator::set_default(int val) {
    auto& arg = parser_ref_.arguments_[argument_index_];
    arg.int_default_value = val;
    arg.int_current_value = val;
    arg.has_default = true;
    arg.arg_type = ArgumentParser::Argument::Type::INT;
    return *this;
}
ArgumentConfigurator& ArgumentConfigurator::set_default(long int val) {
    auto& arg = parser_ref_.arguments_[argument_index_];
    arg.long_int_default_value = val;
    arg.long_int_current_value = val;
    arg.has_default = true;
    arg.arg_type = ArgumentParser::Argument::Type::LONG_INT;
    return *this;
}
ArgumentConfigurator& ArgumentConfigurator::set_default(double val) {
    auto& arg = parser_ref_.arguments_[argument_index_];
    arg.double_default_value = val;
    arg.double_current_value = val;
    arg.has_default = true;
    arg.arg_type = ArgumentParser::Argument::Type::DOUBLE;
    return *this;
}
ArgumentConfigurator& ArgumentConfigurator::set_default(bool val) {
    auto& arg = parser_ref_.arguments_[argument_index_];
    arg.bool_default_value = val;
    arg.bool_current_value = val;
    arg.has_default = true;
    arg.arg_type = ArgumentParser::Argument::Type::BOOL;
    return *this;
}
ArgumentConfigurator& ArgumentConfigurator::action_store_true() {
    auto& arg = parser_ref_.arguments_[argument_index_];
    arg.arg_type = ArgumentParser::Argument::Type::BOOL;
    arg.is_flag_action = true;
    arg.bool_default_value = false;
    arg.bool_current_value = false;
    arg.has_default = true;
    return *this;
}
ArgumentConfigurator& ArgumentConfigurator::action_store_false() {
    auto& arg = parser_ref_.arguments_[argument_index_];
    arg.arg_type = ArgumentParser::Argument::Type::BOOL;
    arg.is_flag_action = true;
    arg.bool_default_value = true;
    arg.bool_current_value = true;
    arg.has_default = true;
    return *this;
}
ArgumentConfigurator& ArgumentConfigurator::type_string() { parser_ref_.arguments_[argument_index_].arg_type = ArgumentParser::Argument::Type::STRING; return *this; }
ArgumentConfigurator& ArgumentConfigurator::type_int() { parser_ref_.arguments_[argument_index_].arg_type = ArgumentParser::Argument::Type::INT; return *this; }
ArgumentConfigurator& ArgumentConfigurator::type_long_int() { parser_ref_.arguments_[argument_index_].arg_type = ArgumentParser::Argument::Type::LONG_INT; return *this; }

ArgumentConfigurator& ArgumentConfigurator::type_double() { parser_ref_.arguments_[argument_index_].arg_type = ArgumentParser::Argument::Type::DOUBLE; return *this; }

ArgumentParser::Argument::Argument(std::string d_name, bool pos) : dest_name(std::move(d_name)), is_positional(pos) {
    if (is_positional) flags.push_back(dest_name);
}

ArgumentParser::ArgumentParser(std::string description) : program_description_(std::move(description)) {
    add_default_help_argument();
}

void ArgumentParser::add_default_help_argument() {
    arguments_.emplace_back("help", false);
    size_t idx = arguments_.size() - 1;
    auto& help_arg = arguments_[idx];
    help_arg.flags = {"-h", "--help"};
    help_arg.help_text = "Show this help message and exit.";
    help_arg.arg_type = Argument::Type::BOOL;
    help_arg.is_flag_action = true;
    help_arg.bool_default_value = false;
    help_arg.bool_current_value = false;
    help_arg.has_default = true;
    arg_map_by_dest_["help"] = idx;
    for (const auto& flag : help_arg.flags) arg_map_by_flag_[flag] = idx;
}

std::string ArgumentParser::derive_dest_name(const std::vector<std::string>& arg_flags) const {
    std::string dest_candidate;
    for (const auto& flag : arg_flags) {
        if (flag.rfind("--", 0) == 0) {
            dest_candidate = flag.substr(2);
            std::replace(dest_candidate.begin(), dest_candidate.end(), '-', '_');
            return dest_candidate;
        }
    }
    for (const auto& flag : arg_flags) {
        if (flag.rfind("-", 0) == 0 && flag.length() > 1 && flag[1] != '-') {
            dest_candidate = flag.substr(1);
            if (!dest_candidate.empty() && std::isalpha(dest_candidate[0])) return dest_candidate;
        }
    }
    if (!arg_flags.empty()) {
        std::string temp = arg_flags[0];
        size_t first_char = temp.find_first_not_of('-');
        if (first_char != std::string::npos) temp = temp.substr(first_char);
        std::replace(temp.begin(), temp.end(), '-', '_');
        if (!temp.empty()) return temp;
    }
    throw std::runtime_error("Cannot derive dest_name from flags.");
}

ArgumentConfigurator ArgumentParser::add_argument(const std::string& dest_name) {
    if (arg_map_by_dest_.count(dest_name)) throw std::runtime_error("Dest name '" + dest_name + "' already defined.");
    arguments_.emplace_back(dest_name, true);
    size_t idx = arguments_.size() - 1;
    arg_map_by_dest_[dest_name] = idx;
    positional_arg_indices_.push_back(idx);
    return ArgumentConfigurator(*this, idx);
}

ArgumentConfigurator ArgumentParser::add_flag(const std::vector<std::string>& arg_flags) {
    if (arg_flags.empty()) throw std::runtime_error("Optional argument needs flags.");
    for (const auto& flag : arg_flags) if (arg_map_by_flag_.count(flag)) throw std::runtime_error("Flag '" + flag + "' redefined.");
    std::string dest_name = derive_dest_name(arg_flags);
    if (arg_map_by_dest_.count(dest_name) && dest_name != "help") throw std::runtime_error("Dest name '" + dest_name + "' redefined.");
    arguments_.emplace_back(dest_name, false);
    size_t idx = arguments_.size() - 1;
    arguments_[idx].flags = arg_flags;
    arg_map_by_dest_[dest_name] = idx;
    for (const auto& flag : arg_flags) arg_map_by_flag_[flag] = idx;
    return ArgumentConfigurator(*this, idx);
}

void ArgumentParser::initialize_default_values() {
    for (auto& arg : arguments_) {
        if (arg.has_default) {
            switch (arg.arg_type) {
                case Argument::Type::STRING: arg.string_current_value = arg.string_default_value; break;
                case Argument::Type::INT:    arg.int_current_value = arg.int_default_value;       break;
                case Argument::Type::LONG_INT:    arg.long_int_current_value = arg.long_int_default_value;       break;

                case Argument::Type::DOUBLE: arg.double_current_value = arg.double_default_value; break;
                case Argument::Type::BOOL:   arg.bool_current_value = arg.bool_default_value;     break;
            }
        }
    }
}

void ArgumentParser::handle_parsed_value(size_t arg_idx, const std::string& val_str) {
    Argument& arg = arguments_[arg_idx];
    try {
        switch (arg.arg_type) {
            case Argument::Type::STRING: arg.string_current_value = val_str; break;
            case Argument::Type::INT:    arg.int_current_value = std::stoi(val_str); break;
            case Argument::Type::LONG_INT:    arg.long_int_current_value = std::stoll(val_str); break;

            case Argument::Type::DOUBLE: arg.double_current_value = std::stod(val_str); break;
            case Argument::Type::BOOL:
                if (val_str == "true" || val_str == "1") arg.bool_current_value = true;
                else if (val_str == "false" || val_str == "0") arg.bool_current_value = false;
                else throw std::invalid_argument("Bad bool value");
                break;
        }
        arg.value_is_set = true;
    } catch (const std::exception& e) {
        std::string flag_name = arg.is_positional ? arg.dest_name : arg.flags[0];
        throw std::runtime_error("Invalid value for " + flag_name + ": '" + val_str + "'. " + e.what());
    }
}

void ArgumentParser::process_boolean_flag(size_t arg_idx, bool val_if_present) {
    Argument& arg = arguments_[arg_idx];
    if (!arg.is_flag_action || arg.arg_type != Argument::Type::BOOL) {
        throw std::runtime_error("Internal: process_boolean_flag called on non-boolean-flag " + arg.dest_name);
    }
    arg.bool_current_value = val_if_present;
    arg.value_is_set = true;
}

void ArgumentParser::parse_args(int argc, char* argv[]) {
    program_name_ = argv[0];
    initialize_default_values();
    std::vector<std::string> tokens;
    for (int i = 1; i < argc; ++i) tokens.push_back(argv[i]);
    size_t current_pos_idx = 0;
    bool double_dash = false;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        if (token == "--") { double_dash = true; continue; }
        if (!double_dash && (token == "-h" || token == "--help")) { print_help(); std::exit(0); }

        if (!double_dash && token.rfind("-", 0) == 0) {
            auto it = arg_map_by_flag_.find(token);
            if (it == arg_map_by_flag_.end()) {
                if (token.length() > 2 && token[0] == '-' && token[1] != '-') {
                    bool all_bool = true; std::vector<size_t> short_indices;
                    for (size_t j = 1; j < token.length(); ++j) {
                        auto it_s = arg_map_by_flag_.find("-" + std::string(1, token[j]));
                        if (it_s == arg_map_by_flag_.end() || !arguments_[it_s->second].is_flag_action) { all_bool = false; break; }
                        short_indices.push_back(it_s->second);
                    }
                    if (all_bool && !short_indices.empty()) {
                        for (size_t s_idx : short_indices) process_boolean_flag(s_idx, !arguments_[s_idx].bool_default_value);
                        continue;
                    }
                }
                throw std::runtime_error("Unknown option: " + token);
            }
            size_t arg_idx = it->second; Argument& arg_def = arguments_[arg_idx];
            if (arg_def.is_flag_action) process_boolean_flag(arg_idx, !arg_def.bool_default_value);
            else {
                if (++i >= tokens.size()) throw std::runtime_error(token + " needs a value.");
                handle_parsed_value(arg_idx, tokens[i]);
            }
        } else {
            if (current_pos_idx >= positional_arg_indices_.size()) throw std::runtime_error("Too many positional args: " + token);
            handle_parsed_value(positional_arg_indices_[current_pos_idx++], token);
        }
    }
    for (auto& arg : arguments_) {
        if (arg.is_required && !arg.value_is_set) throw std::runtime_error("Required arg missing: " + (arg.is_positional ? arg.dest_name : arg.flags[0]));
        if (!arg.value_is_set && arg.has_default) arg.value_is_set = true;
    }
}

bool ArgumentParser::is_present(const std::string& dest_name) const {
    auto it = arg_map_by_dest_.find(dest_name);
    if (it == arg_map_by_dest_.end()) return false;
    const auto& arg = arguments_[it->second];
    if (arg.is_flag_action) { 
        return arg.bool_current_value != arg.bool_default_value;
    }
    return arg.value_is_set;
}

void ArgumentParser::print_help() const {
    std::string name = program_name_;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    std::cout << "Usage: " << name << " [options]";
    for (size_t idx : positional_arg_indices_) std::cout << " " << (arguments_[idx].is_required ? "" : "[") << arguments_[idx].dest_name << (arguments_[idx].is_required ? "" : "]");
    std::cout << std::endl;
    if (!program_description_.empty()) std::cout << "\n" << program_description_ << std::endl;
    
    auto print_args_section = [&](const std::string& title, bool is_positional_section) {
        std::cout << "\n" << title << ":" << std::endl;
        for (const auto& arg : arguments_) {
            if (arg.is_positional != is_positional_section) continue;
            std::string flags_display_str = is_positional_section ? arg.dest_name : "";
            if (!is_positional_section) for(size_t i=0; i<arg.flags.size(); ++i) flags_display_str += arg.flags[i] + (i < arg.flags.size()-1 ? ", ":"");
            if (!arg.is_flag_action && !is_positional_section) flags_display_str += " <" + arg.dest_name + ">";
            std::cout << "  " << std::left << std::setw(25) << flags_display_str << arg.help_text;
            if (arg.has_default) {
                std::cout << " (default: ";
                switch(arg.arg_type) {
                    case Argument::Type::STRING: std::cout << "\"" << arg.string_default_value << "\""; break;
                    case Argument::Type::INT:    std::cout << arg.int_default_value; break;
                    case Argument::Type::LONG_INT:    std::cout << arg.int_default_value; break;

                    case Argument::Type::DOUBLE: std::cout << arg.double_default_value; break;
                    case Argument::Type::BOOL:   std::cout << (arg.bool_default_value ? "true":"false"); break;
                }
                std::cout << ")";
            }
            std::cout << std::endl;
        }
    };
    print_args_section("Positional arguments", true);
    print_args_section("Optional arguments", false);
}
