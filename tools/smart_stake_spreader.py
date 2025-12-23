#!/usr/bin/env python3
import time
import subprocess
import sys

# Configuration
COIN_AMOUNT = 3000  # Amount to split into
WAIT_SECONDS = 3    # Time to wait between transactions

def run_command(cmd):
    try:
        result = subprocess.check_output(cmd, shell=True)
        return result.decode('utf-8').strip()
    except subprocess.CalledProcessError:
        return None

def get_balance():
    output = run_command("./src/kore-cli -testnet getbalance")
    return float(output) if output else 0.0

def spread_stakes():
    print("=== SMART STAKE SPREADER (SAFE MODE) ===")
    while True:
        balance = get_balance()
        if balance < COIN_AMOUNT:
            print(f"Balance {balance} is too low. Stopping.")
            break

        print(f"Balance: {balance} | Splitting...")
        
        addr = run_command("./src/kore-cli -testnet getnewaddress")
        if not addr:
            time.sleep(1)
            continue

        txid = run_command(f"./src/kore-cli -testnet sendtoaddress {addr} {COIN_AMOUNT}")
        
        if txid:
            print(f"Sent! TxID: {txid}")
            print(f"Waiting {WAIT_SECONDS}s for safety...")
            time.sleep(WAIT_SECONDS)
        else:
            print("Send failed, retrying...")
            time.sleep(10)

if __name__ == "__main__":
    spread_stakes()
