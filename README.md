## Prerequisites

### Hardware Requirements

- **AMD CPU**: AMD EPYC processor supporting SEV-SNP (e.g., EPYC 7002 series or newer)
- **Memory**: Recommended at least 16GB RAM
- **Storage**: Recommended at least 50GB available space

### Software Requirements

1. **Operating System**: 
   - Ubuntu 20.04 LTS or newer
   - Kernel version 5.10+ (requires SEV-SNP support)

2. **AMD SEV-SNP SDK**:
   ```bash
   # Download and install AMD SEV-SNP SDK
   # Reference: https://github.com/AMDESE/AMDSEV
   ```

3. **Build Tools**:
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential gcc g++ make cmake
   ```

4. **Dependencies**:
   ```bash
   sudo apt-get install -y libssl-dev libcrypto++-dev
   ```

5. **QEMU/KVM** (for running SEV-SNP Guest VM):
   ```bash
   sudo apt-get install -y qemu-kvm libvirt-daemon-system libvirt-clients
   ```

## Build Steps

### 1. Environment Setup

```bash
# Set SEV-SNP SDK environment variables
export SEV_SNP_SDK=/opt/amd/sev-snp-sdk
export PATH=$PATH:$SEV_SNP_SDK/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SEV_SNP_SDK/lib

# Verify SEV-SNP support
# Check if kernel supports SEV-SNP
cat /proc/cpuinfo | grep sev_snp
```

### 2. Build Project

```bash
cd AMD_SEV_SNP
make
```

This will build:
- Guest VM binary (`GuestVM/sev_guest.bin`)
- Host VM application (`HostVM/host_vm_app`)

### 3. Build Options

```bash
# Debug mode build
make DEBUG=1

# Release mode build (default)
make RELEASE=1

# Clean build files
make clean
```

## Deployment Process

### 1. Configure System

Edit the `config/test_config.json` configuration file:

```json
{
  "cluster": {
    "tee_count": 4,
    "max_tee_count": 10
  },
  "transactions": {
    "total_tx_count": 10000,
    "tx_per_tee": 2500
  },
  "operations": {
    "ops_per_tee": 5000,
    "ops_per_tx": 2
  },
  "conflict": {
    "per_tee_conflict_rate": 0.2,
    "global_conflict_rate": 0.15
  }
}
```

### 2. Start Host VM Mediator Service

```bash
cd AMD_SEV_SNP/HostVM
./host_vm_app --mediator --config ../../config/test_config.json
```

### 3. Start Guest VM Nodes

For each TEE node:

```bash
# Node 1
./host_vm_app --guest --node-id 1 --config ../../config/test_config.json

# Node 2
./host_vm_app --guest --node-id 2 --config ../../config/test_config.json

# ... and so on
```

### 4. Verify Deployment

Check if all nodes started successfully:

```bash
# View node status
./host_vm_app --status

# View logs
tail -f logs/host_vm.log
tail -f logs/guest_vm_*.log
```

## Usage Examples

### Basic Demo

```bash
cd AMD_SEV_SNP/HostVM
./host_vm_app --demo
```

The demo program will:
1. Initialize SEV-SNP Guest VM
2. Initialize TEE cluster (default 4 nodes)
3. Register tokens
4. Start L2 simulator
5. Process transactions and operations
6. Generate epoch state output

### Run with Custom Configuration

```bash
./host_vm_app --config /path/to/custom_config.json
```

### Multi-Node Cluster Deployment

1. **Prepare Configuration Files**: Create separate configuration files for each node
2. **Start Mediator Service**: Start Host VM mediator on one node
3. **Start Guest VMs**: Start Guest VM on each node, specifying node ID
4. **Verify Cluster**: Use `--status` command to check cluster status

## Configuration Reference

### Cluster Configuration (`cluster`)

- `tee_count`: Number of TEE nodes (default: 4)
- `max_tee_count`: Maximum number of nodes (default: 10)

### Transaction Configuration (`transactions`)

- `total_tx_count`: Total number of transactions
- `tx_per_tee`: Number of transactions processed by each TEE

### Operation Configuration (`operations`)

- `ops_per_tee`: Number of operations received by each TEE
- `ops_per_tx`: Number of operations generated per transaction (Transfer typically has 2)

### Conflict Configuration (`conflict`)

- `per_tee_conflict_rate`: Conflict rate within each TEE (0.0-1.0)
- `global_conflict_rate`: Global conflict rate (0.0-1.0)

## Development Guide

### Adding New Features

1. **Core Business Logic**: Add new modules in `Common/` directory
2. **Platform-Specific Implementation**: Add in `AMD_SEV_SNP/GuestVM/` or `AMD_SEV_SNP/HostVM/`
3. **Configuration Items**: Add configuration items in `config/test_config.json`

### Debugging

```bash
# Enable verbose logging
./host_vm_app --debug --log-level verbose

# Debug with GDB
gdb ./host_vm_app
```

### Testing

```bash
# Run unit tests
make test

# Run integration tests
make test-integration
```

## Troubleshooting

### Common Issues

1. **SEV-SNP Not Supported**
   ```bash
   # Check CPU support
   cat /proc/cpuinfo | grep sev_snp
   # If empty, need to update BIOS or use CPU supporting SEV-SNP
   ```

2. **Guest VM Startup Failed**
   - Check if SEV-SNP SDK is correctly installed
   - Check QEMU/KVM configuration
   - View log files `logs/guest_vm_*.log`

3. **Inter-Node Communication Failed**
   - Check if Host VM mediator service is running
   - Check network configuration
   - Verify VM ID configuration is correct

4. **Build Errors**
   - Confirm SEV-SNP SDK path is correct
   - Check if dependencies are installed
   - Review path configuration in `Makefile`

## Performance Optimization

### Recommended Configuration

- **Number of Nodes**: 4-8 nodes (adjust based on workload)
- **Memory**: At least 4GB per Guest VM
- **CPU**: At least 2 cores per Guest VM

### Tuning Parameters

In the configuration file, you can adjust:
- Leader election interval
- Epoch output interval
- Synchronization interval
- DAG synchronization strategy

