#include "orderbook.hpp" 
#include <iostream> 
#include <algorithm> 
#include <vector>    
#include <set>       

OrderBook::OrderBook(const std::string& instrument_name)
    : instrument_name_(instrument_name) {
}

void OrderBook::addInitialOutputRecord(const Order& order_state_for_log, OrderStatus status_to_log, 
    unsigned long long executed_qty_this_event, double exec_price, long long counterparty, 
    unsigned long long event_timestamp) {
    unsigned long long quantity_for_output_column = 0;
    if (status_to_log == OrderStatus::PENDING || status_to_log == OrderStatus::REJECTED) {
        quantity_for_output_column = order_state_for_log.quantity; // Original total quantity
    } else if (status_to_log == OrderStatus::PARTIALLY_EXECUTED) {
        quantity_for_output_column = order_state_for_log.remaining_quantity;
    } else { // EXECUTED, CANCELED
        quantity_for_output_column = 0;
    }

    double price_for_output_column = (status_to_log == OrderStatus::CANCELED) ? 0.0 : order_state_for_log.price;
    auto outputrec = OutputRecord(
        event_timestamp, // Use the provided event_timestamp
        order_state_for_log.order_id,
        instrument_name_, 
        order_state_for_log.side,
        order_state_for_log.type,
        quantity_for_output_column, 
        price_for_output_column,    
        order_state_for_log.action,   
        status_to_log,  
        executed_qty_this_event,   
        exec_price,     
        counterparty    
    );
    std::stringstream ss;
    ss << outputrec;
    output_log_queue_->push(ss.str()); // Push the output record to the thread-safe queue
}

void OrderBook::recordMatchAndCreateOutput(Order& aggressive_order, Order& passive_order,
                                        unsigned long long matched_qty, double match_price, unsigned long long event_timestamp) {
    aggressive_order.remaining_quantity -= matched_qty;
    aggressive_order.cumulative_executed_quantity += matched_qty;
    aggressive_order.status = (aggressive_order.remaining_quantity == 0) ? OrderStatus::EXECUTED : OrderStatus::PARTIALLY_EXECUTED;

    passive_order.remaining_quantity -= matched_qty;
    passive_order.cumulative_executed_quantity += matched_qty;
    passive_order.status = (passive_order.remaining_quantity == 0) ? OrderStatus::EXECUTED : OrderStatus::PARTIALLY_EXECUTED;

    ids_traded_this_event_.insert(aggressive_order.order_id);
    ids_traded_this_event_.insert(passive_order.order_id);

    unsigned long long aggressive_qty_for_output = (aggressive_order.status == OrderStatus::EXECUTED) ? 0 : aggressive_order.remaining_quantity;
    unsigned long long passive_qty_for_output = (passive_order.status == OrderStatus::EXECUTED) ? 0 : passive_order.remaining_quantity;

    auto outputrec1 = OutputRecord(
        event_timestamp, 
        aggressive_order.order_id, 
        instrument_name_,
        aggressive_order.side, 
        aggressive_order.type, 
        aggressive_qty_for_output, 
        aggressive_order.price,    
        aggressive_order.action,   
        aggressive_order.status,   
        matched_qty, 
        match_price, 
        passive_order.order_id
    );

    auto outputrec2 = OutputRecord(
        event_timestamp, 
        passive_order.order_id, 
        instrument_name_,
        passive_order.side, 
        passive_order.type, 
        passive_qty_for_output, 
        passive_order.price,    
        passive_order.action,   
        passive_order.status,   
        matched_qty, 
        match_price, 
        aggressive_order.order_id
    );

    std::stringstream ss1;
    std::stringstream ss2;
    ss1 << outputrec1;
    ss2 << outputrec2;
    output_log_queue_->push(ss1.str()); // Push the output record to the thread-safe queue
    output_log_queue_->push(ss2.str()); // Push the output record to the thread-safe queue

}

