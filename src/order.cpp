#include "order.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm> 
#include <cctype>    
#include <iomanip>   
#include <limits>    

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}

std::string trim_whitespace(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c){ return std::isspace(c); }).base();
    return (start < end ? std::string(start, end) : std::string());
}

std::optional<Side> sanitizeSide(const std::string& side_str_raw, Logger& logger, const std::string& original_line) {
    std::string side_str = toUpper(trim_whitespace(side_str_raw));
    if (side_str == "BUY") {
        return Side::BUY;
    }
    if (side_str == "SELL") {
        return Side::SELL;
    }
    logger.warn("Invalid 'side' value: '", side_str_raw, "'. Expected BUY or SELL. Original line: '", original_line, "'");
    return std::nullopt;
}

std::optional<OrderType> sanitizeOrderType(const std::string& type_str_raw, Logger& logger, const std::string& original_line) {
    std::string type_str = toUpper(trim_whitespace(type_str_raw));
    if (type_str == "LIMIT") {
        return OrderType::LIMIT;
    }
    if (type_str == "MARKET") {
        return OrderType::MARKET;
    }
    logger.warn("Invalid 'type' value: '", type_str_raw, "'. Expected LIMIT or MARKET. Original line: '", original_line, "'");
    return std::nullopt;
}

std::optional<OrderAction> sanitizeOrderAction(const std::string& action_str_raw, Logger& logger, const std::string& original_line) {
    std::string action_str = toUpper(trim_whitespace(action_str_raw));
    if (action_str == "NEW") {
        return OrderAction::NEW;
    }
    if (action_str == "MODIFY") {
        return OrderAction::MODIFY;
    }
    if (action_str == "CANCEL") {
        return OrderAction::CANCEL;
    }
    logger.warn("Invalid 'action' value: '", action_str_raw, "'. Expected NEW, MODIFY, or CANCEL. Original line: '", original_line, "'");
    return std::nullopt;
}


std::optional<std::string> get_field_by_header(
    const std::vector<std::string>& fields,
    const std::map<std::string, size_t>& header_map,
    const std::string& header_name,
    Logger& logger) {
    
    auto it = header_map.find(header_name);
    if (it == header_map.end()) {
        logger.warn("Header '", header_name, "' not found in CSV header map."); 
        return std::nullopt; 
    }
    size_t index = it->second;
    if (index >= fields.size()) {
        logger.warn("Index for header '", header_name, "' (", index, ") is out of bounds for current line (", fields.size(), " fields).");
        return std::nullopt;
    }
    return fields[index]; 
}

