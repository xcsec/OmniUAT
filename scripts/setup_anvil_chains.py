#!/usr/bin/env python3
"""
Setup Ethereum mainnet + 3 rollup chains using Anvil
Requires Foundry: curl -L https://foundry.paradigm.xyz | bash
"""

import os
import sys
import subprocess
import signal
import time
import json
from pathlib import Path
from typing import List, Tuple, Optional

SCRIPT_DIR = Path(__file__).parent.absolute()
PROJECT_ROOT = SCRIPT_DIR.parent
CHAINS_DIR = PROJECT_ROOT / "chains"
LOGS_DIR = CHAINS_DIR / "logs"
PIDS_DIR = CHAINS_DIR / "pids"
CONFIG_FILE = CHAINS_DIR / "chains_config.json"

CHAINS_CONFIG = {
    "l1": {
        "name": "l1",
        "chain_id": 1,
        "port": 8545,
        "host": "0.0.0.0",
        "block_time": 2,
        "gas_limit": 30000000,
        "accounts": 10,
        "balance": 10000,
    },
    "rollups": [
        {
            "name": "rollup1",
            "chain_id": 42161,
            "port": 8546,
            "host": "0.0.0.0",
            "block_time": 1,
            "gas_limit": 30000000,
            "accounts": 10,
            "balance": 10000,
        },
        {
            "name": "rollup2",
            "chain_id": 42162,
            "port": 8547,
            "host": "0.0.0.0",
            "block_time": 1,
            "gas_limit": 30000000,
            "accounts": 10,
            "balance": 10000,
        },
        {
            "name": "rollup3",
            "chain_id": 42163,
            "port": 8548,
            "host": "0.0.0.0",
            "block_time": 1,
            "gas_limit": 30000000,
            "accounts": 10,
            "balance": 10000,
        },
    ],
    "mnemonic": "test test test test test test test test test test test junk",
}

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'

def print_colored(message: str, color: str = Colors.NC):

def check_anvil_installed() -> bool:
    try:
        result = subprocess.run(
            ["anvil", "--version"],
            capture_output=True,
            text=True,
            timeout=5
        )
        return result.returncode == 0
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False

def create_directories():
    CHAINS_DIR.mkdir(exist_ok=True)
    LOGS_DIR.mkdir(exist_ok=True)
    PIDS_DIR.mkdir(exist_ok=True)

def save_config():
    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(CONFIG_FILE, 'w') as f:
        json.dump(CHAINS_CONFIG, f, indent=2)

def load_config():
    if CONFIG_FILE.exists():
        with open(CONFIG_FILE, 'r') as f:
            return json.load(f)
    return CHAINS_CONFIG