void OrderBook::processSingleOrder(Order& incoming_order_request) { 
    unsigned long long current_event_timestamp = incoming_order_request.timestamp;

    if (incoming_order_request.instrument != instrument_name_) {
        std::cerr << "Error: Order " << incoming_order_request.order_id << " for instrument " << incoming_order_request.instrument
                  << " sent to OrderBook for " << instrument_name_ << std::endl;
        addInitialOutputRecord(incoming_order_request, OrderStatus::REJECTED, 0, 0.0, 0, current_event_timestamp);
        return;
    }

    if (incoming_order_request.action == OrderAction::NEW) {
        incoming_order_request.remaining_quantity = incoming_order_request.quantity;
        incoming_order_request.cumulative_executed_quantity = 0;
        incoming_order_request.status = OrderStatus::PENDING; 
    }

    ids_traded_this_event_.clear(); 

    if (incoming_order_request.action == OrderAction::NEW) {
        Order order_to_process = incoming_order_request; 

        if (order_to_process.type == OrderType::LIMIT) {
            if (order_to_process.side == Side::BUY) {
                bids_[order_to_process.price].push_back(order_to_process);
            } else { 
                asks_[order_to_process.price].push_back(order_to_process);
            }
            addInitialOutputRecord(order_to_process, OrderStatus::PENDING, 0, 0.0, 0, current_event_timestamp);
            matchOrders(current_event_timestamp); 
        } else if (order_to_process.type == OrderType::MARKET) {
            unsigned long long initial_market_order_qty = order_to_process.remaining_quantity;

            if (order_to_process.side == Side::BUY) {
                while (order_to_process.remaining_quantity > 0 && !asks_.empty()) {
                    auto best_ask_level_iter = asks_.begin(); 
                    std::list<Order>& orders_at_best_ask = best_ask_level_iter->second;
                    
                    if (orders_at_best_ask.empty()){ 
                        asks_.erase(best_ask_level_iter); 
                        continue;
                    }
                    Order& resting_sell_order = orders_at_best_ask.front(); 
                    
                    double match_price = resting_sell_order.price; 
                    unsigned long long match_qty = std::min(order_to_process.remaining_quantity, resting_sell_order.remaining_quantity);

                    recordMatchAndCreateOutput(order_to_process, resting_sell_order, match_qty, match_price, current_event_timestamp);

                    if (resting_sell_order.remaining_quantity == 0) {
                        orders_at_best_ask.pop_front();
                        if (orders_at_best_ask.empty()) {
                            asks_.erase(best_ask_level_iter);
                        }
                    }
                    if (order_to_process.remaining_quantity == 0) break;
                }
            } else { 
                while (order_to_process.remaining_quantity > 0 && !bids_.empty()) {
                    auto best_bid_level_iter = bids_.begin(); 
                    std::list<Order>& orders_at_best_bid = best_bid_level_iter->second;

                     if (orders_at_best_bid.empty()){ 
                        bids_.erase(best_bid_level_iter);
                        continue;
                    }
                    Order& resting_buy_order = orders_at_best_bid.front(); 

                    double match_price = resting_buy_order.price;
                    unsigned long long match_qty = std::min(order_to_process.remaining_quantity, resting_buy_order.remaining_quantity);
                    
                    recordMatchAndCreateOutput(order_to_process, resting_buy_order, match_qty, match_price, current_event_timestamp);

                    if (resting_buy_order.remaining_quantity == 0) {
                        orders_at_best_bid.pop_front();
                        if (orders_at_best_bid.empty()) {
                            bids_.erase(best_bid_level_iter);
                        }
                    }
                    if (order_to_process.remaining_quantity == 0) break;
                }
            }
            
            if (order_to_process.cumulative_executed_quantity == 0 && initial_market_order_qty > 0) { 
                 addInitialOutputRecord(order_to_process, OrderStatus::REJECTED, 0, 0.0, 0, current_event_timestamp); 
            }
        }
    } else if (incoming_order_request.action == OrderAction::MODIFY) {
        bool found_original_order = false;
        Order existing_order_copy; 
        std::vector<double> price_levels_to_erase_bids;
        std::vector<double> price_levels_to_erase_asks;

        auto find_and_remove_from_list = 
            [&](std::list<Order>& order_list, long long target_order_id, Order& copy_target) -> bool {
            for (auto it = order_list.begin(); it != order_list.end(); ++it) {
                if (it->order_id == target_order_id) {
                    copy_target = *it; 
                    order_list.erase(it);
                    return true;
                }
            }
            return false;
        };

        for (auto map_iter = bids_.begin(); map_iter != bids_.end(); ++map_iter) {
            if (find_and_remove_from_list(map_iter->second, incoming_order_request.order_id, existing_order_copy)) {
                if (map_iter->second.empty()) {
                    price_levels_to_erase_bids.push_back(map_iter->first);
                }
                found_original_order = true;
                break;
            }
        }
        for(double p : price_levels_to_erase_bids) { bids_.erase(p); }

        if (!found_original_order) { 
            for (auto map_iter = asks_.begin(); map_iter != asks_.end(); ++map_iter) {
                if (find_and_remove_from_list(map_iter->second, incoming_order_request.order_id, existing_order_copy)) {
                    if (map_iter->second.empty()) {
                       price_levels_to_erase_asks.push_back(map_iter->first);
                    }
                    found_original_order = true;
                    break;
                }
            }
            for(double p : price_levels_to_erase_asks) { asks_.erase(p); }
        }

        if (!found_original_order) {
            addInitialOutputRecord(incoming_order_request, OrderStatus::REJECTED, 0, 0.0, 0, current_event_timestamp);
            return;
        }

        Order modified_order = existing_order_copy; 
        modified_order.timestamp = current_event_timestamp; // Use MODIFY event's timestamp
        modified_order.price = incoming_order_request.price;         
        modified_order.quantity = incoming_order_request.quantity;   
        modified_order.action = OrderAction::MODIFY;                 
        modified_order.type = incoming_order_request.type;           

        if (modified_order.quantity <= modified_order.cumulative_executed_quantity) {
            modified_order.remaining_quantity = 0; 
            modified_order.status = OrderStatus::EXECUTED; 
            if (modified_order.cumulative_executed_quantity == 0 && modified_order.quantity == 0) {
                modified_order.status = OrderStatus::CANCELED;
            }
            addInitialOutputRecord(modified_order, modified_order.status, 0, 0.0, 0, current_event_timestamp);
        } else {
            modified_order.remaining_quantity = modified_order.quantity - modified_order.cumulative_executed_quantity;
            modified_order.status = OrderStatus::PENDING; 

            Order order_to_readd = modified_order; 

            if (order_to_readd.type == OrderType::LIMIT) {
                 if (order_to_readd.side == Side::BUY) {
                    bids_[order_to_readd.price].push_back(order_to_readd);
                } else { 
                    asks_[order_to_readd.price].push_back(order_to_readd);
                }
                
                bool traded_in_match = false;
                long long temp_id = order_to_readd.order_id; 
                
                matchOrders(current_event_timestamp);

                if (ids_traded_this_event_.count(temp_id)) {
                    traded_in_match = true;
                }

                Order* final_resting_order_ptr = nullptr;
                if (order_to_readd.side == Side::BUY && bids_.count(order_to_readd.price)) {
                    for (Order& o : bids_.at(order_to_readd.price)) { if (o.order_id == temp_id) { final_resting_order_ptr = &o; break; } }
                } else if (order_to_readd.side == Side::SELL && asks_.count(order_to_readd.price)) {
                    for (Order& o : asks_.at(order_to_readd.price)) { if (o.order_id == temp_id) { final_resting_order_ptr = &o; break; } }
                }

                if (final_resting_order_ptr && !traded_in_match) { // If it rests and wasn't part of the immediate match
                     addInitialOutputRecord(*final_resting_order_ptr, final_resting_order_ptr->status, 0, 0.0, 0, current_event_timestamp);
                }
            } else if (order_to_readd.type == OrderType::MARKET) {
                unsigned long long initial_mod_market_qty = order_to_readd.remaining_quantity;
                unsigned long long cum_exec_before_market_sweep = order_to_readd.cumulative_executed_quantity;

                if (order_to_readd.side == Side::BUY) {
                    while (order_to_readd.remaining_quantity > 0 && !asks_.empty()) { 
                        auto iter = asks_.begin(); 
                        if (iter->second.empty()) { asks_.erase(iter); continue; }
                        Order& resting = iter->second.front();
                        double m_price = resting.price; unsigned long long m_qty = std::min(order_to_readd.remaining_quantity, resting.remaining_quantity);
                        recordMatchAndCreateOutput(order_to_readd, resting, m_qty, m_price, current_event_timestamp);
                        if(resting.remaining_quantity == 0) { iter->second.pop_front(); if(iter->second.empty()) asks_.erase(iter); }
                        if(order_to_readd.remaining_quantity == 0) break;
                    }
                } else { 
                    while (order_to_readd.remaining_quantity > 0 && !bids_.empty()) { 
                        auto iter = bids_.begin(); 
                        if (iter->second.empty()) { bids_.erase(iter); continue; }
                        Order& resting = iter->second.front();
                        double m_price = resting.price; unsigned long long m_qty = std::min(order_to_readd.remaining_quantity, resting.remaining_quantity);
                        recordMatchAndCreateOutput(order_to_readd, resting, m_qty, m_price, current_event_timestamp);
                        if(resting.remaining_quantity == 0) { iter->second.pop_front(); if(iter->second.empty()) bids_.erase(iter); }
                        if(order_to_readd.remaining_quantity == 0) break;
                    }
                }
                if (order_to_readd.cumulative_executed_quantity == cum_exec_before_market_sweep && initial_mod_market_qty > 0) { 
                     addInitialOutputRecord(order_to_readd, OrderStatus::REJECTED, 0, 0.0, 0, current_event_timestamp); 
                }
            }
        }

    } else if (incoming_order_request.action == OrderAction::CANCEL) {
        bool found_and_cancelled = false;
        Order order_being_cancelled_data; 

        std::vector<double> price_levels_to_erase_bids_cancel;
        std::vector<double> price_levels_to_erase_asks_cancel;
        
        auto find_and_remove_for_cancel = 
            [&](std::list<Order>& order_list, long long target_order_id, Order& copy_target) -> bool {
            for (auto it = order_list.begin(); it != order_list.end(); ++it) {
                if (it->order_id == target_order_id) {
                    copy_target = *it; 
                    order_list.erase(it);
                    return true;
                }
            }
            return false;
        };

        for (auto map_iter = bids_.begin(); map_iter != bids_.end(); ++map_iter) {
            if (find_and_remove_for_cancel(map_iter->second, incoming_order_request.order_id, order_being_cancelled_data)) {
                if (map_iter->second.empty()) {
                    price_levels_to_erase_bids_cancel.push_back(map_iter->first);
                }
                found_and_cancelled = true;
                break;
            }
        }
        for(double p : price_levels_to_erase_bids_cancel) { bids_.erase(p); }

        if (!found_and_cancelled) {
            for (auto map_iter = asks_.begin(); map_iter != asks_.end(); ++map_iter) {
                 if (find_and_remove_for_cancel(map_iter->second, incoming_order_request.order_id, order_being_cancelled_data)) {
                    if (map_iter->second.empty()) {
                        price_levels_to_erase_asks_cancel.push_back(map_iter->first);
                    }
                    found_and_cancelled = true;
                    break;
                }
            }
            for(double p : price_levels_to_erase_asks_cancel) { asks_.erase(p); }
        }

        if (found_and_cancelled) {
            order_being_cancelled_data.timestamp = current_event_timestamp; 
            order_being_cancelled_data.action = OrderAction::CANCEL; 
            addInitialOutputRecord(order_being_cancelled_data, OrderStatus::CANCELED, 0, 0.0, 0, current_event_timestamp);
        } else {
            //std::cerr << "Warning: Order CANCEL for order_id " << incoming_order_request.order_id << " not found." << std::endl;
            addInitialOutputRecord(incoming_order_request, OrderStatus::REJECTED, 0, 0.0, 0, current_event_timestamp);
        }

    } else { 
        std::cerr << "Warning: Unknown order action for order_id " << incoming_order_request.order_id << std::endl;
        addInitialOutputRecord(incoming_order_request, OrderStatus::REJECTED, 0, 0.0, 0, current_event_timestamp);
    }
}

