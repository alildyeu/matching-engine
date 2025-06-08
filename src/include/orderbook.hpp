#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>
#include <functional> // For std::greater
#include <iostream>   // For ostream in OutputRecord, cerr
#include <algorithm>  // For std::min
#include <set>        // For ids_traded_this_event_
#include <atomic>

// Assumes order.hpp defines:
// - struct Order (with all fields: timestamp, order_id, instrument, side, type, quantity, price, action,
//                 remaining_quantity, cumulative_executed_quantity, status)
// - enum class Side, OrderType, OrderAction, OrderStatus
// - helper functions: sideToString, orderTypeToString, orderActionToString, orderStatusToString
#include "order.hpp" 

// Structure for CSV output, matching the PDF specification
struct OutputRecord {
    unsigned long long timestamp;
    long long order_id;
    std::string instrument;
    std::string side_str; 
    std::string type_str; 
    unsigned long long original_quantity_or_remaining; // Renamed for clarity based on usage
    double price; 
    std::string action_str; 
    std::string status_str; 
    unsigned long long executed_this_event_quantity; 
    double execution_price;
    long long counterparty_id; 

    OutputRecord(unsigned long long ts, long long oid, const std::string& instr,
                 Side s, OrderType ot, unsigned long long oq_or_rem, double p, OrderAction oa,
                 OrderStatus st, unsigned long long eq, double ep, long long cpid)
        : timestamp(ts), order_id(oid), instrument(instr),
          side_str(sideToString(s)), type_str(orderTypeToString(ot)),
          original_quantity_or_remaining(oq_or_rem), price(p), action_str(orderActionToString(oa)),
          status_str(orderStatusToString(st)), executed_this_event_quantity(eq),
          execution_price(ep), counterparty_id(cpid) {}

    friend std::ostream& operator<<(std::ostream& os, const OutputRecord& orc) {
        os << orc.timestamp << ","
           << orc.order_id << ","
           << orc.instrument << ","
           << orc.side_str << ","
           << orc.type_str << ","
           << orc.original_quantity_or_remaining << "," // This is the 6th column
           << orc.price << ","
           << orc.action_str << ","
           << orc.status_str << ","
           << orc.executed_this_event_quantity << ","
           << orc.execution_price << ","
           << orc.counterparty_id;
        return os;
    }
};


class OrderBook {
public:
    explicit OrderBook(const std::string& instrument_name);

    
    void startProcessingThread(){
        processing_thread_ = std::thread([this]() {
            while (!stop_processing_ || !order_queue_.empty()) {
                Order order;
                if (order_queue_.try_pop(order)) {
                    processSingleOrder(order);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Sleep briefly to avoid busy-waiting
                }
            }
        });
    }

    void set_output_log_queue(std::shared_ptr<ThreadSafeQueue<std::string>>& output_log_queue) {
        output_log_queue_ = output_log_queue; // Set the output log queue for logging output records
    }

    void stopProcessingThread() {
        stop_processing_ = true;
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
    }

    void addOrder(Order &order){
        order_queue_.push(std::move(order)); // Push the order into the thread-safe queue
    }

    const std::vector<OutputRecord>& getOutputRecords() const;
    const std::string& getInstrumentName() const;
    void printOrderBookSnapshot() const;

private:
std::shared_ptr<ThreadSafeQueue<std::string>> output_log_queue_; // Optional: for logging output records in a thread-safe manner
    ThreadSafeQueue<Order> order_queue_; // Thread-safe queue for incoming orders
    std::atomic<bool> stop_processing_{false}; // Flag to stop processing thread
    std::thread processing_thread_; // Thread for processing orders
    void processSingleOrder(Order& order);
    std::string instrument_name_;
    std::map<double, std::list<Order>, std::greater<double>> bids_; 
    std::map<double, std::list<Order>> asks_;                     
  
    std::set<long long> ids_traded_this_event_; 

    // Corrected declarations:
    void matchOrders(unsigned long long event_timestamp);

    void recordMatchAndCreateOutput(Order& aggressive_order, Order& passive_order,
                                    unsigned long long matched_qty, double match_price,
                                    unsigned long long event_timestamp); // Added event_timestamp
    
    void addInitialOutputRecord(const Order& order_state_for_log, OrderStatus status_to_log, 
                                unsigned long long executed_qty_this_event, 
                                double exec_price, long long counterparty,
                                unsigned long long event_timestamp); // Added event_timestamp
};
