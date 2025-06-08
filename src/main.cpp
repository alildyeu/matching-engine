#include "app_config.hpp" 
#include "logger.hpp"
#include "order.hpp"      // For Order struct, readOrdersFromStream, enums, and toString helpers
#include "orderbook.hpp" // For OrderBook class and OutputRecord struct
#include "thread_safe_queue.hpp" 
#include <thread>
#include <iostream>       // For std::cerr, std::cout
#include <fstream>        // For std::ifstream and std::ofstream
#include <vector>
#include <map>
#include <string>
#include <algorithm>      // For std::stable_sort
#include "main.hpp"
#include <atomic>



int main(int argc, char* argv[]) {
    AppConfig config("A matching engine for the stock market");
    // Initialize logger.
    Logger logger("MatchingEngineApp"); 
   
    if (!config.parse(argc, argv)) {
        logger.error("Exiting due to argument parsing error or help request.");
        return 1; // Indicate failure
    }

    // Configure log level (the level of infos from error we want to see)
    LogLevel log_level = string_to_level(config.get_log_level());
    logger.set_level(log_level); 


    logger.info("Configuration loaded successfully:");
    logger.info("  Log Level:       ", config.get_log_level());
    logger.info("  Input File:      ", config.get_order_input_file());
    logger.info("  Output File:     ", config.get_order_result_output_file());
    logger.info("  Queue Size:      ", config.get_queue_size());


    //Read all orders from the input CSV file
    std::vector<Order> all_input_orders;
    // open the input file stream
    std::ifstream input_file_stream(config.get_order_input_file());

    if (!input_file_stream.is_open()) {
        logger.critical("Failed to open input order file: ", config.get_order_input_file());
        return 1; // Critical error, stop the program
    }

    //To have a thread safe queue wich means that multiple threads can work together
    // without stealing each other's tasks
    ThreadSafeQueue<Order> order_queue;

    // readOrdersFromStream is defined in order.cpp and declared in order.hpp


    std::atomic<bool> is_done_reading(false); // To signal when reading is done

    std::thread read_fromfile_thread(
        [&]() {
                auto timer = TimingManager::ScopedTimer(
                "Time reading from CSV", 
                logger
            );
    
           readOrdersFromStream(input_file_stream, logger, order_queue, 
                             100000); // Pass the queue size from config
            input_file_stream.close();
            is_done_reading= true; // Signal that reading is done
        }
    );

    

    // Order Book Management and Processing Loop
    //  Map instrument name to its OrderBook instance
    std::map<std::string, OrderBook> order_books; 
    std::shared_ptr<ThreadSafeQueue<std::string>> output_log_queue; // log output records in a thread-safe manner

    output_log_queue = std::make_shared<ThreadSafeQueue<std::string>>(); // Initialize the output log queue
    
    std::atomic<bool> is_done_launching_jobs(false); // To signal when processing is done
    std::thread launch_work_thread([&]() {
    auto timer = TimingManager::ScopedTimer(
                "Time Starting order processing threads for all instruments", 
                logger
            );
    
    while (!is_done_reading || order_queue.size() != 0) {
        // Process orders from the queue
        if (order_queue.size() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Avoid busy-waiting
            continue;
        }
      Order order_request = order_queue.pop(); // Pop from the thread-safe queue
    // Pass by reference as OrderBook::addOrder takes Order&
        logger.debug("Processing incoming order request: ID=", order_request.order_id, 
                    ", Instrument=", order_request.instrument,
                    ", Action=", orderActionToString(order_request.action),
                    ", Type=", orderTypeToString(order_request.type), 
                    ", Side=", sideToString(order_request.side),       
                    ", Qty=", order_request.quantity, 
                    ", Price=", order_request.price);
        
        // Find or create the OrderBook for the order's instrument
        if (order_books.find(order_request.instrument) == order_books.end()) {
            logger.debug("Creating new order book for instrument: ", order_request.instrument);
            // Emplace a new OrderBook for this instrument
            order_books.emplace(std::piecewise_construct,
                                std::forward_as_tuple(order_request.instrument),
                                std::forward_as_tuple(order_request.instrument));
            order_books.at(order_request.instrument).set_output_log_queue(output_log_queue);
            order_books.at(order_request.instrument).startProcessingThread(); // Start processing thread for this book
        }
        
        OrderBook& current_book = order_books.at(order_request.instrument);
        current_book.addOrder(order_request); // This method processes the order and generates OutputRecords
    }

   is_done_launching_jobs = true; // Signal that all orders have been launched for processing
            
    });

    std::atomic<bool> is_done_processing_orders(false); // To signal when processing is done
    std::thread read_orders_thread([&]() {
        {
            while(!is_done_launching_jobs){
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Avoid waiting
            }
            auto timer = TimingManager::ScopedTimer(
                        "Time Stop processing threads for all instruments", 
                        logger
                    );
            for (auto & [instrument_name, book] : order_books){
                book.stopProcessingThread(); // Stop the processing thread for each order book
            }
            is_done_processing_orders = true; // Signal that processing is done
        }
    });

    logger.info("All input orders have been processed by their respective order books.");

    // Write the OutputRecord to the output CSV file
    std::ofstream output_file_stream(config.get_order_result_output_file());
    if (!output_file_stream.is_open()) {
        logger.critical("Failed to open output order result file: ", config.get_order_result_output_file());
        return 1; // error
    }

    // Write the CSV header
    output_file_stream << "timestamp,order_id,instrument,side,type,quantity,price,action,status,executed_quantity,execution_price,counterparty_id\n";


    {
    auto timer = TimingManager::ScopedTimer(
        "Time writing results to output file", 
        logger
    );
    
        // Write each record in the output log queue to the output file
    while (!output_log_queue->empty()  || !is_done_processing_orders) {
        std::string record;
        if (output_log_queue->try_pop(record)) {
        output_file_stream << record << "\n";
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(10)); // Avoid waiting
        }
    }}
    output_file_stream.close();
    logger.info("Output records successfully written to: ", config.get_order_result_output_file());

    if (read_fromfile_thread.joinable()) {
        read_fromfile_thread.join(); // Wait for the reading thread to finish
    }
  
     if (launch_work_thread.joinable()) {
        launch_work_thread.join(); // Wait for the reading thread to finish
    }
    if (read_orders_thread.joinable()) {
        read_orders_thread.join(); // Wait for the reading thread to finish
    }
    logger.info("Matching engine run completed successfully.");
    return 0; // success
}