std::optional<Order> parseCsvLineToOrder(
    const std::vector<std::string>& fields, 
    const std::map<std::string, size_t>& header_map,
    Logger& logger,
    const std::string& original_line 
) {
    Order order; 

    std::optional<std::string> temp_opt_str;

    temp_opt_str = get_field_by_header(fields, header_map, "timestamp", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'timestamp' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    try {
        order.timestamp = std::stoull(*temp_opt_str);
    } catch (const std::invalid_argument& ia) {
        logger.error("Field 'timestamp' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
        return std::nullopt;
    } catch (const std::out_of_range& oor) {
        logger.error("Field 'timestamp' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
        return std::nullopt;
    }

    temp_opt_str = get_field_by_header(fields, header_map, "order_id", logger);
    if (!temp_opt_str) {
         logger.warn("Mandatory field 'order_id' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    try {
        order.order_id = std::stoll(*temp_opt_str);
    } catch (const std::invalid_argument& ia) {
        logger.error("Field 'order_id' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
        return std::nullopt;
    } catch (const std::out_of_range& oor) {
        logger.error("Field 'order_id' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
        return std::nullopt;
    }

    temp_opt_str = get_field_by_header(fields, header_map, "instrument", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'instrument' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    order.instrument = *temp_opt_str; 

    temp_opt_str = get_field_by_header(fields, header_map, "side", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'side' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    std::optional<Side> sanitized_side = sanitizeSide(*temp_opt_str, logger, original_line);
    if (!sanitized_side) return std::nullopt; 
    order.side = *sanitized_side;

    temp_opt_str = get_field_by_header(fields, header_map, "type", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'type' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    std::optional<OrderType> sanitized_type = sanitizeOrderType(*temp_opt_str, logger, original_line);
    if (!sanitized_type) return std::nullopt; 
    order.type = *sanitized_type;

    temp_opt_str = get_field_by_header(fields, header_map, "quantity", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'quantity' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    try {
        order.quantity = std::stoull(*temp_opt_str); 
        if (order.quantity == 0 && (order.action == OrderAction::NEW || order.action == OrderAction::MODIFY)) { 
            logger.warn("Field 'quantity' is zero for a NEW/MODIFY action. This might be invalid. Original line: '", original_line, "'");
        }
    } catch (const std::invalid_argument& ia) {
        logger.error("Field 'quantity' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
        return std::nullopt;
    } catch (const std::out_of_range& oor) {
        logger.error("Field 'quantity' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
        return std::nullopt;
    }

    temp_opt_str = get_field_by_header(fields, header_map, "price", logger);
    if (!temp_opt_str) {
        if (order.type == OrderType::LIMIT && order.action == OrderAction::NEW) { 
            logger.warn("Mandatory field 'price' missing for NEW LIMIT order. Original line: '", original_line, "'");
            return std::nullopt;
        }
        order.price = 0.0; 
    } else { 
        if (order.type == OrderType::MARKET) {
            order.price = 0.0; 
            if (!temp_opt_str->empty() && trim_whitespace(*temp_opt_str) != "0" && trim_whitespace(*temp_opt_str) != "0.0") {
                 logger.debug("Price field value '", *temp_opt_str, "' ignored for MARKET order. Original line: '", original_line, "'");
            }
        } else { 
            try {
                order.price = std::stod(*temp_opt_str);
                if (order.price <= 0 && order.type == OrderType::LIMIT && order.action == OrderAction::NEW) { 
                     logger.warn("Field 'price' for NEW LIMIT order is zero or negative ('", *temp_opt_str, "'). This might be unintentional. Original line: '", original_line, "'");
                }
            } catch (const std::invalid_argument& ia) {
                logger.error("Field 'price' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
                return std::nullopt;
            } catch (const std::out_of_range& oor) {
                logger.error("Field 'price' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
                return std::nullopt;
            }
        }
    }
    
    temp_opt_str = get_field_by_header(fields, header_map, "action", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'action' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    std::optional<OrderAction> sanitized_action = sanitizeOrderAction(*temp_opt_str, logger, original_line);
    if (!sanitized_action) return std::nullopt; 
    order.action = *sanitized_action;

    order.remaining_quantity = order.quantity; 
    order.cumulative_executed_quantity = 0;
    order.status = OrderStatus::UNKNOWN; 

    return order;
}

void readOrdersFromStream(std::istream& stream, Logger& logger, ThreadSafeQueue<Order>& order_queue,
long int  max_queue_size_allowed) {
   
    std::string line;
    long long line_number = 0;
    std::map<std::string, size_t> header_map;

    logger.info("Starting to read orders from stream...");

    if (std::getline(stream, line)) {
        line_number++;
        std::string trimmed_header_line = trim_whitespace(line);
        if (trimmed_header_line.empty()) {
            logger.critical("Header line is empty. Aborting.");
            return;
        }
        logger.info("Reading header line: ", trimmed_header_line);
        std::stringstream header_ss(trimmed_header_line);
        std::string header_field;
        size_t current_index = 0;
        while (std::getline(header_ss, header_field, ',')) {
            std::string trimmed_header_field = trim_whitespace(header_field);
            if (trimmed_header_field.empty()){
                logger.warn("Empty column name found in header at index ", current_index, ". Original header: '", trimmed_header_line, "'");
            }
            header_map[trimmed_header_field] = current_index++;
        }
        if (header_map.empty()) {
            logger.critical("CSV header could not be parsed (all fields might be empty or missing). Aborting.");
            return;
        }
        logger.info("Parsed header. Number of columns: ", header_map.size());
    } else {
        logger.error("Could not read header line from stream (empty file or stream error).");
        return;
    }
    long int order_succcess_parsed = 0;
    while (std::getline(stream, line)) {
        line_number++;
        std::string original_line_for_log = line; 
        
        std::string trimmed_line = trim_whitespace(line);
        if (trimmed_line.empty()) {
            logger.debug("Skipping empty line at number: ", line_number);
            continue;
        }

        std::stringstream data_ss(trimmed_line); 
        std::string field_value;
        std::vector<std::string> fields;
        while (std::getline(data_ss, field_value, ',')) {
            fields.push_back(trim_whitespace(field_value)); 
        }

        if (fields.size() != header_map.size()) {
            if (!fields.empty() || (fields.size() < header_map.size() && header_map.size() > 0) ) { 
                 logger.warn("Malformed data line (field count ", fields.size(), " does not match header count ", header_map.size(), ") at line ", line_number, ". Original line: '", original_line_for_log, "'");
            }
             continue; 
        }
        
        std::optional<Order> parsed_order_opt = parseCsvLineToOrder(fields, header_map, logger, original_line_for_log);
        if (parsed_order_opt) {
            while (order_queue.size() >= static_cast<long unsigned>(max_queue_size_allowed))
            { // could use a condition variable here for better efficiency but reality is that it will not matter much in practice for 1 thread
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wait until space is available
            }
            order_queue.push(*parsed_order_opt); // Push to the thread-safe queue
            order_succcess_parsed++;
        } else {
            logger.warn("Failed to parse order at line: ", line_number, ". See previous errors for details. Original line: '", original_line_for_log, "'");
        }
    }

    logger.info("Finished reading orders. Total lines processed (including header): ", 
        line_number, 
        ". Orders successfully parsed: ", order_succcess_parsed);
  
}
