#!/usr/bin/env python3
import subprocess
import json
import time
import sys
import argparse

# ================= CONFIGURATION =================
SPLITS_PER_ROUND = 5 
MATURITY_REQUIRED = 5  
MIN_REQUIRED_COINS = 5002.0
# =================================================

def run_rpc(cli_cmd, method, *args):
    """Executes a kore-cli command and returns the JSON result."""
    command = cli_cmd + [method] + [str(a) for a in args]
    try:
        # stderr=subprocess.STDOUT helps ignore non-critical RPC warnings
        result = subprocess.check_output(command, stderr=subprocess.STDOUT)
        if not result: return None
        try:
            return json.loads(result)
        except json.JSONDecodeError:
            return result.decode('utf-8').strip()
    except subprocess.CalledProcessError:
        return None

def ensure_staking_active(cli_cmd):
    """Force staking to be ON without interrupting flow."""
    try:
        run_rpc(cli_cmd, "setstaking", "true")
    except:
        pass

def main():
    parser = argparse.ArgumentParser(description="KORE Zero-Downtime Spreader")
    parser.add_argument("amount", type=float, help="KORE per split")
    parser.add_argument("rounds", type=int, help="Total rounds")
    parser.add_argument("--cli", default="./src/kore-cli", help="Path to kore-cli")
    parser.add_argument("--datadir", help="Path to datadir")
    parser.add_argument("--testnet", action="store_true", help="Use testnet")
    
    args = parser.parse_args()
    cli_cmd = [args.cli]
    if args.datadir: cli_cmd.append(f"-datadir={args.datadir}")
    if args.testnet: cli_cmd.append("-testnet")

    print(f"--- Smart Stake Spreader (Always-On Staking Mode) ---")
    
    # Ensure staking is ON at start
    ensure_staking_active(cli_cmd)
    
    round_idx = 1
    while round_idx <= args.rounds:
        # 1. UNLOCK ALL
        # We start by ensuring everything is free to stake
        run_rpc(cli_cmd, "lockunspent", "true")

        # 2. SCAN WALLET
        unspent = run_rpc(cli_cmd, "listunspent")
        if not unspent:
            print("  > Wallet scanning... Waiting 15s.")
            time.sleep(15)
            continue

        # 3. FIND TARGET
        candidates = [u for u in unspent if u['confirmations'] >= MATURITY_REQUIRED and u['amount'] >= MIN_REQUIRED_COINS]
        candidates.sort(key=lambda x: x['amount'], reverse=True)

        if not candidates:
            print(f"  > Waiting for mature UTXO >= {MIN_REQUIRED_COINS}...")
            # Heartbeat to ensure staking stays ON during long waits
            ensure_staking_active(cli_cmd)
            time.sleep(30)
            continue

        target = candidates[0]
        print(f"\n[Round {round_idx}/{args.rounds}] Target: {target['amount']} KORE")

        try:
            # 4. EXCLUDE TARGET FROM STAKING (The "Let Go" Step)
            # We lock the target coin. The Staker will see it's locked and ignore it.
            # ALL OTHER coins continue to stake normally.
            print("  > Isolating target from Staker (others keep staking)...")
            lock_target = [{"txid": target['txid'], "vout": target['vout']}]
            run_rpc(cli_cmd, "lockunspent", "false", json.dumps(lock_target))
            
            # Wait 5 seconds for the Staker to refresh and "forget" this specific coin
            time.sleep(5)

            # 5. PREPARE BROADCAST (Flash Swap)
            # We need to unlock the target (to spend it) and lock others (to force input selection)
            # This happens in milliseconds, so staking impact is negligible.
            
            # A. Unlock everything first
            run_rpc(cli_cmd, "lockunspent", "true") 
            
            # B. Lock everything EXCEPT target
            # This forces sendmany to use ONLY the target, preventing vin.size=6 crash
            others = [u for u in unspent if u['txid'] != target['txid'] or u['vout'] != target['vout']]
            if others:
                lock_others = [{"txid": u['txid'], "vout": u['vout']} for u in others]
                run_rpc(cli_cmd, "lockunspent", "false", json.dumps(lock_others))

            # 6. BROADCAST
            print("  > Broadcasting...")
            recipients = {run_rpc(cli_cmd, "getnewaddress"): args.amount for _ in range(SPLITS_PER_ROUND)}
            txid = run_rpc(cli_cmd, "sendmany", "", json.dumps(recipients), "1", "FanOut")
            
            # 7. IMMEDIATE RELEASE
            # Unlock everything instantly. Staking is fully restored for all coins.
            run_rpc(cli_cmd, "lockunspent", "true")
            ensure_staking_active(cli_cmd) # Double check staking is ON

            if txid:
                print(f"  > Success! TXID: {txid}")
                
                # Lock the spent input to prevent double-spending it
                run_rpc(cli_cmd, "lockunspent", "false", json.dumps(lock_target))
                
                round_idx += 1
                
                print("  > Cooling down (30s). Staking is ACTIVE.")
                h = run_rpc(cli_cmd, "getblockcount")
                while True:
                    time.sleep(20)
                    if (run_rpc(cli_cmd, "getblockcount") or 0) > h: 
                        print("  > Block confirmed.")
                        break
            else:
                print("  > Broadcast failed. Retrying...")
                time.sleep(10)

        except Exception as e:
            print(f"  > Error: {e}")
            # Safety: Unlock everything and ensure staking is ON
            run_rpc(cli_cmd, "lockunspent", "true")
            ensure_staking_active(cli_cmd)
            time.sleep(20)

    print("\nProcess Complete.")

if __name__ == "__main__":
    main()
