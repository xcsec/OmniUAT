# Experiment Configuration File Guide

## Configuration File Location

Configuration file is located at `config/test_config.json`

## Configuration Parameters

### 1. Cluster Configuration (`cluster`)

- `tee_count`: Number of TEE nodes (default: 3, range: 1-10)
- `max_tee_count`: Maximum number of TEE nodes (default: 10)

### 2. Transaction Configuration (`transactions`)

- `total_tx_count`: Total number of transactions (default: 100)
- `tx_per_tee`: Number of transactions sorted per TEE (default: 30)
- `min_tx_per_tee`: Minimum transactions per TEE (default: 10)
- `max_tx_per_tee`: Maximum transactions per TEE (default: 50)

### 3. Operation Configuration (`operations`)

- `ops_per_tee`: Number of operations received per TEE (default: 100)
- `min_ops_per_tee`: Minimum operations per TEE (default: 50)
- `max_ops_per_tee`: Maximum operations per TEE (default: 200)
- `ops_per_tx`: Number of operations per transaction (default: 2, Transfer transactions typically have 2 operations)

### 4. Conflict Rate Configuration (`conflict`)

- `per_tee_conflict_rate`: Conflict rate within each TEE (default: 0.3, i.e., 30%)
  - Range: 0.0 - 1.0
  - Represents the proportion of operations received by this TEE that will cause conflicts
  
- `global_conflict_rate`: Conflict rate across all TEE operation sets (default: 0.2, i.e., 20%)
  - Range: 0.0 - 1.0
  - Represents the proportion of operations that will cause conflicts after merging all TEE operations
  
- `conflict_account_count`: Number of accounts used for conflicts (default: 10)
  - These accounts will be used to generate conflicting operations
  
- `conflict_token_count`: Number of tokens used for conflicts (default: 3)
  - These tokens will be used to generate conflicting operations

### 5. Failed Transaction Configuration (`failure`)

- `failed_tx_count`: Number of failed transactions (default: 5)
  - These transactions will fail due to insufficient balance or other reasons
  
- `failure_rate`: Failure rate (default: 0.05, i.e., 5%)
  - Range: 0.0 - 1.0
  - Represents the proportion of all transactions that will fail
  
- `insufficient_balance_rate`: Failure rate due to insufficient balance (default: 0.03, i.e., 3%)
  - Range: 0.0 - 1.0
  - Represents the proportion of failed transactions due to insufficient balance

### 6. Token and Account Configuration (`token`, `accounts`)

- `token_count`: Number of tokens (default: 5)
- `initial_balance`: Initial balance (default: 10000)
- `account_count`: Total number of accounts (default: 20)
- `accounts_per_tee`: Number of accounts managed per TEE (default: 7)

### 7. Timing Configuration (`timing`)

- `leader_election_interval`: Leader election interval (milliseconds, default: 10000)
- `epoch_interval`: Epoch output interval (milliseconds, default: 30000)
- `sync_interval`: Synchronization interval (milliseconds, default: 5000)

## Usage

### 1. Modify Configuration File

Edit `config/test_config.json` to adjust parameters as needed:

```json
{
  "cluster": {
    "tee_count": 5
  },
  "operations": {
    "ops_per_tee": 200
  },
  "conflict": {
    "per_tee_conflict_rate": 0.4,
    "global_conflict_rate": 0.25
  },
  "failure": {
    "failed_tx_count": 10,
    "failure_rate": 0.1
  }
}
```

### 2. Run Experiment

```bash
# Use default configuration file
./test_experiment

# Or specify configuration file
./test_experiment config/test_config.json

# Or use custom configuration file
./test_experiment my_experiment_config.json
```

### 3. View Results

The program will output:
- Configuration information
- Number of operations generated per TEE
- Conflict rate statistics (per TEE and global)
- Failed transaction statistics

## Parameter Adjustment Suggestions

### Test Different TEE Counts
```json
"cluster": {
  "tee_count": 3  // Change to 5, 7, 10, etc.
}
```

### Test Different Conflict Rates
```json
"conflict": {
  "per_tee_conflict_rate": 0.1,   // Low conflict rate
  "global_conflict_rate": 0.05
}
```

```json
"conflict": {
  "per_tee_conflict_rate": 0.8,   // High conflict rate
  "global_conflict_rate": 0.6
}
```

### Test Different Failure Rates
```json
"failure": {
  "failed_tx_count": 20,
  "failure_rate": 0.2  // 20% failure rate
}
```

### Test Different Operation Counts
```json
"operations": {
  "ops_per_tee": 500,  // Large number of operations
  "ops_per_tx": 2
}
```

## Output Description

The program will output the following statistics:

1. **Configuration Information**: Display all configuration parameters
2. **Operation Generation**: Number of operations generated per TEE
3. **Conflict Rate Analysis**: 
   - Conflict rate per TEE
   - Global conflict rate
   - Comparison with target conflict rate
4. **Failed Transaction Analysis**:
   - Number of failed transactions per TEE
   - Total number of failed transactions
   - Comparison with target failure count

## Notes

1. `per_tee_conflict_rate` and `global_conflict_rate` are target values, actual conflict rates may vary slightly
2. `failed_tx_count` is an exact value, will generate the specified number of failed transactions
3. Number of conflict accounts and tokens affects conflict distribution
4. Initial balance needs to be large enough, otherwise many transactions will fail
