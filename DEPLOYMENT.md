# Quick Deployment Guide

This document provides steps for quickly deploying the OmniState TEE Committee system.

## Prerequisites Checklist

Before starting deployment, please confirm:

- [ ] Hardware supports AMD SEV-SNP (EPYC 7002+ series CPU)
- [ ] Operating system is Ubuntu 20.04+, kernel version 5.10+
- [ ] AMD SEV-SNP SDK is installed
- [ ] QEMU/KVM and related tools are installed
- [ ] Build toolchain is installed (gcc, g++, make)

## Quick Start (5 minutes)

### Step 1: Environment Setup

```bash
# Set SEV-SNP SDK environment
export SEV_SNP_SDK=/opt/amd/sev-snp-sdk
export PATH=$PATH:$SEV_SNP_SDK/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SEV_SNP_SDK/lib

# Verify SEV-SNP support
cat /proc/cpuinfo | grep sev_snp
```

### Step 2: Build Project

```bash
cd AMD_SEV_SNP
make clean
make
```

### Step 3: Configure System

```bash
# Copy configuration example
cp ../config/test_config.json.example ../config/test_config.json

# Edit configuration as needed
nano ../config/test_config.json
```

### Step 4: Run Demo

```bash
cd HostVM
./host_vm_app --demo
```

## Production Deployment

### 1. Single Node Deployment

Suitable for development and testing environments:

```bash
# Start single node system
cd AMD_SEV_SNP/HostVM
./host_vm_app --config ../../config/test_config.json
```

### 2. Multi-Node Cluster Deployment

#### 2.1 Preparation Phase

Create configuration files for each node:

```bash
# Node 1 configuration
cp config/test_config.json.example config/node1_config.json
# Edit node1_config.json, set node_id=1

# Node 2 configuration
cp config/test_config.json.example config/node2_config.json
# Edit node2_config.json, set node_id=2

# ... and so on
```

#### 2.2 Start Mediator Service

Start Host VM mediator service on one node:

```bash
./host_vm_app --mediator --port 8080 --config ../../config/mediator_config.json
```

#### 2.3 Start Guest VM Nodes

Start Guest VM on each node:

```bash
# Node 1
./host_vm_app --guest --node-id 1 \
  --mediator-url http://mediator-node:8080 \
  --config ../../config/node1_config.json

# Node 2
./host_vm_app --guest --node-id 2 \
  --mediator-url http://mediator-node:8080 \
  --config ../../config/node2_config.json

# ... and so on
```

#### 2.4 Verify Cluster

```bash
# Check cluster status
./host_vm_app --status --mediator-url http://mediator-node:8080

# View node logs
tail -f logs/guest_vm_*.log
```

## Docker Deployment (Optional)

### Build Docker Image

```bash
# Create Dockerfile (needs adjustment based on actual situation)
docker build -t omnistate-tee:latest .
```

### Run Container

```bash
# Run Host VM mediator
docker run -d --name mediator \
  -p 8080:8080 \
  -v $(pwd)/config:/app/config \
  omnistate-tee:latest \
  ./host_vm_app --mediator

# Run Guest VM node
docker run -d --name guest-vm-1 \
  --device=/dev/kvm \
  -v $(pwd)/config:/app/config \
  omnistate-tee:latest \
  ./host_vm_app --guest --node-id 1
```

## Configuration Reference

### Minimal Configuration

```json
{
  "cluster": {
    "tee_count": 2
  },
  "transactions": {
    "total_tx_count": 1000
  },
  "operations": {
    "ops_per_tee": 500
  }
}
```

### Recommended Configuration (Production)

```json
{
  "cluster": {
    "tee_count": 4,
    "max_tee_count": 8
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
  },
  "timing": {
    "leader_election_interval": 10000,
    "epoch_interval": 30000,
    "sync_interval": 5000
  }
}
```

## Monitoring and Logging

### View Logs

```bash
# Host VM logs
tail -f logs/host_vm.log

# Guest VM logs
tail -f logs/guest_vm_*.log

# All logs
tail -f logs/*.log
```

### Monitoring Metrics

The system outputs the following metrics:

- Node status
- Transaction processing rate (TPS)
- Latency statistics
- Conflict rate
- DAG synchronization status

### Health Check

```bash
# Check node health status
./host_vm_app --health-check

# Check cluster status
./host_vm_app --status
```

## Troubleshooting

### Issue 1: SEV-SNP Not Supported

**Symptom**: `cat /proc/cpuinfo | grep sev_snp` returns empty

**Solution**:
1. Confirm CPU supports SEV-SNP (EPYC 7002+)
2. Update BIOS to latest version
3. Enable SEV-SNP feature in BIOS
4. Use kernel supporting SEV-SNP (5.10+)

### Issue 2: Guest VM Startup Failed

**Symptom**: Guest VM cannot start or exits immediately

**Solution**:
1. Check if SEV-SNP SDK is correctly installed
2. Check QEMU/KVM configuration
3. View log files `logs/guest_vm_*.log`
4. Confirm sufficient system resources (memory, CPU)

### Issue 3: Inter-Node Communication Failed

**Symptom**: Nodes cannot communicate with each other

**Solution**:
1. Check if Host VM mediator service is running
2. Check network configuration and firewall rules
3. Verify VM ID configuration is correct
4. Check mediator service URL configuration

### Issue 4: Build Errors

**Symptom**: `make` command fails

**Solution**:
1. Confirm SEV-SNP SDK path is correct (`SEV_SNP_SDK` environment variable)
2. Check if dependencies are installed (`libssl-dev`, `libcrypto++-dev`)
3. Review path configuration in `Makefile`
4. Check compiler version (requires gcc 7+)

## Performance Tuning

### Recommended Settings

- **Number of Nodes**: 4-8 nodes (based on workload)
- **Memory**: At least 4GB per Guest VM
- **CPU**: At least 2 cores per Guest VM
- **Network**: Gigabit Ethernet or faster

### Tuning Parameters

Adjust in configuration file:

```json
{
  "timing": {
    "leader_election_interval": 10000,  // Lower for faster response
    "epoch_interval": 30000,            // Lower for higher throughput
    "sync_interval": 5000               // Lower for better consistency
  }
}
```

## Security Recommendations

1. **Key Management**: Use Hardware Security Module (HSM) for key management
2. **Certificate Verification**: Enable SEV-SNP certificate verification
3. **Access Control**: Configure access control lists for inter-VM communication
4. **Audit Logging**: Enable detailed audit logging
5. **Network Security**: Use VPN or dedicated network to isolate cluster

## Next Steps

After deployment, you can:

1. Read [README.md](README.md) to understand detailed features
2. View [Architecture Documentation](../ARCHITECTURE.md) to understand system design
3. Refer to API documentation for integration into your application
4. Participate in community discussions and contribute code

## Getting Help

- GitHub Issues: [Submit Issues](https://github.com/your-repo/issues)
- Documentation: [Full Documentation](../README.md)
- Email: your-email@example.com
