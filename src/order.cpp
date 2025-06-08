#include "order.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm> 
#include <cctype>    
#include <iomanip>   
#include <limits>    
#include <atomic>



// this class is used to read orders from the CSV input file, parse them into Order objects, and push them into a thread-safe queue.

// 2 Functions to clean the data so it can be read and parsed correctly

// put every character in a string to upper case
std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}

// delete the blank spaces at the beginning and end of a string
std::string trim_whitespace(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c){ return std::isspace(c); }).base();
    return (start < end ? std::string(start, end) : std::string());
}

// Function to convert a raw string ("buy", "SELL"...) into a valid "Side" enum.
std::optional<Side> sanitizeSide(const std::string& side_str_raw, Logger& logger, const std::string& original_line) {
    std::string side_str = toUpper(trim_whitespace(side_str_raw));
    if (side_str == "BUY") {
        return Side::BUY;
    }
    if (side_str == "SELL") {
        return Side::SELL;
    }
    // If the side is not recognized, log a warning and return std::nullopt
    logger.warn("Invalid 'side' value: '", side_str_raw, "'. Expected BUY or SELL. Original line: '", original_line, "'");
    return std::nullopt;
}


// Function to convert a raw string ("LIMIT", "MARKET"...) into a valid "OrderType" enum
std::optional<OrderType> sanitizeOrderType(const std::string& type_str_raw, Logger& logger, const std::string& original_line) {
    std::string type_str = toUpper(trim_whitespace(type_str_raw));
    // Check for valid order types
    if (type_str == "LIMIT") {
        return OrderType::LIMIT;
    }
    if (type_str == "MARKET") {
        return OrderType::MARKET;
    }
    // If the type is not recognized, log a warning and return std::nullopt
    logger.warn("Invalid 'type' value: '", type_str_raw, "'. Expected LIMIT or MARKET. Original line: '", original_line, "'");
    return std::nullopt;
}


// Function to convert a raw string ("NEW", "MODIFY", "CANCEL"...) into a valid "OrderAction" enum
std::optional<OrderAction> sanitizeOrderAction(const std::string& action_str_raw, Logger& logger, const std::string& original_line) {
    std::string action_str = toUpper(trim_whitespace(action_str_raw));
    //check for valid order actions
    if (action_str == "NEW") {
        return OrderAction::NEW;
    }
    if (action_str == "MODIFY") {
        return OrderAction::MODIFY;
    }
    if (action_str == "CANCEL") {
        return OrderAction::CANCEL;
    }
    // If the action is not recognized, log a warning and return std::nullopt
    logger.warn("Invalid 'action' value: '", action_str_raw, "'. Expected NEW, MODIFY, or CANCEL. Original line: '", original_line, "'");
    return std::nullopt;
}

// Returns the value of a CSV field corresponding to a given header name.

