// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

import "./TEENodeManagement.sol";

contract OmniL1 {
    event StateRootUpdated(
        uint64 indexed epochId,
        bytes32 indexed stateRoot,
        bytes32 indexed dagHash,
        uint256 timestamp
    );
    event TokenCreated(
        bytes32 indexed tokenAddress,
        address indexed creator,
        string name,
        string symbol,
        uint256 totalSupply
    );
    event BalanceVerified(
        address indexed account,
        bytes32 indexed tokenAddress,
        uint256 balance,
        bytes32[] merkleProof
    );
    event TransactionVerified(
        bytes32 indexed txHash,
        bool isValid,
        bytes32[] merkleProof
    );

    struct EpochState {
        uint64 epochId;
        bytes32 stateRoot;
        bytes32 dagHash;
        uint256 timestamp;
        bool isConfirmed;
    }

    struct Token {
        bytes32 tokenAddress;
        address creator;
        string name;
        string symbol;
        uint256 totalSupply;
        bool exists;
    }

    TEENodeManagement public teeNodeManagement;
    mapping(uint64 => EpochState) public epochStates;
    mapping(bytes32 => Token) public tokens;
    mapping(uint32 => bool) public leaderNodes;
    
    uint64 public latestConfirmedEpoch;
    bytes32 public latestStateRoot;
    address public owner;
    
    bytes32 public constant CREATE2_SALT = keccak256("OMNI_L1_L2_SALT");

    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner");
        _;
    }

    modifier onlyLeader() {
        require(leaderNodes[getNodeIdFromAddress(msg.sender)], "Only leader");
        _;
    }

    constructor(address _teeNodeManagement) {
        require(_teeNodeManagement != address(0), "Invalid tee node management");
        teeNodeManagement = TEENodeManagement(_teeNodeManagement);
        latestConfirmedEpoch = 0;
        latestStateRoot = bytes32(0);
        owner = msg.sender;
    }

    function submitStateRoot(
        uint64 epochId,
        bytes32 stateRoot,
        bytes32 dagHash,
        bytes calldata signature,
        bytes32 tssPublicKeyHash
    ) external onlyLeader {
        require(epochId > latestConfirmedEpoch, "Invalid epoch");
        require(stateRoot != bytes32(0), "Invalid state root");
        require(dagHash != bytes32(0), "Invalid DAG hash");
        
        bool isValid = teeNodeManagement.verifyEpochSignature(
            epochId,
            stateRoot,
            dagHash,
            signature,
            tssPublicKeyHash
        );
        require(isValid, "Invalid signature");
        
        epochStates[epochId] = EpochState({
            epochId: epochId,
            stateRoot: stateRoot,
            dagHash: dagHash,
            timestamp: block.timestamp,
            isConfirmed: true
        });
        
        latestConfirmedEpoch = epochId;
        latestStateRoot = stateRoot;
        
        emit StateRootUpdated(epochId, stateRoot, dagHash, block.timestamp);
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

    function getLatestStateRoot()
        external
        view
        returns (
            bytes32 stateRoot,
            uint64 epochId,
            uint256 timestamp
        )
    {
        return (
            latestStateRoot,
            latestConfirmedEpoch,
            epochStates[latestConfirmedEpoch].timestamp
        );
    }

    function getEpochState(uint64 epochId)
        external
        view
        returns (
            bytes32 stateRoot,
            bytes32 dagHash,
            uint256 timestamp,
            bool isConfirmed
        )
    {
        EpochState storage state = epochStates[epochId];
        return (
            state.stateRoot,
            state.dagHash,
            state.timestamp,
            state.isConfirmed
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

    function verifyBalance(
        address account,
        bytes32 tokenAddress,
        uint256 balance,
        bytes32[] calldata merkleProof
    ) external returns (bool isValid) {
        require(latestStateRoot != bytes32(0), "No state root");
        
        bytes32 leaf = keccak256(abi.encodePacked(
            account,
            tokenAddress,
            balance
        ));
        
        isValid = verifyMerkleProof(leaf, merkleProof, latestStateRoot);
        
        if (isValid) {
            emit BalanceVerified(account, tokenAddress, balance, merkleProof);
        }
        
        return isValid;
    }

    function verifyTransaction(
        bytes32 txHash,
        bool isValid,
        bytes32[] calldata merkleProof
    ) external returns (bool proofValid) {
        require(latestStateRoot != bytes32(0), "No state root");
        
        bytes32 leaf = keccak256(abi.encodePacked(
            txHash,
            isValid
        ));
        
        proofValid = verifyMerkleProof(leaf, merkleProof, latestStateRoot);
        
        if (proofValid) {
            emit TransactionVerified(txHash, isValid, merkleProof);
        }
        
        return proofValid;
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

    function computeL2Address(address deployer, bytes32 bytecodeHash)
        external
        pure
        returns (address l2Address)
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

    function setLeaderNode(uint32 nodeId, bool isLeader) external onlyOwner {
        leaderNodes[nodeId] = isLeader;
    }

    function getNodeIdFromAddress(address nodeAddress) internal view returns (uint32) {
        for (uint32 i = 1; i < 1000; i++) {
            (address addr, bool registered, , ) = teeNodeManagement.getNodeInfo(i);
            if (registered && addr == nodeAddress) {
                return i;
            }
        }
        return 0;
    }
}

