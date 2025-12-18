#!/usr/bin/env python3
import subprocess
import json
import time
import sys
import argparse

# ================= CONFIGURATION =================
# How many splits to create per transaction? 
# (e.g., 1 big input -> 5 new staking outputs)
SPLITS_PER_ROUND = 5 
# =================================================

def run_rpc(cli_cmd, method, *args):
    """Executes a kore-cli command and returns the JSON result."""
    command = cli_cmd + [method] + [str(a) for a in args]
    try:
        result = subprocess.check_output(command)
        if not result:
            return None
        # Try to parse JSON, if it's just a string (like a txid), wrap it
        try:
            return json.loads(result)
        except json.JSONDecodeError:
            return result.decode('utf-8').strip()
    except subprocess.CalledProcessError as e:
        print(f"Error running RPC: {e}")
        sys.exit(1)

def get_block_height(cli_cmd):
    return int(run_rpc(cli_cmd, "getblockcount"))

def main():
    parser = argparse.ArgumentParser(description="Smart Staking Fan-Out Script")
    parser.add_argument("amount", type=float, help="Amount of coins to send to EACH new address")
    parser.add_argument("rounds", type=int, help="How many rounds of splitting to perform")
    parser.add_argument("--cli", default="./src/kore-cli", help="Path to kore-cli (default: ./src/kore-cli)")
    parser.add_argument("--datadir", help="Path to datadir (e.g. /home/mx/.kore/testnet3)")
    parser.add_argument("--testnet", action="store_true", help="Use testnet defaults")
    
    args = parser.parse_args()

    # Build the base CLI command
    cli_cmd = [args.cli]
    if args.datadir:
        cli_cmd.append(f"-datadir={args.datadir}")
    if args.testnet:
        # Assuming standard testnet path if only flag is given
        print("Assuming default testnet datadir...")
        cli_cmd.append("-testnet")

    print(f"--- Smart Stake Spreader Started ---")
    print(f"Target: {args.amount} KORE per address")
    print(f"Splits per tx: {SPLITS_PER_ROUND}")
    print(f"Total Rounds: {args.rounds}")
    
    current_height = get_block_height(cli_cmd)
    print(f"Current Block Height: {current_height}")

    for round_num in range(1, args.rounds + 1):
        print(f"\n[Round {round_num}/{args.rounds}] Analyzing wallet UTXOs...")

        # 1. Get all unspent outputs
        unspent = run_rpc(cli_cmd, "listunspent")
        
        # 2. Sort by amount (Descending) - Coin Control Logic
        # We want the biggest pile of coins to split from.
        unspent.sort(key=lambda x: x['amount'], reverse=True)

        if not unspent:
            print("Error: No unspent coins found!")
            break

        # Pick the winner
        source_utxo = unspent[0]
        source_amount = float(source_utxo['amount'])
        source_addr = source_utxo['address']
        
        required_amount = args.amount * SPLITS_PER_ROUND
        
        print(f"  > Largest UTXO found: {source_amount:.4f} KORE (Address: {source_addr})")

        if source_amount < required_amount + 0.01: # Buffer for fees
            print(f"  > Error: Not enough funds in largest UTXO to perform split.")
            print(f"  > Needed: {required_amount} | Have: {source_amount}")
            print("  > Stopping.")
            break

        # 3. Generate new addresses and build 'sendmany' dictionary
        send_many_dict = {}
        print(f"  > Generating {SPLITS_PER_ROUND} new addresses...")
        
        for _ in range(SPLITS_PER_ROUND):
            # We create a new address for every split to maximize privacy and staking separation
            new_addr = run_rpc(cli_cmd, "getnewaddress")
            send_many_dict[new_addr] = args.amount
        
        # 4. Send the Transaction
        # We use the empty string "" to denote the default account
        print(f"  > Sending {required_amount} KORE (split into {SPLITS_PER_ROUND})...")
        try:
            txid = run_rpc(cli_cmd, "sendmany", "", json.dumps(send_many_dict))
            print(f"  > TX Sent! ID: {txid}")
        except Exception as e:
            print(f"  > Transaction Failed: {e}")
            break

        # 5. Wait for a Block
        if round_num < args.rounds:
            print("  > Waiting for next block to ensure inputs valid...")
            while True:
                new_height = get_block_height(cli_cmd)
                if new_height > current_height:
                    current_height = new_height
                    print(f"  > Block {new_height} mined! Proceeding to next round.")
                    break
                time.sleep(5) # Check every 5 seconds
        else:
            print("\nDone! All rounds completed.")

if __name__ == "__main__":
    main()