void OrderBook::matchOrders(unsigned long long event_timestamp) {
    while (!bids_.empty() && !asks_.empty()) {
        auto best_bid_level_map_iter = bids_.begin(); 
        auto best_ask_level_map_iter = asks_.begin(); 

        double best_bid_price = best_bid_level_map_iter->first;
        double best_ask_price = best_ask_level_map_iter->first;

        if (best_bid_price < best_ask_price) { 
            break; 
        }

        std::list<Order>& orders_at_best_bid = best_bid_level_map_iter->second;
        std::list<Order>& orders_at_best_ask = best_ask_level_map_iter->second;
        
        if (orders_at_best_bid.empty()){ bids_.erase(best_bid_level_map_iter); continue; }
        if (orders_at_best_ask.empty()){ asks_.erase(best_ask_level_map_iter); continue; }

        Order& buy_order = orders_at_best_bid.front();  
        Order& sell_order = orders_at_best_ask.front(); 

        double match_price;
        if (buy_order.timestamp < sell_order.timestamp) { 
            match_price = buy_order.price; 
        } else if (sell_order.timestamp < buy_order.timestamp) { 
            match_price = sell_order.price; 
        } else { 
            match_price = best_bid_price; 
        }
        
        unsigned long long match_qty = std::min(buy_order.remaining_quantity, sell_order.remaining_quantity);

        recordMatchAndCreateOutput(buy_order, sell_order, match_qty, match_price, event_timestamp);

        if (buy_order.remaining_quantity == 0) {
            orders_at_best_bid.pop_front();
        }
        if (sell_order.remaining_quantity == 0) {
            orders_at_best_ask.pop_front();
        }

        if (orders_at_best_bid.empty()) {
            bids_.erase(best_bid_level_map_iter); 
        }
        if (orders_at_best_ask.empty()) {
            asks_.erase(best_ask_level_map_iter); 
        }
    }
}

const std::string& OrderBook::getInstrumentName() const {
    return instrument_name_;
}

void OrderBook::printOrderBookSnapshot() const {
    std::cout << "---- Order Book Snapshot for: " << instrument_name_ << " ----" << std::endl;
    
    std::cout << "ASKS (Price: RemainingQty@OrderID Action Status):" << std::endl;
    if (asks_.empty()) {
        std::cout << "  <empty>" << std::endl;
    } else {
        for (const auto& [price, order_list] : asks_) {
            std::cout << "  Price " << price << ": ";
            for (const auto& order : order_list) {
                std::cout << order.remaining_quantity << "@" << order.order_id 
                          << "(" << orderActionToString(order.action) << "," << orderStatusToString(order.status) << ") ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "BIDS (Price: RemainingQty@OrderID Action Status):" << std::endl;
    if (bids_.empty()) {
        std::cout << "  <empty>" << std::endl;
    } else {
        for (const auto& [price, order_list] : bids_) {
            std::cout << "  Price " << price << ": ";
            for (const auto& order : order_list) {
                 std::cout << order.remaining_quantity << "@" << order.order_id 
                          << "(" << orderActionToString(order.action) << "," << orderStatusToString(order.status) << ") ";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "----------------------------------------" << std::endl;
}
