import csv
import random
import argparse
import time
import sys # Import the sys module to access stdout

# Define possible values for order fields
# Generate 200 unique instrument symbols
INSTRUMENTS = [f"INST{i:03}" for i in range(1, 201)] 

SIDES = ["BUY", "SELL"]
TYPES = ["LIMIT", "MARKET"]
ACTIONS = ["NEW", "MODIFY", "CANCEL"]

def generate_order_data(num_lines, output_target):
    """
    Generates synthetic order data and writes it to the output_target.
    """
    writer = csv.writer(output_target) 
    # Write header
    writer.writerow(["timestamp", "order_id", "instrument", "side", "type", "quantity", "price", "action"])

    # Initial values
    # Using nanoseconds for timestamp
    current_timestamp = int(time.time() * 1_000_000_000) 
    order_id_counter = 1
    
    # Keep track of active orders to allow for plausible MODIFY/CANCEL actions
    active_orders = [] 

    for i in range(num_lines):
        current_timestamp += random.randint(100, 10000) # Increment timestamp

        action = ""
        # Determine action:
        # If no active orders, action must be NEW.
        # Otherwise, choose among NEW, MODIFY, CANCEL.
        # we define probabilities for each type of orders to have a realistic book
        if not active_orders:
            action = "NEW"
        else:
            action_choice_rand = random.random()
            if action_choice_rand < 0.70:  # 70% chance of NEW
                action = "NEW"
            elif action_choice_rand < 0.85:  # 15% chance of MODIFY
                action = "MODIFY"
            else:  # 15% chance of CANCEL
                action = "CANCEL"
        

        order_id_to_use = 0 # Will be set based on action
        instrument = random.choice(INSTRUMENTS)
        side = random.choice(SIDES)
        order_type = random.choice(TYPES)
        quantity = random.randint(1, 200) * 5 
        
        base_price = random.uniform(50.0, 500.0)
        price_tick = 0.01
        price = round(base_price / price_tick) * price_tick 

        if action == "NEW":
            order_id_to_use = order_id_counter
            order_id_counter += 1
            
            if order_type == "MARKET":
                price = 0.0 
            
            active_orders.append({
                "id": order_id_to_use, 
                "instrument": instrument, 
                "quantity": quantity, # original quantity
                "price": price,
                "side": side,
                "type": order_type
            })

        elif action == "MODIFY":
            # This block is only reached if active_orders is not empty.
            order_to_modify = random.choice(active_orders) # Select an existing active order
            
            order_id_to_use = order_to_modify["id"]
            instrument = order_to_modify["instrument"] # Instrument typically doesn't change on modify
            side = order_to_modify["side"]           # Side typically doesn't change
            
            # New quantity and price for the modify action
            quantity = random.randint(1, 200) * 5 
            if order_to_modify["price"] != 0.0:
                new_base_price = random.uniform(order_to_modify["price"] * 0.95, order_to_modify["price"] * 1.05)
            else: 
                new_base_price = random.uniform(50.0, 500.0) 
            price = round(new_base_price / price_tick) * price_tick
            
            order_type = "LIMIT" # Modify actions in example are LIMIT
            if order_to_modify["type"] == "MARKET" and price == 0.0: 
                price = round(random.uniform(50.0, 500.0) / price_tick) * price_tick
            # Note: The 'active_orders' list is not updated here for the modification.
            # It still holds the original details of the order. The matching engine handles the state.

        elif action == "CANCEL":
            # This block is only reached if active_orders is not empty.
            order_to_cancel_idx = random.randrange(len(active_orders))
            order_to_cancel = active_orders.pop(order_to_cancel_idx) # Remove from active_orders
            
            order_id_to_use = order_to_cancel["id"]
            instrument = order_to_cancel["instrument"]
            side = order_to_cancel["side"]
            order_type = order_to_cancel["type"] # Use original type for the cancel line
            
            # For CANCEL, the input CSV example shows original quantity and price 0
            quantity = order_to_cancel["quantity"] # Use original quantity for the CANCEL line
            price = 0.0 # Price is 0 for CANCEL action in example
        
        # Format price string
        price_str = f"{price:.2f}" if price != 0.0 else "0" 
        if price == 0.0 and order_type == "LIMIT" and action != "CANCEL": 
             price_str = "0.00" 
        elif price == 0.0 and action == "CANCEL": # Ensure CANCEL with price 0 is just "0"
            price_str = "0" 

        writer.writerow([current_timestamp, order_id_to_use, instrument, side,
            order_type, quantity, price_str, action])



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate CSV order data")
    parser.add_argument(
        "num_lines", 
        type=int, 
        help="Number of data lines to generate (excluding header)."
    )
    parser.add_argument(
        "-o", "--output_file",
        type=str,
        help="Optional: Path to the output CSV file. If not provided, prints to stdout."
    )

    args = parser.parse_args()

    if args.output_file:
        try:
            with open(args.output_file, 'w', newline='') as outfile:
                generate_order_data(args.num_lines, outfile) # Pass file object
            print(f"Successfully generated {args.num_lines} lines to {args.output_file}")
        except IOError as e:
            print(f"Error writing to file {args.output_file}: {e}")
            print("Falling back to generating data on stdout:")
            generate_order_data(args.num_lines, sys.stdout) # Fallback to stdout
    else:
        generate_order_data(args.num_lines, sys.stdout) # Pass sys.stdout
