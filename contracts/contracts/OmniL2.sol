// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

contract OmniL2 {
    event StateRootSynced(
        uint64 indexed epochId,
        bytes32 indexed stateRoot,
        bytes32 indexed l1BlockHash,
        uint256 timestamp
    );
    event TokenCreated(
        bytes32 indexed tokenAddress,
        address indexed creator,
        string name,
        string symbol,
        uint256 totalSupply
    );
    event TransactionExecuted(
        bytes32 indexed txHash,
        address indexed account,
        bytes32 indexed tokenAddress,
        uint256 amount,
        bool isValid
    );
    event LogRecycled(
        uint64 indexed epochId,
        uint256 confirmedTxCount,
        uint256 rejectedTxCount
    );

    struct SyncedState {
        uint64 epochId;
        bytes32 stateRoot;
        bytes32 l1BlockHash;
        uint256 blockNumber;
        uint256 timestamp;
        bool isSynced;
    }

    struct Token {
        bytes32 tokenAddress;
        address creator;
        string name;
        string symbol;
        uint256 totalSupply;
        bool exists;
    }

    struct TransactionLog {
        bytes32 txHash;
        address account;
        bytes32 tokenAddress;
        uint256 amount;
        bool isValid;
        uint64 epochId;
        bool isConfirmed;
        bool isRejected;
    }

    address public l1OmniContract;
    address public l1BlockHeaderVerifier;
    
    mapping(uint64 => SyncedState) public syncedStates;
    mapping(bytes32 => TransactionLog) public transactionLogs;
    mapping(bytes32 => Token) public tokens;
    mapping(uint64 => bytes32[]) public epochTransactions;
    
    uint64 public latestSyncedEpoch;
    bytes32 public latestSyncedStateRoot;
    
    bytes32 public constant CREATE2_SALT = keccak256("OMNI_L1_L2_SALT");

    modifier onlyL1Contract() {
        require(msg.sender == l1OmniContract, "Only L1 contract");
        _;
    }

    constructor(address _l1OmniContract, address _l1BlockHeaderVerifier) {
        require(_l1OmniContract != address(0), "Invalid L1 contract");
        require(_l1BlockHeaderVerifier != address(0), "Invalid verifier");
        l1OmniContract = _l1OmniContract;
        l1BlockHeaderVerifier = _l1BlockHeaderVerifier;
        latestSyncedEpoch = 0;
        latestSyncedStateRoot = bytes32(0);
    }

    function syncStateRootFromL1(
        uint64 epochId,
        bytes32 stateRoot,
        bytes32 l1BlockHash,
        uint256 l1BlockNumber,
        bytes32[] calldata stateProof,
        bytes32[] calldata blockHeaderProof
    ) external {
        require(epochId > latestSyncedEpoch, "Invalid epoch");
        require(stateRoot != bytes32(0), "Invalid state root");
        
        bool blockHeaderValid = verifyL1BlockHeader(
            l1BlockHash,
            l1BlockNumber,
            blockHeaderProof
        );
        require(blockHeaderValid, "Invalid L1 block header");
        
        bool stateProofValid = verifyL1StateProof(
            stateRoot,
            l1BlockHash,
            stateProof
        );
        require(stateProofValid, "Invalid L1 state proof");
        
        syncedStates[epochId] = SyncedState({
            epochId: epochId,
            stateRoot: stateRoot,
            l1BlockHash: l1BlockHash,
            blockNumber: l1BlockNumber,
            timestamp: block.timestamp,
            isSynced: true
        });
        
        latestSyncedEpoch = epochId;
        latestSyncedStateRoot = stateRoot;
        
        emit StateRootSynced(epochId, stateRoot, l1BlockHash, block.timestamp);
    }

    function createToken(
        bytes32 tokenAddress,
        string calldata name,
        string calldata symbol,
        uint256 totalSupply
    ) external {
        require(tokenAddress != bytes32(0), "Invalid token address");
        require(!tokens[tokenAddress].exists, "Token already exists");
        require(totalSupply > 0, "Invalid total supply");
        
        tokens[tokenAddress] = Token({
            tokenAddress: tokenAddress,
            creator: msg.sender,
            name: name,
            symbol: symbol,
            totalSupply: totalSupply,
            exists: true
        });
        
        emit TokenCreated(tokenAddress, msg.sender, name, symbol, totalSupply);
    }

    function executeTransaction(
        bytes32 txHash,
        address account,
        bytes32 tokenAddress,
        uint256 amount,
        bool isValid,
        bytes32[] calldata balanceProof,
        bytes32[] calldata txProof
    ) external {
        require(latestSyncedStateRoot != bytes32(0), "No synced state root");
        require(!transactionLogs[txHash].isConfirmed, "Transaction already confirmed");
        
        bytes32 balanceLeaf = keccak256(abi.encodePacked(
            account,
            tokenAddress,
            amount
        ));
        bool balanceValid = verifyMerkleProof(
            balanceLeaf,
            balanceProof,
            latestSyncedStateRoot
        );
        require(balanceValid, "Invalid balance proof");
        
        bytes32 txLeaf = keccak256(abi.encodePacked(txHash, isValid));
        bool txValid = verifyMerkleProof(
            txLeaf,
            txProof,
            latestSyncedStateRoot
        );
        require(txValid, "Invalid transaction proof");
        
        TransactionLog storage log = transactionLogs[txHash];
        log.txHash = txHash;
        log.account = account;
        log.tokenAddress = tokenAddress;
        log.amount = amount;
        log.isValid = isValid;
        log.epochId = latestSyncedEpoch;
        log.isConfirmed = false;
        log.isRejected = false;
        
        epochTransactions[latestSyncedEpoch].push(txHash);
        
        emit TransactionExecuted(txHash, account, tokenAddress, amount, isValid);
    }

    function recycleLogs(uint64 epochId) external {
        require(epochId < latestSyncedEpoch, "Cannot recycle current epoch");
        require(syncedStates[epochId].isSynced, "Epoch not synced");
        
        bytes32[] storage txs = epochTransactions[epochId];
        uint256 confirmedCount = 0;
        uint256 rejectedCount = 0;
        
        bytes32 epochStateRoot = syncedStates[epochId].stateRoot;
        
        for (uint256 i = 0; i < txs.length; i++) {
            bytes32 txHash = txs[i];
            TransactionLog storage log = transactionLogs[txHash];
            
            if (log.isConfirmed || log.isRejected) {
                continue;
            }
            
            bytes32 txLeaf = keccak256(abi.encodePacked(txHash, log.isValid));
            
            if (log.isValid) {
                log.isConfirmed = true;
                confirmedCount++;
            } else {
                log.isRejected = true;
                rejectedCount++;
            }
        }
        
        emit LogRecycled(epochId, confirmedCount, rejectedCount);
    }

    function getLatestSyncedStateRoot()
        external
        view
        returns (bytes32 stateRoot, uint64 epochId)
    {
        return (latestSyncedStateRoot, latestSyncedEpoch);
    }

    function getTransactionLog(bytes32 txHash)
        external
        view
        returns (
            address account,
            bytes32 tokenAddress,
            uint256 amount,
            bool isValid,
            bool isConfirmed,
            bool isRejected
        )
    {
        TransactionLog storage log = transactionLogs[txHash];
        return (
            log.account,
            log.tokenAddress,
            log.amount,
            log.isValid,
            log.isConfirmed,
            log.isRejected
        );
    }

    function getTokenInfo(bytes32 tokenAddress)
        external
        view
        returns (
            address creator,
            string memory name,
            string memory symbol,
            uint256 totalSupply,
            bool exists
        )
    {
        Token storage token = tokens[tokenAddress];
        return (
            token.creator,
            token.name,
            token.symbol,
            token.totalSupply,
            token.exists
        );
    }

    function verifyL1BlockHeader(
        bytes32 blockHash,
        uint256 blockNumber,
        bytes32[] calldata proof
    ) internal pure returns (bool) {
        require(blockHash != bytes32(0), "Invalid block hash");
        require(blockNumber > 0, "Invalid block number");
        return true;
    }

    function verifyL1StateProof(
        bytes32 stateRoot,
        bytes32 blockHash,
        bytes32[] calldata proof
    ) internal pure returns (bool) {
        require(stateRoot != bytes32(0), "Invalid state root");
        return true;
    }

    function verifyMerkleProof(
        bytes32 leaf,
        bytes32[] calldata proof,
        bytes32 root
    ) internal pure returns (bool isValid) {
        bytes32 computedHash = leaf;
        
        for (uint256 i = 0; i < proof.length; i++) {
            bytes32 proofElement = proof[i];
            
            if (computedHash < proofElement) {
                computedHash = keccak256(abi.encodePacked(computedHash, proofElement));
            } else {
                computedHash = keccak256(abi.encodePacked(proofElement, computedHash));
            }
        }
        
        return computedHash == root;
    }

    function computeContractAddress(address deployer, bytes32 bytecodeHash)
        external
        pure
        returns (address contractAddress)
    {
        bytes32 hash = keccak256(
            abi.encodePacked(
                bytes1(0xff),
                deployer,
                CREATE2_SALT,
                bytecodeHash
            )
        );
        return address(uint160(uint256(hash)));
    }
}

