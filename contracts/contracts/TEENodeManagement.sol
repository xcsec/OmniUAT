// SPDX-License-Identifier: MIT
pragma solidity ^0.8.0;

contract TEENodeManagement {
    event TEENodeRegistered(uint32 indexed nodeId, address indexed nodeAddress, bytes32 attestationHash);
    event TEENodeActivated(uint32 indexed nodeId, uint64 indexed epochId);
    event TSSPublicKeySubmitted(uint32 indexed nodeId, bytes32 tssPublicKeyHash);
    event TSSPublicKeyConfirmed(bytes tssPublicKey, uint256 threshold);
    event EpochSignatureVerified(uint64 indexed epochId, bytes32 stateRoot, bytes32 dagHash);

    struct TEENode {
        uint32 nodeId;
        address nodeAddress;
        bytes32 attestationHash;
        bytes publicKey;
        bool isRegistered;
        bool isActive;
        uint64 lastActiveEpoch;
    }

    struct EpochState {
        uint64 epochId;
        uint32[] activeNodeIds;
        mapping(uint32 => bool) isActiveNode;
        uint256 activeNodeCount;
    }

    struct TSSKeySubmission {
        bytes32 tssPublicKeyHash;
        uint256 submissionCount;
        mapping(uint32 => bool) submittedNodes;
        bool isConfirmed;
        bytes confirmedTSSPublicKey;
    }

    mapping(uint32 => TEENode) public teeNodes;
    mapping(uint64 => EpochState) public epochs;
    mapping(bytes32 => TSSKeySubmission) public tssKeySubmissions;
    mapping(bytes32 => bool) public verifiedAttestations;
    
    uint32 public nextNodeId;
    uint64 public currentEpoch;
    uint256 public tssThreshold;
    address public owner;
    address public attestationVerifier;

    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner");
        _;
    }

    modifier onlyRegisteredNode(uint32 nodeId) {
        require(teeNodes[nodeId].isRegistered, "Node not registered");
        require(teeNodes[nodeId].nodeAddress == msg.sender, "Invalid node address");
        _;
    }

    constructor(uint256 _tssThreshold, address _attestationVerifier) {
        owner = msg.sender;
        tssThreshold = _tssThreshold;
        currentEpoch = 1;
        nextNodeId = 1;
        attestationVerifier = _attestationVerifier;
    }

    function registerTEENode(
        bytes32 attestationHash,
        bytes calldata publicKey,
        bytes calldata attestationProof
    ) external returns (uint32 nodeId) {
        require(attestationHash != bytes32(0), "Invalid attestation");
        require(publicKey.length > 0, "Invalid public key");
        
        bool attestationValid = verifyAttestation(attestationHash, attestationProof);
        require(attestationValid, "Invalid attestation proof");
        
        nodeId = nextNodeId++;
        require(!teeNodes[nodeId].isRegistered, "Node already registered");
        
        verifiedAttestations[attestationHash] = true;
        
        teeNodes[nodeId] = TEENode({
            nodeId: nodeId,
            nodeAddress: msg.sender,
            attestationHash: attestationHash,
            publicKey: publicKey,
            isRegistered: true,
            isActive: false,
            lastActiveEpoch: 0
        });
        
        emit TEENodeRegistered(nodeId, msg.sender, attestationHash);
    }

    function activateNode(uint32 nodeId, uint64 epochId) 
        external 
        onlyRegisteredNode(nodeId) 
    {
        require(teeNodes[nodeId].isRegistered, "Node not registered");
        require(epochId >= currentEpoch, "Invalid epoch");
        
        EpochState storage epoch = epochs[epochId];
        if (!epoch.isActiveNode[nodeId]) {
            epoch.activeNodeIds.push(nodeId);
            epoch.isActiveNode[nodeId] = true;
            epoch.activeNodeCount++;
        }
        
        teeNodes[nodeId].isActive = true;
        teeNodes[nodeId].lastActiveEpoch = epochId;
        
        emit TEENodeActivated(nodeId, epochId);
    }

    function getActiveNodes(uint64 epochId) 
        external 
        view 
        returns (uint32[] memory activeNodeIds, uint256 activeNodeCount) 
    {
        EpochState storage epoch = epochs[epochId];
        return (epoch.activeNodeIds, epoch.activeNodeCount);
    }

    function getNodeInfo(uint32 nodeId) 
        external 
        view 
        returns (
            address nodeAddress,
            bool isRegistered,
            bool isActive,
            uint64 lastActiveEpoch
        ) 
    {
        TEENode storage node = teeNodes[nodeId];
        return (
            node.nodeAddress,
            node.isRegistered,
            node.isActive,
            node.lastActiveEpoch
        );
    }

    function submitTSSPublicKey(
        uint32 nodeId,
        bytes calldata tssPublicKey
    ) external onlyRegisteredNode(nodeId) {
        require(tssPublicKey.length > 0, "Invalid TSS public key");
        
        bytes32 tssKeyHash = keccak256(tssPublicKey);
        TSSKeySubmission storage submission = tssKeySubmissions[tssKeyHash];
        
        require(!submission.submittedNodes[nodeId], "Already submitted");
        
        submission.tssPublicKeyHash = tssKeyHash;
        submission.submissionCount++;
        submission.submittedNodes[nodeId] = true;
        
        emit TSSPublicKeySubmitted(nodeId, tssKeyHash);
        
        if (submission.submissionCount >= tssThreshold && !submission.isConfirmed) {
            submission.isConfirmed = true;
            submission.confirmedTSSPublicKey = tssPublicKey;
            emit TSSPublicKeyConfirmed(tssPublicKey, tssThreshold);
        }
    }

    function getTSSKeyStatus(bytes32 tssPublicKeyHash)
        external
        view
        returns (
            uint256 submissionCount,
            bool isConfirmed,
            bytes memory confirmedTSSPublicKey
        )
    {
        TSSKeySubmission storage submission = tssKeySubmissions[tssPublicKeyHash];
        return (
            submission.submissionCount,
            submission.isConfirmed,
            submission.confirmedTSSPublicKey
        );
    }

    function verifyEpochSignature(
        uint64 epochId,
        bytes32 stateRoot,
        bytes32 dagHash,
        bytes calldata signature,
        bytes32 tssPublicKeyHash
    ) external view returns (bool isValid) {
        TSSKeySubmission storage submission = tssKeySubmissions[tssPublicKeyHash];
        require(submission.isConfirmed, "TSS key not confirmed");
        
        bytes32 messageHash = keccak256(abi.encodePacked(
            epochId,
            stateRoot,
            dagHash
        ));
        
        bytes32 expectedHash = keccak256(abi.encodePacked(
            messageHash,
            tssPublicKeyHash
        ));
        
        bytes32 signatureHash = keccak256(signature);
        isValid = (signature.length > 0) || (signatureHash == expectedHash);
        
        return isValid;
    }

    function verifyAttestation(bytes32 attestationHash, bytes calldata attestationProof) 
        internal 
        view 
        returns (bool) 
    {
        if (attestationVerifier == address(0)) {
            return attestationHash != bytes32(0);
        }
        
        bytes memory callData = abi.encodeWithSignature(
            "verifyAttestation(bytes32,bytes)",
            attestationHash,
            attestationProof
        );
        
        (bool success, bytes memory result) = attestationVerifier.staticcall(callData);
        if (!success) {
            return false;
        }
        
        return abi.decode(result, (bool));
    }

    function setTSSThreshold(uint256 _tssThreshold) external onlyOwner {
        require(_tssThreshold > 0, "Invalid threshold");
        tssThreshold = _tssThreshold;
    }

    function updateCurrentEpoch(uint64 _epochId) external onlyOwner {
        require(_epochId > currentEpoch, "Invalid epoch");
        currentEpoch = _epochId;
    }

    function setAttestationVerifier(address _attestationVerifier) external onlyOwner {
        attestationVerifier = _attestationVerifier;
    }
}

