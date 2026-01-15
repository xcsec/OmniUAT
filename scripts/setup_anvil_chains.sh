#!/bin/bash

# Setup Ethereum mainnet + 3 rollup chains using Anvil
# Requires Foundry: curl -L https://foundry.paradigm.xyz | bash

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CHAINS_DIR="$PROJECT_ROOT/chains"
LOGS_DIR="$CHAINS_DIR/logs"
PIDS_DIR="$CHAINS_DIR/pids"

L1_CHAIN_ID=1
L1_PORT=8545
L1_RPC_URL="http://127.0.0.1:$L1_PORT"

ROLLUP_CHAINS=(
    "rollup1:42161:8546"
    "rollup2:42162:8547"
    "rollup3:42163:8548"
)

mkdir -p "$CHAINS_DIR" "$LOGS_DIR" "$PIDS_DIR"

if ! command -v anvil &> /dev/null; then
    echo -e "${RED}Error: anvil command not found${NC}"
    echo "Please install Foundry: curl -L https://foundry.paradigm.xyz | bash"
    echo "Then run: foundryup"
    exit 1
fi

stop_chains() {
    echo -e "${YELLOW}Stopping all Anvil instances...${NC}"
    
    if [ -d "$PIDS_DIR" ]; then
        for pid_file in "$PIDS_DIR"/*.pid; do
            if [ -f "$pid_file" ]; then
                pid=$(cat "$pid_file")
                chain_name=$(basename "$pid_file" .pid)
                if kill -0 "$pid" 2>/dev/null; then
                    echo "Stopping $chain_name (PID: $pid)"
                    kill "$pid" 2>/dev/null || true
                fi
                rm -f "$pid_file"
            fi
        done
    fi
    
    for port in $L1_PORT 8546 8547 8548; do
        pid=$(lsof -ti:$port 2>/dev/null || true)
        if [ ! -z "$pid" ]; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    
    echo -e "${GREEN}All Anvil instances stopped${NC}"
}

start_l1() {
    echo -e "${GREEN}Starting L1 mainnet (Chain ID: $L1_CHAIN_ID, Port: $L1_PORT)...${NC}"
    
    anvil \
        --chain-id $L1_CHAIN_ID \
        --port $L1_PORT \
        --host 0.0.0.0 \
        --block-time 2 \
        --gas-limit 30000000 \
        --accounts 10 \
        --balance 10000 \
        --mnemonic "test test test test test test test test test test test junk" \
        > "$LOGS_DIR/l1.log" 2>&1 &
    
    L1_PID=$!
    echo $L1_PID > "$PIDS_DIR/l1.pid"
    echo -e "${GREEN}L1 mainnet started (PID: $L1_PID)${NC}"
    echo "RPC URL: $L1_RPC_URL"
    
    sleep 2
    if ! kill -0 $L1_PID 2>/dev/null; then
        echo -e "${RED}L1 mainnet failed to start, check logs: $LOGS_DIR/l1.log${NC}"
        exit 1
    fi
}

start_rollup() {
    local name=$1
    local chain_id=$2
    local port=$3
    
    echo -e "${GREEN}Starting $name (Chain ID: $chain_id, Port: $port)...${NC}"
    
    anvil \
        --chain-id $chain_id \
        --port $port \
        --host 0.0.0.0 \
        --block-time 1 \
        --gas-limit 30000000 \
        --accounts 10 \
        --balance 10000 \
        --mnemonic "test test test test test test test test test test test junk" \
        > "$LOGS_DIR/$name.log" 2>&1 &
    
    local pid=$!
    echo $pid > "$PIDS_DIR/$name.pid"
    echo -e "${GREEN}$name started (PID: $pid)${NC}"
    echo "RPC URL: http://127.0.0.1:$port"
    
    sleep 1
    if ! kill -0 $pid 2>/dev/null; then
        echo -e "${RED}$name failed to start, check logs: $LOGS_DIR/$name.log${NC}"
        exit 1
    fi
}

start_all_chains() {
    echo -e "${YELLOW}Starting all chains...${NC}"
    
    start_l1
    
    for chain_config in "${ROLLUP_CHAINS[@]}"; do
        IFS=':' read -r name chain_id port <<< "$chain_config"
        start_rollup "$name" "$chain_id" "$port"
    done
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}All chains started successfully!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "L1 mainnet:"
    echo "  Chain ID: $L1_CHAIN_ID"
    echo "  RPC URL: $L1_RPC_URL"
    echo ""
    echo "Rollup chains:"
    for chain_config in "${ROLLUP_CHAINS[@]}"; do
        IFS=':' read -r name chain_id port <<< "$chain_config"
        echo "  $name:"
        echo "    Chain ID: $chain_id"
        echo "    RPC URL: http://127.0.0.1:$port"
    done
    echo ""
    echo "Logs directory: $LOGS_DIR"
    echo "PID files directory: $PIDS_DIR"
    echo ""
    echo "Stop all chains: $0 stop"
    echo "Check status: $0 status"
}

show_status() {
    echo -e "${YELLOW}Chain status:${NC}"
    echo ""
    
    if [ -f "$PIDS_DIR/l1.pid" ]; then
        pid=$(cat "$PIDS_DIR/l1.pid")
        if kill -0 "$pid" 2>/dev/null; then
            echo -e "${GREEN}✓${NC} L1 mainnet (PID: $pid, Port: $L1_PORT) - Running"
        else
            echo -e "${RED}✗${NC} L1 mainnet (PID: $pid) - Stopped"
        fi
    else
        echo -e "${RED}✗${NC} L1 mainnet - Not started"
    fi
    
    for chain_config in "${ROLLUP_CHAINS[@]}"; do
        IFS=':' read -r name chain_id port <<< "$chain_config"
        if [ -f "$PIDS_DIR/$name.pid" ]; then
            pid=$(cat "$PIDS_DIR/$name.pid")
            if kill -0 "$pid" 2>/dev/null; then
                echo -e "${GREEN}✓${NC} $name (PID: $pid, Port: $port) - Running"
            else
                echo -e "${RED}✗${NC} $name (PID: $pid) - Stopped"
            fi
        else
            echo -e "${RED}✗${NC} $name - Not started"
        fi
    done
}

main() {
    case "${1:-start}" in
        start)
            stop_chains
            start_all_chains
            ;;
        stop)
            stop_chains
            ;;
        restart)
            stop_chains
            sleep 1
            start_all_chains
            ;;
        status)
            show_status
            ;;
        logs)
            if [ -z "$2" ]; then
                echo "Usage: $0 logs [l1|rollup1|rollup2|rollup3]"
                exit 1
            fi
            if [ -f "$LOGS_DIR/$2.log" ]; then
                tail -f "$LOGS_DIR/$2.log"
            else
                echo "Log file does not exist: $LOGS_DIR/$2.log"
                exit 1
            fi
            ;;
        *)
            echo "Usage: $0 {start|stop|restart|status|logs}"
            echo ""
            echo "Commands:"
            echo "  start   - Start all chains (default)"
            echo "  stop    - Stop all chains"
            echo "  restart - Restart all chains"
            echo "  status  - Show chain status"
            echo "  logs    - View logs (e.g., $0 logs l1)"
            exit 1
            ;;
    esac
}

trap 'echo -e "\n${YELLOW}Received interrupt signal, stopping...${NC}"; stop_chains; exit 0' INT TERM

main "$@"

