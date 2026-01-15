# Smart Contracts

## Contracts

### 1. TEENodeManagement.sol

TEE node management contract deployed on Ethereum L1.

**Key Functions:**
- `registerTEENode(bytes32 attestationHash, bytes publicKey, bytes attestationProof)` - Register a TEE node with attestation verification
- `activateNode(uint32 nodeId, uint64 epochId)` - Activate a node in an epoch
- `submitTSSPublicKey(uint32 nodeId, bytes tssPublicKey)` - Submit TSS public key
- `verifyEpochSignature(...)` - Verify threshold signature for epoch state

**Features:**
- Attestation verification for TEE node registration
- TSS (Threshold Signature Scheme) key management
- Epoch-based node activation

### 2. OmniL1.sol

L1 application contract deployed on Ethereum L1.

**Key Functions:**
- `submitStateRoot(uint64 epochId, bytes32 stateRoot, bytes32 dagHash, bytes signature, bytes32 tssPublicKeyHash)` - Submit state root from leader
- `createToken(bytes32 tokenAddress, string name, string symbol, uint256 totalSupply)` - Create a new token
- `verifyBalance(...)` - Verify account balance using Merkle proof
- `verifyTransaction(...)` - Verify transaction validity using Merkle proof

**Features:**
- Accepts state root submissions from leader nodes
- Token creation functionality
- Merkle proof verification for stateless operations

### 3. OmniL2.sol

L2 application contract deployed on rollup chains.

**Key Functions:**
- `syncStateRootFromL1(...)` - Synchronize state root from L1
- `createToken(bytes32 tokenAddress, string name, string symbol, uint256 totalSupply)` - Create a new token
- `executeTransaction(...)` - Execute transaction with Merkle proof verification
- `recycleLogs(uint64 epochId)` - Recycle and reorganize execution logs

**Features:**
- Synchronizes state roots from L1 with proof verification
- Token creation functionality
- Transaction execution with Merkle proof verification

## Deployment

1. Deploy `TEENodeManagement.sol` on L1
2. Deploy `OmniL1.sol` on L1, passing the `TEENodeManagement` contract address
3. Deploy `OmniL2.sol` on each L2 chain, passing the L1 contract addresses

## Notes

- All contracts use CREATE2 for deterministic address deployment
- Attestation verification can be implemented via an external verifier contract
- Leader nodes are managed through the `setLeaderNode` function in `OmniL1.sol`