def stop_all_chains():
    print_colored("Stopping all Anvil instances...", Colors.YELLOW)
    
    stopped_count = 0
    
    if PIDS_DIR.exists():
        for pid_file in PIDS_DIR.glob("*.pid"):
            try:
                with open(pid_file, 'r') as f:
                    pid = int(f.read().strip())
                
                chain_name = pid_file.stem
                try:
                    os.kill(pid, signal.SIGTERM)
                    print_colored(f"Stopped {chain_name} (PID: {pid})", Colors.GREEN)
                    stopped_count += 1
                    time.sleep(0.5)
                except ProcessLookupError:
                    pass
                
                pid_file.unlink()
            except (ValueError, FileNotFoundError):
                pass
    
    ports = [CHAINS_CONFIG["l1"]["port"]] + [
        rollup["port"] for rollup in CHAINS_CONFIG["rollups"]
    ]
    
    for port in ports:
        try:
            result = subprocess.run(
                ["lsof", "-ti", f":{port}"],
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                pids = result.stdout.strip().split('\n')
                for pid_str in pids:
                    if pid_str:
                        try:
                            pid = int(pid_str)
                            os.kill(pid, signal.SIGTERM)
                            stopped_count += 1
                        except (ValueError, ProcessLookupError):
                            pass
        except FileNotFoundError:
            pass
    
    if stopped_count > 0:
        print_colored(f"Stopped {stopped_count} instances", Colors.GREEN)
    else:
        print_colored("No running instances", Colors.YELLOW)

def start_chain(chain_config: dict, is_l1: bool = False) -> Optional[int]:
    chain_name = chain_config["name"]
    print_colored(
        f"Starting {'L1 mainnet' if is_l1 else chain_name} "
        f"(Chain ID: {chain_config['chain_id']}, Port: {chain_config['port']})...",
        Colors.GREEN
    )
    
    cmd = [
        "anvil",
        "--chain-id", str(chain_config["chain_id"]),
        "--port", str(chain_config["port"]),
        "--host", chain_config["host"],
        "--block-time", str(chain_config["block_time"]),
        "--gas-limit", str(chain_config["gas_limit"]),
        "--accounts", str(chain_config["accounts"]),
        "--balance", str(chain_config["balance"]),
        "--mnemonic", CHAINS_CONFIG["mnemonic"],
    ]
    
    log_file = LOGS_DIR / f"{chain_name}.log"
    try:
        with open(log_file, 'w') as log:
            process = subprocess.Popen(
                cmd,
                stdout=log,
                stderr=subprocess.STDOUT,
                start_new_session=True
            )
        
        pid_file = PIDS_DIR / f"{chain_name}.pid"
        with open(pid_file, 'w') as f:
            f.write(str(process.pid))
        
        time.sleep(2)
        
        if process.poll() is None:
            print_colored(
                f"{'L1 mainnet' if is_l1 else chain_name} started (PID: {process.pid})",
                Colors.GREEN
            )
            print_colored(f"RPC URL: http://127.0.0.1:{chain_config['port']}", Colors.BLUE)
            return process.pid
        else:
            print_colored(
                f"{'L1 mainnet' if is_l1 else chain_name} failed to start, check logs: {log_file}",
                Colors.RED
            )
            return None
    except Exception as e:
        print_colored(f"Error starting {chain_name}: {e}", Colors.RED)
        return None

def start_all_chains():
    print_colored("Starting all chains...", Colors.YELLOW)
    
    l1_pid = start_chain(CHAINS_CONFIG["l1"], is_l1=True)
    if l1_pid is None:
        print_colored("L1 failed to start, aborting", Colors.RED)
        return False
    
    
    rollup_pids = []
    for rollup_config in CHAINS_CONFIG["rollups"]:
        pid = start_chain(rollup_config, is_l1=False)
        if pid is None:
            print_colored(f"{rollup_config['name']} failed to start, continuing", Colors.YELLOW)
        else:
            rollup_pids.append((rollup_config["name"], pid))
    
    print_colored("=" * 40, Colors.GREEN)
    print_colored("All chains started successfully!", Colors.GREEN)
    print_colored("=" * 40, Colors.GREEN)
    
    print_colored("L1 mainnet:", Colors.YELLOW)
    
    print_colored("Rollup chains:", Colors.YELLOW)
    for rollup_config in CHAINS_CONFIG["rollups"]:
    
    print_colored(f"Logs directory: {LOGS_DIR}", Colors.BLUE)
    print_colored(f"PID files directory: {PIDS_DIR}", Colors.BLUE)
    
    return True

def show_status():
    print_colored("Chain status:", Colors.YELLOW)
    
    l1_pid_file = PIDS_DIR / "l1.pid"
    if l1_pid_file.exists():
        try:
            with open(l1_pid_file, 'r') as f:
                pid = int(f.read().strip())
            try:
                os.kill(pid, 0)
                print_colored(
                    f"✓ L1 mainnet (PID: {pid}, Port: {CHAINS_CONFIG['l1']['port']}) - Running",
                    Colors.GREEN
                )
            except ProcessLookupError:
                print_colored(f"✗ L1 mainnet (PID: {pid}) - Stopped", Colors.RED)
        except (ValueError, FileNotFoundError):
            print_colored("✗ L1 mainnet - Corrupted PID file", Colors.RED)
    else:
        print_colored("✗ L1 mainnet - Not started", Colors.RED)
    
    for rollup_config in CHAINS_CONFIG["rollups"]:
        rollup_name = rollup_config["name"]
        pid_file = PIDS_DIR / f"{rollup_name}.pid"
        
        if pid_file.exists():
            try:
                with open(pid_file, 'r') as f:
                    pid = int(f.read().strip())
                try:
                    os.kill(pid, 0)
                    print_colored(
                        f"✓ {rollup_name} (PID: {pid}, Port: {rollup_config['port']}) - Running",
                        Colors.GREEN
                    )
                except ProcessLookupError:
                    print_colored(f"✗ {rollup_name} (PID: {pid}) - Stopped", Colors.RED)
            except (ValueError, FileNotFoundError):
                print_colored(f"✗ {rollup_name} - Corrupted PID file", Colors.RED)
        else:
            print_colored(f"✗ {rollup_name} - Not started", Colors.RED)

def show_logs(chain_name: str):
    log_file = LOGS_DIR / f"{chain_name}.log"
    if not log_file.exists():
        print_colored(f"Log file does not exist: {log_file}", Colors.RED)
        return
    
    try:
        subprocess.run(["tail", "-f", str(log_file)])
    except KeyboardInterrupt:
        print_colored("\nStopped viewing logs", Colors.YELLOW)
    except FileNotFoundError:
        with open(log_file, 'r') as f:
            print(f.read())

def main():
    global CHAINS_CONFIG
    CHAINS_CONFIG = load_config()
    
    create_directories()
    
    if not check_anvil_installed():
        print_colored("Error: anvil command not found", Colors.RED)
        print_colored("Please install Foundry: curl -L https://foundry.paradigm.xyz | bash", Colors.YELLOW)
        print_colored("Then run: foundryup", Colors.YELLOW)
        sys.exit(1)
    
    command = sys.argv[1] if len(sys.argv) > 1 else "start"
    
    if command == "start":
        stop_all_chains()
        time.sleep(1)
        success = start_all_chains()
        sys.exit(0 if success else 1)
    
    elif command == "stop":
        stop_all_chains()
    
    elif command == "restart":
        stop_all_chains()
        time.sleep(1)
        success = start_all_chains()
        sys.exit(0 if success else 1)
    
    elif command == "status":
        show_status()
    
    elif command == "logs":
        if len(sys.argv) < 3:
            print_colored("Usage: python3 setup_anvil_chains.py logs [l1|rollup1|rollup2|rollup3]", Colors.RED)
            sys.exit(1)
        show_logs(sys.argv[2])
    
    elif command == "config":
        save_config()
        print_colored(f"Configuration saved to: {CONFIG_FILE}", Colors.GREEN)
    
    else:
        print_colored("Usage: python3 setup_anvil_chains.py {start|stop|restart|status|logs|config}", Colors.RED)
        print_colored("Commands:", Colors.YELLOW)
        print("  start   - Start all chains (default)")
        print("  logs    - View logs (e.g., python3 setup_anvil_chains.py logs l1)")
        sys.exit(1)

if __name__ == "__main__":
    def signal_handler(sig, frame):
        print_colored("\nReceived interrupt signal, stopping...", Colors.YELLOW)
        stop_all_chains()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    main()

