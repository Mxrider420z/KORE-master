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

    print(f"--- Smart Stake Spreader (Safe-Swap Mode) ---")
    
    ensure_staking_active(cli_cmd)
    
    round_idx = 1
    while round_idx <= args.rounds:
        # 1. CLEAN START: Unlock all to scan correctly
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
            ensure_staking_active(cli_cmd)
            time.sleep(30)
            continue

        target = candidates[0]
        print(f"\n[Round {round_idx}/{args.rounds}] Target: {target['amount']} KORE")

        try:
            # 4. EXCLUDE TARGET FROM STAKING
            print("  > Isolating target from Staker...")
            lock_target = [{"txid": target['txid'], "vout": target['vout']}]
            run_rpc(cli_cmd, "lockunspent", "false", json.dumps(lock_target))
            
            # Wait for staker to refresh
            time.sleep(5)

            # 5. THE SAFE SWAP (Fixes the crash)
            # Goal: Target UNLOCKED, Others LOCKED.
            # CRITICAL: We do NOT use 'lockunspent true' (unlock all) here.
            
            # A. Lock 'Others' FIRST. 
            # (Target is already locked from Step 4, so now EVERYTHING is locked).
            others = [u for u in unspent if u['txid'] != target['txid'] or u['vout'] != target['vout']]
            if others:
                lock_others = [{"txid": u['txid'], "vout": u['vout']} for u in others]
                run_rpc(cli_cmd, "lockunspent", "false", json.dumps(lock_others))

            # B. Unlock ONLY the Target
            # Now Target is available for sendmany, but Others are safe.
            # Staker has had no chance to grab Target because we just swapped locks.
            run_rpc(cli_cmd, "lockunspent", "true", json.dumps(lock_target))

            # 6. BROADCAST
            print("  > Broadcasting...")
            recipients = {run_rpc(cli_cmd, "getnewaddress"): args.amount for _ in range(SPLITS_PER_ROUND)}
            txid = run_rpc(cli_cmd, "sendmany", "", json.dumps(recipients), "1", "FanOut")
            
            # 7. RESTORE
            run_rpc(cli_cmd, "lockunspent", "true") # Unlock all
            ensure_staking_active(cli_cmd) 

            if txid:
                print(f"  > Success! TXID: {txid}")
                # Re-lock the spent input immediately so listunspent doesn't pick it up before conf
                run_rpc(cli_cmd, "lockunspent", "false", json.dumps(lock_target))
                
                round_idx += 1
                print("  > Cooling down (30s)...")
                
                # Simple confirmation waiter
                h = run_rpc(cli_cmd, "getblockcount")
                timeout = 0
                while timeout < 20:
                    time.sleep(10)
                    if (run_rpc(cli_cmd, "getblockcount") or 0) > h: 
                        print("  > Block confirmed.")
                        break
                    timeout += 1
            else:
                print("  > Broadcast failed. Retrying...")
                time.sleep(5)

        except Exception as e:
            print(f"  > Error: {e}")
            run_rpc(cli_cmd, "lockunspent", "true")
            ensure_staking_active(cli_cmd)
            time.sleep(10)

    print("\nProcess Complete.")

if __name__ == "__main__":
    main()
