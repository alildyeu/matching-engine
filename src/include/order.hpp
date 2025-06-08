#pragma once

#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <map>
#include <algorithm> // Required for std::transform for toUpper in cpp, not strictly here
#include <cctype>    // Required for std::toupper for toUpper in cpp, not strictly here
#include "logger.hpp" // Assuming a Logger class is defined for logging
#include "thread_safe_queue.hpp"
#include <thread>
#include <time.h>
#include <atomic>



// Enum for Order Side
enum class Side {
    BUY,
    SELL,
    UNKNOWN // For invalid parsing or default state
};

// Enum for Order Type
enum class OrderType {
    LIMIT,
    MARKET,
    UNKNOWN // For invalid parsing or default state
};

// Enum for Order Action
enum class OrderAction {
    NEW,
    MODIFY,
    CANCEL,
    UNKNOWN // For invalid parsing or default state
};

// Enum for Order Status (used by the matching engine)
enum class OrderStatus {
    PENDING,
    PARTIALLY_EXECUTED,
    EXECUTED,
    CANCELED,
    REJECTED,
    UNKNOWN // Default/error state
};

// Helper functions to convert enums to strings (for printing/logging)
inline std::string sideToString(Side side) {
    switch (side) {
        case Side::BUY: return "BUY";
        case Side::SELL: return "SELL";
        default: return "UNKNOWN_SIDE";
    }
}

inline std::string orderTypeToString(OrderType type) {
    switch (type) {
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::MARKET: return "MARKET";
        default: return "UNKNOWN_TYPE";
    }
}

inline std::string orderActionToString(OrderAction action) {
    switch (action) {
        case OrderAction::NEW: return "NEW";
        case OrderAction::MODIFY: return "MODIFY";
        case OrderAction::CANCEL: return "CANCEL";
        default: return "UNKNOWN_ACTION";
    }
}

inline std::string orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING: return "PENDING";
        case OrderStatus::PARTIALLY_EXECUTED: return "PARTIALLY_EXECUTED";
        case OrderStatus::EXECUTED: return "EXECUTED";
        case OrderStatus::CANCELED: return "CANCELED";
        case OrderStatus::REJECTED: return "REJECTED";
        default: return "UNKNOWN_STATUS";
    }
}

struct Order {
    // Fields from CSV
    unsigned long long timestamp;
    long long order_id;
    std::string instrument;
    Side side;
    OrderType type;
    unsigned long long quantity; // Original total quantity of the order
    double price;
    OrderAction action;

    // Fields for matching engine state tracking
    unsigned long long remaining_quantity;
    unsigned long long cumulative_executed_quantity;
    OrderStatus status;

    // Default constructor
    Order() : timestamp(0), order_id(0), instrument(""), side(Side::UNKNOWN), 
              type(OrderType::UNKNOWN), quantity(0), price(0.0), action(OrderAction::UNKNOWN),
              remaining_quantity(0), cumulative_executed_quantity(0), status(OrderStatus::UNKNOWN) {}

    // Overloaded operator<< for easy printing/logging
    friend std::ostream& operator<<(std::ostream& os, const Order& order) {
        os << "Timestamp: " << order.timestamp
           << ", Order ID: " << order.order_id
           << ", Instrument: " << order.instrument
           << ", Side: " << sideToString(order.side)
           << ", Type: " << orderTypeToString(order.type)
           << ", OrigQty: " << order.quantity // Renamed for clarity if needed, but 'quantity' is fine
           << ", Price: ";
        if (order.type == OrderType::MARKET && order.action != OrderAction::CANCEL) { // Market orders (not cancels) have N/A price for display
            os << "N/A (MARKET)";
        } else {
            os << order.price;
        }
        os << ", Action: " << orderActionToString(order.action)
           << ", Status: " << orderStatusToString(order.status)
           << ", RemQty: " << order.remaining_quantity
           << ", CumExecQty: " << order.cumulative_executed_quantity;
        return os;
    }
};

// Forward declarations for functions in order.cpp (or wherever they are defined)

// Sanitizer functions
std::optional<Side> sanitizeSide(const std::string& side_str_raw, Logger& logger, const std::string& original_line);
std::optional<OrderType> sanitizeOrderType(const std::string& type_str_raw, Logger& logger, const std::string& original_line);
std::optional<OrderAction> sanitizeOrderAction(const std::string& action_str_raw, Logger& logger, const std::string& original_line);

// CSV parsing utility
std::optional<std::string> get_field_by_header(
    const std::vector<std::string>& fields,
    const std::map<std::string, size_t>& header_map,
    const std::string& header_name,
    Logger& logger);

// Main CSV line parser
std::optional<Order> parseCsvLineToOrder(
    const std::vector<std::string>& fields,
    const std::map<std::string, size_t>& header_map,
    Logger& logger,
    const std::string& original_line
);

// Main processing function (if it's considered part of the "order" module)
void readOrdersFromStream(std::istream& stream, Logger& logger, ThreadSafeQueue<Order>& order_queue,
long int  max_queue_size_allowed);