std::optional<std::string> get_field_by_header(
    // The values of the current CSV line, split by commas
    const std::vector<std::string>& fields,
    // map the header names to their indices in the CSV line
    const std::map<std::string, size_t>& header_map,

    const std::string& header_name,
    Logger& logger) {
    
    auto it = header_map.find(header_name);
    // if we dont find the header name in the csv, it returns an error (nullopt)
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

// this function reads a CSV line already split into fields,
// and converts them into an Order structure
std::optional<Order> parseCsvLineToOrder(
    const std::vector<std::string>& fields, 
    const std::map<std::string, size_t>& header_map,
    Logger& logger,
    const std::string& original_line 
) {

    //declare an order object. for now it is empty
    Order order; 

    std::optional<std::string> temp_opt_str;

    // read the timestamp field in the CSV line
    temp_opt_str = get_field_by_header(fields, header_map, "timestamp", logger);
    if (!temp_opt_str) {
        // verify that the field is here
        logger.warn("Mandatory field 'timestamp' missing or an issue with header mapping. Original line: '", original_line, "'");
        // if missing, we abandon the parsing of this line
        return std::nullopt;
    }

    try {
    // stoull : convert a string to an unsigned long long
    // unsigned = non negative values
    // long long = can have very big number
        order.timestamp = std::stoull(*temp_opt_str);
    // if the string cant be converted to a big number
    } catch (const std::invalid_argument& ia) {
        logger.error("Field 'timestamp' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
        return std::nullopt;
    // if the string is too big to be converted to an unsigned long long
    } catch (const std::out_of_range& oor) {
        logger.error("Field 'timestamp' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
        return std::nullopt;
    }

    // read the order_id field in the CSV line
    temp_opt_str = get_field_by_header(fields, header_map, "order_id", logger);
    // check validity
    if (!temp_opt_str) {
         logger.warn("Mandatory field 'order_id' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    // stoll : convert the string to a long long
    // signed long long so it can have negative values
    try {
        order.order_id = std::stoll(*temp_opt_str);
    // if the string can't be converted to a long long
    } catch (const std::invalid_argument& ia) {
        logger.error("Field 'order_id' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
        return std::nullopt;
    // if the string is too big to be converted to a long long
    } catch (const std::out_of_range& oor) {
        logger.error("Field 'order_id' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
        return std::nullopt;
    }

    // read the instrument field in the CSV line
    temp_opt_str = get_field_by_header(fields, header_map, "instrument", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'instrument' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    // we have access to the string in the temp_op_str
    // and we affect it to the order.instrument object
    order.instrument = *temp_opt_str; 

    // check for the 'side' field in the CSV line
    temp_opt_str = get_field_by_header(fields, header_map, "side", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'side' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    // call sanitizeSide to clean the string
    std::optional<Side> sanitized_side = sanitizeSide(*temp_opt_str, logger, original_line);
    if (!sanitized_side) return std::nullopt; 
    // affect the value to the order.side object
    order.side = *sanitized_side;

    // same function but for the 'type' field
    temp_opt_str = get_field_by_header(fields, header_map, "type", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'type' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    std::optional<OrderType> sanitized_type = sanitizeOrderType(*temp_opt_str, logger, original_line);
    if (!sanitized_type) return std::nullopt; 
    order.type = *sanitized_type;

    // read the 'quantity' field in the CSV line
    temp_opt_str = get_field_by_header(fields, header_map, "quantity", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'quantity' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    //convert to unsigned long long (cant be negative)
    try {
        order.quantity = std::stoull(*temp_opt_str); 
        // if quantity is zero, and we have a NEW or MODIFY action : raise a warning because its not normal
        if (order.quantity == 0 && (order.action == OrderAction::NEW || order.action == OrderAction::MODIFY)) { 
            logger.warn("Field 'quantity' is zero for a NEW/MODIFY action. This might be invalid. Original line: '", original_line, "'");
        }
    } catch (const std::invalid_argument& ia) {
        logger.error("Field 'quantity' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
        return std::nullopt;
    // if value too big to be converted to an unsigned long long
    } catch (const std::out_of_range& oor) {
        logger.error("Field 'quantity' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
        return std::nullopt;
    }

    // read the 'price' field in the CSV line
    temp_opt_str = get_field_by_header(fields, header_map, "price", logger);
    if (!temp_opt_str) {
        // if LIMIT order, we need the price field
        if (order.type == OrderType::LIMIT && order.action == OrderAction::NEW) { 
            logger.warn("Mandatory field 'price' missing for NEW LIMIT order. Original line: '", original_line, "'");
            return std::nullopt;
        }
        order.price = 0.0; 
    } else { 
        // if its a MARKET order, we ignore the price field
        // because the order will be executed at the best price available in the book
        if (order.type == OrderType::MARKET) {
            // set price to 0
            order.price = 0.0; 
            if (!temp_opt_str->empty() && trim_whitespace(*temp_opt_str) != "0" && trim_whitespace(*temp_opt_str) != "0.0") {
                 logger.debug("Price field value '", *temp_opt_str, "' ignored for MARKET order. Original line: '", original_line, "'");
            }
        } else { 
            try {
                //stod : convert a string to a double
                order.price = std::stod(*temp_opt_str);
                // if the price is zero/negative, and we have a NEW LIMIT order, we raise a warning
                if (order.price <= 0 && order.type == OrderType::LIMIT && order.action == OrderAction::NEW) { 
                     logger.warn("Field 'price' for NEW LIMIT order is zero or negative ('", *temp_opt_str, "'). This might be unintentional. Original line: '", original_line, "'");
                }
            // if the string can't be converted to a double
            } catch (const std::invalid_argument& ia) {
                logger.error("Field 'price' with value '", *temp_opt_str, "' cannot be converted: invalid argument. Original line: '", original_line, "'. Details: ", ia.what());
                return std::nullopt;
            } catch (const std::out_of_range& oor) {
                logger.error("Field 'price' with value '", *temp_opt_str, "' cannot be converted: out of range. Original line: '", original_line, "'. Details: ", oor.what());
                return std::nullopt;
            }
        }
    }
    

    // read the action field from the CSV line
    temp_opt_str = get_field_by_header(fields, header_map, "action", logger);
    if (!temp_opt_str) {
        logger.warn("Mandatory field 'action' missing or an issue with header mapping. Original line: '", original_line, "'");
        return std::nullopt;
    }
    // cleaning
    std::optional<OrderAction> sanitized_action = sanitizeOrderAction(*temp_opt_str, logger, original_line);
    if (!sanitized_action) return std::nullopt; 
    order.action = *sanitized_action;

    // Initialize remaining_quantity and cumulative_executed_quantity
    // trhough time, orders are executed, so the remaining quantity will decrease
    order.remaining_quantity = order.quantity; 
    order.cumulative_executed_quantity = 0;
    order.status = OrderStatus::UNKNOWN; 

    return order;
}

// This function reads orders from a CSV file, line by line
// creates Order objects from the parsed lines,
// and pushes them into a thread-safe queue for further processing.
/// istream : representes an input stream
void readOrdersFromStream(std::istream& stream, Logger& logger, ThreadSafeQueue<Order>& order_queue,
long int  max_queue_size_allowed) {
   
    // declare a line, wich will contain every line read from the CSV
    std::string line;
    //count number of lines
    long long line_number = 0;
    //map to associate header namees to their index (position in the CSV)
    std::map<std::string, size_t> header_map;

    logger.info("Starting to read orders from stream...");

    // First part of the function reads the header of the csv
    // read the first line of the stream (the csv file)
    if (std::getline(stream, line)) {
        //incrementation of the lines
        line_number++;
        //cleaning
        std::string trimmed_header_line = trim_whitespace(line);
        if (trimmed_header_line.empty()) {
            logger.critical("Header line is empty. Aborting.");
            return;
        }
        logger.info("Reading header line: ", trimmed_header_line);

        //stringstream is used to split the header line
        std::stringstream header_ss(trimmed_header_line);
        // variable to hold each field in the header
        std::string header_field;
        size_t current_index = 0;
        // read each field in the header line (separated by ,)
        while (std::getline(header_ss, header_field, ',')) {
            std::string trimmed_header_field = trim_whitespace(header_field);
            if (trimmed_header_field.empty()){
                logger.warn("Empty column name found in header at index ", current_index, ". Original header: '", trimmed_header_line, "'");
            }
            // store the column name and its index in the header_map
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

    // Second part of the function reads the rest of the csv and transforms them in Orders
    // count the number of orders
    long int order_succcess_parsed = 0;
    // as long as we can read lines from the stream, we read the next line
    while (std::getline(stream, line)) {
        line_number++;
        std::string original_line_for_log = line; 
        //clean
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
        // check that the number of fields in the line matches the header count
        if (fields.size() != header_map.size()) {
            if (!fields.empty() || (fields.size() < header_map.size() && header_map.size() > 0) ) { 
                 logger.warn("Malformed data line (field count ", fields.size(), " does not match header count ", header_map.size(), ") at line ", line_number, ". Original line: '", original_line_for_log, "'");
            }
             continue; 
        }
        
        // convert the current line in an Order
        std::optional<Order> parsed_order_opt = parseCsvLineToOrder(fields, header_map, logger, original_line_for_log);
        if (parsed_order_opt) {
            // if the queue is full, we wait
            while (order_queue.size() >= static_cast<long unsigned>(max_queue_size_allowed))
            { 
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
        "Orders successfully parsed: ", order_succcess_parsed);
  
}
