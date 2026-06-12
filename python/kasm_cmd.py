import socket
import argparse
import sys
import time
import struct

# --- Constants ---
KASM_PORT = 2345
NUM_COMMAND_WORDS = 26
INT16_LIMIT = (2**15) - 1  # 32767

def parse_arguments():
    """Configures and parses command-line arguments for the KASM client."""
    parser = argparse.ArgumentParser(
        description="Command utility for the KASM actuator service."
    )
    
    parser.add_argument(
        '-i','--ip',
        type=str,
        default='127.0.0.1',
        help="Target IP address of the KASM service. Default is 127.0.0.1."
    )
    
    parser.add_argument(
        '-s','--start',
        type=int,
        required=True,
        help="Starting position command value (-32767 to 32767)."
    )
    
    parser.add_argument(
        '-e','--end',
        type=int,
        required=True,
        help="Ending position command value (-32767 to 32767)."
    )
    
    parser.add_argument(
        '-n','--steps',
        type=int,
        required=True,
        help="Number of steps to take to transition from start to end."
    )
    
    parser.add_argument(
        '-t','--time',
        type=float,
        default=100.0,
        help="Time delay between steps in milliseconds. Default is 100ms."
    )
    
    return parser.parse_args()

def main():
    args = parse_arguments()
    
    # Verify 16-bit signed integer overflow boundaries
    if abs(args.start) > INT16_LIMIT:
        print(f"Error: Starting position {args.start} overflows 16-bit limits (+/- {INT16_LIMIT}).", file=sys.stderr)
        sys.exit(1)
        
    if abs(args.end) > INT16_LIMIT:
        print(f"Error: Ending position {args.end} overflows 16-bit limits (+/- {INT16_LIMIT}).", file=sys.stderr)
        sys.exit(1)
        
    if args.steps <= 0:
        print("Error: Number of steps must be a positive integer greater than 0.", file=sys.stderr)
        sys.exit(1)

    # Compute distance per step
    step_size = (args.end - args.start) / args.steps
    delay_sec = args.time / 1000.0
    
    print("Initializing KASM Actuator Commander...")
    print(f"Target: {args.ip}:{KASM_PORT}")
    print(f"Profile: {args.start} -> {args.end} over {args.steps} steps ({args.time}ms per step)")

    # 3. Create UDP Socket
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        next_time = time.time()
        
        # Iterate from 0 to n_steps (inclusive to make sure we hit the final 'end' position)
        for step in range(args.steps + 1):
            # Calculate the current interpolated position
            current_pos = int(round(args.start + (step * step_size)))
            
            # Populate array of 26 command values with the current position
            command_array = [current_pos] * NUM_COMMAND_WORDS
            
            # 5. Pack array into 26 16-bit signed short integers using Big-Endian format ('!26h')
            # 26 words * 2 bytes/word = 52 bytes total payload size
            payload = struct.pack("!26h", *command_array)
            
            # Send the payload
            udp_sock.sendto(payload, (args.ip, KASM_PORT))
            
            # 6. High-precision timing tracking between steps
            if step < args.steps:  # Skip sleeping after the final step execution
                next_time += delay_sec
                sleep_time = next_time - time.time()
                if sleep_time > 0:
                    time.sleep(sleep_time)
                else:
                    # Reset baseline clock if the OS or network environment experiences a hiccup
                    next_time = time.time()
                    
        print("\nTrajectory execution complete. All steps sent successfully.")

    except KeyboardInterrupt:
        print("\nTrajectory interrupted by user via KeyboardInterrupt.")
    finally:
        # Close the socket gracefully
        udp_sock.close()
        print("UDP socket closed cleanly. Exiting.")

if __name__ == "__main__":
    main()