// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2015-2018 The KORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>

#include "db.h"
#include "kernel.h"
#include "script/interpreter.h"
#include "stakeinput.h"
#include "timedata.h"
#include "util.h"
#include "wallet.h"

using namespace std;

// Protocol V3: Check if we should use V3 stake modifier algorithm
bool UseProtocolV3(int nHeight)
{
    return nHeight >= Params().ProtocolV3StartHeight();
}

// Helper: Convert 64-bit legacy modifier to 256-bit for V3 fork boundary
uint256 ConvertModifier64To256(uint64_t nModifier64)
{
    uint256 result;
    result.SetNull();
    // Place the 64-bit value in the low bytes (little-endian)
    memcpy(result.begin(), &nModifier64, sizeof(nModifier64));
    return result;
}

// Protocol V3: Compute stake modifier using Hash(PrevModifier + Kernel)
// Returns full 256-bit hash for maximum entropy (2^256 search space)
// This eliminates stake grinding attacks by making pre-computation infeasible
uint256 ComputeStakeModifierV2(const uint256& hashKernel, const uint256& nPrevModifier)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << nPrevModifier;  // Previous modifier first
    ss << hashKernel;     // Kernel hash second
    return ss.GetHash();  // Return full 256-bit hash
}

// Hard checkpoints of stake modifiers to ensure they are deterministic
static std::map<int, unsigned int> mapStakeModifierCheckpoints =
    boost::assign::map_list_of(0, 0xfd11f4e7u);

// Get time weight
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    return nIntervalEnd - nIntervalBeginning - Params().GetStakeMinAge();
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: no generation at genesis block");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    int nminimum = Params().GetMaxStakeModifierInterval() - 1;
    assert(nSection >= 0 && nSection < Params().GetMaxStakeModifierInterval());
    return Params().GetModifierInterval() * nminimum / (nminimum + ((nminimum - nSection) * (MODIFIER_INTERVAL_RATIO - 1)));
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval(int nHeight)
{
    int64_t nSelectionInterval = 0;
    for (int nSection = 0; nSection < Params().GetMaxStakeModifierInterval(); nSection++) {
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);
    }
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(
    vector<pair<int64_t, uint256> >& vSortedByTimestamp,
    map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop,
    uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    bool fModifierV2 = false;
    bool fSelected = false;
    uint256 hashBest = 0;
    *pindexSelected = (const CBlockIndex*)0;
    BOOST_FOREACH (const PAIRTYPE(int64_t, uint256) & item, vSortedByTimestamp) {
        if (!mapBlockIndex.count(item.second))
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s", item.second.ToString().c_str());

        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;

        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;

        // compute the selection hash by hashing an input that is unique to that block
        uint256 hashProof;
        hashProof = pindex->GetBlockHash();

        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        uint256 hashSelection = Hash(ss.begin(), ss.end());

        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection >>= 32;

        if (fSelected && hashSelection < hashBest) {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        } else if (!fSelected) {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        }
    }
    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("SelectBlockFromCandidates: selection hash=%s\n", hashBest.ToString().c_str());
    return fSelected;
}

// This function will only be used before fork happens
void StartStakeModifier_Legacy(CBlockIndex* pindexNew)
{
    if (pindexNew->nHeight < Params().GetLastPoWBlock()) {
        //Give a stake modifier to the first block
        // Lets give a stake modifier to the last block
        uint64_t nStakeModifier = PREDEFINED_MODIFIER_LEGACY;
        pindexNew->SetStakeModifier(nStakeModifier, true);
        pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);

        if (pindexNew->nHeight)
            pindexNew->pprev->pnext = pindexNew;

        // mark as PoS seen
        if (pindexNew->IsProofOfStake())
            setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    }
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
//
// V3 UPGRADE: Returns full 256-bit modifier for enhanced grinding resistance.
// Pre-V3 blocks continue using legacy 64-bit algorithm (converted to 256-bit).
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint256& nStakeModifier, bool& fGeneratedStakeModifier)
{
    nStakeModifier.SetNull();
    fGeneratedStakeModifier = false;
    if (!pindexPrev) {
        fGeneratedStakeModifier = true;
        return true; // genesis block's modifier is 0
    }

    // Check for fork boundaries where we need to seed a new modifier
    int nextHeight = pindexPrev->nHeight + 1;

    // Protocol V3: Use full 256-bit Hash(PrevModifier + Kernel) for ALL V3+ blocks
    if (UseProtocolV3(nextHeight)) {
        fGeneratedStakeModifier = true;
        uint256 hashKernel = pindexPrev->GetBlockHash();

        // Determine previous modifier for V3 computation
        uint256 nPrevMod;
        if (!UseProtocolV3(pindexPrev->nHeight)) {
            // Fork boundary: convert legacy 64-bit to 256-bit
            nPrevMod = ConvertModifier64To256(pindexPrev->nStakeModifier);
            if (GetBoolArg("-printstakemodifier", false))
                LogPrintf("ComputeNextStakeModifier: V3 fork boundary at height %d, converting 64-bit modifier to 256-bit\n", nextHeight);
        } else if (pindexPrev->nHeight == 0 || pindexPrev->nStakeModifierV2.IsNull()) {
            // Genesis block or uninitialized V3 modifier: seed with genesis hash
            // This handles V3-from-genesis chains (testnet/regtest with V3 height = 0)
            nPrevMod = pindexPrev->GetBlockHash();
            if (GetBoolArg("-printstakemodifier", false))
                LogPrintf("ComputeNextStakeModifier: V3 genesis seed at height %d, using block hash as initial modifier\n", nextHeight);
        } else {
            // Post-fork: use previous 256-bit modifier directly
            nPrevMod = pindexPrev->nStakeModifierV2;
        }

        nStakeModifier = ComputeStakeModifierV2(hashKernel, nPrevMod);
        if (GetBoolArg("-printstakemodifier", false))
            LogPrintf("ComputeNextStakeModifier: V3 block %d, 256-bit modifier=%s\n",
                      nextHeight, nStakeModifier.ToString());
        return true;
    }

    // ========== LEGACY CODE PATH (Pre-V3 blocks) ==========

    // Legacy fork boundary (HeightToFork): use static modifier for backward compatibility
    if (pindexPrev->nHeight == 0 || Params().HeightToFork() == nextHeight) {
        fGeneratedStakeModifier = true;
        nStakeModifier = ConvertModifier64To256(PREDEFINED_MODIFIER_LEGACY);
        return true;
    }

    // First find current stake modifier and its generation block time
    uint64_t nStakeModifier64 = 0;
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier64, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");

    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("ComputeNextStakeModifier: prev modifier= %s time=%s\n", boost::lexical_cast<std::string>(nStakeModifier64).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nModifierTime).c_str());

    // Checks if it is already time to generate a new Modifier
    if (Params().GetModifierInterval() > pindexPrev->GetBlockTime() - nModifierTime) {
        nStakeModifier = ConvertModifier64To256(nStakeModifier64);
        return true;
    }

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(Params().GetMaxStakeModifierInterval() * Params().GetModifierInterval() / Params().GetTargetSpacingForStake());
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval(pindexPrev->nHeight);
    int64_t nSelectionIntervalStart = pindexPrev->GetBlockTime() - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;

    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart) {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }

    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier (legacy algorithm)
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound = 0; nRound < min(Params().GetMaxStakeModifierInterval(), (int)vSortedByTimestamp.size()); nRound++) {
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);

        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier64, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);

        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));

        if (GetBoolArg("-printstakemodifier", false))
            LogPrintf("ComputeNextStakeModifier: selected round %d stop=%s height=%d bit=%d\n",
                nRound, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nSelectionIntervalStop).c_str(), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    if (GetBoolArg("-printstakemodifier", false)) {
        LogPrintf("ComputeNextStakeModifier: new modifier=%s time=%s\n", boost::lexical_cast<std::string>(nStakeModifierNew).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexPrev->GetBlockTime()).c_str());
    }

    // Convert legacy 64-bit result to 256-bit
    nStakeModifier = ConvertModifier64To256(nStakeModifierNew);
    fGeneratedStakeModifier = true;
    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
// V3: Returns full 256-bit modifier for enhanced security
bool GetKernelStakeModifier(uint256 hashBlockFrom, uint256& nStakeModifier, bool fPrintProofOfStake)
{
    nStakeModifier.SetNull();
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];

    // Lets check if the block is from before fork, if so we need to change it
    if (UseLegacyCode(pindexFrom->nHeight)) {
        // for all blocks before fork we will return a modifier as
        // it was found at the last block before fork
        nStakeModifier = ConvertModifier64To256(PREDEFINED_MODIFIER_LEGACY);

        if(fDebug)
            LogPrintf("GetKernelStakeModifier(): PREDEFINED_MODIFIER_LEGACY, nStakeModifier=%s\n", nStakeModifier.ToString());

        return true;
    }

    int32_t nStakeModifierHeight = pindexFrom->nHeight;
    int64_t nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval(pindexFrom->nHeight);
    int64_t nTargetTime = pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval;
    const CBlockIndex* pindex = pindexFrom;
    // Use pnext first (works during reindex), fall back to chainActive if needed
    CBlockIndex* pindexNext = pindexFrom->pnext;
    if (!pindexNext && chainActive.Height() > pindexFrom->nHeight) {
        pindexNext = chainActive[pindexFrom->nHeight + 1];
    }

    if (fDebug) {
        LogPrintf("GetKernelStakeModifier coin from             : %d \n", pindexFrom->nHeight);
        LogPrintf("GetKernelStakeModifier StakeModifierInterval : %d \n", nStakeModifierSelectionInterval);
        LogPrintf("GetKernelStakeModifier target Time           : %u \n", nTargetTime);
    }

    // loop to find the stake modifier later by a selection interval
    while (nStakeModifierTime < nTargetTime) {
        if (!pindexNext) {
            // Edge case: no more blocks available
            // V3: Return error instead of falling back to static modifier
            if (UseProtocolV3(pindex->nHeight)) {
                return error("GetKernelStakeModifier(): V3 requires complete chain, no pindexNext at height %d", pindex->nHeight);
            }
            // Legacy fallback
            nStakeModifier = ConvertModifier64To256(PREDEFINED_MODIFIER_LEGACY);
            return true;
        }

        pindex = pindexNext;
        // Use pnext first (works during reindex), fall back to chainActive if needed
        pindexNext = pindex->pnext;
        if (!pindexNext && chainActive.Height() > pindex->nHeight) {
            pindexNext = chainActive[pindex->nHeight + 1];
        }
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
        if (fDebug)
            LogPrintf("GetKernelStakeModifier nStakeModifierTime           : %u  nStakeModifierHeight : %d block: %d \n", nStakeModifierTime, nStakeModifierHeight, pindex->nHeight);
    }

    // V3: Return 256-bit modifier directly
    if (UseProtocolV3(pindex->nHeight)) {
        nStakeModifier = pindex->nStakeModifierV2;
        // Safety check: V3 modifier should never be zero (would indicate uninitialized data)
        if (nStakeModifier.IsNull()) {
            // At V3 boundary, compute from legacy modifier as fallback
            if (!UseProtocolV3(pindex->nHeight - 1) && pindex->pprev) {
                nStakeModifier = ConvertModifier64To256(pindex->pprev->nStakeModifier);
                LogPrintf("GetKernelStakeModifier(): WARNING - V3 modifier was null at height %d, using converted legacy modifier\n", pindex->nHeight);
            } else {
                return error("GetKernelStakeModifier(): V3 modifier is null at height %d", pindex->nHeight);
            }
        }
    } else {
        // Legacy: Convert 64-bit to 256-bit
        nStakeModifier = ConvertModifier64To256(pindex->nStakeModifier);
    }
    return true;
}

// V3: Uses 256-bit stake modifier for enhanced grinding resistance
bool CheckStake(const CDataStream& ssUniqueID, CAmount nValueIn, const uint256& nStakeModifier, const uint256& bnTarget, unsigned int nTimeBlockFrom, unsigned int& nTimeTx)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << nStakeModifier << nTimeBlockFrom << ssUniqueID << nTimeTx;
    uint256 hashProofOfStake = ss.GetHash();

    // get the stake weight - weight is equal to coin amount
    uint256 bnCoinDayWeight = uint256(nValueIn) / MINIMUM_STAKE_VALUE;

    // Now check if proof-of-stake hash meets target protocol
    bool canStake = hashProofOfStake / bnCoinDayWeight < bnTarget;

    if (fDebug) {
        LogPrintf("CheckStake()\n");
        LogPrintf("hash:                %s\n", hashProofOfStake.ToString());
        LogPrintf("hashWeightened:      %s\n", (hashProofOfStake / bnCoinDayWeight).ToString());
        LogPrintf("target:              %s\n", bnTarget.ToString());
        LogPrintf("bnCoinDayWeight:     %s\n", bnCoinDayWeight.ToString());
        LogPrintf("nValueIn:            %s\n", nValueIn);
        LogPrintf("nStakeModifier:      %s\n", nStakeModifier.ToString());
        LogPrintf("nTimeBlockFrom:      %u\n", nTimeBlockFrom);
        LogPrintf("nTimeTx:             %u\n", nTimeTx);
        LogPrintf("%s\n", canStake ? "Can stake" : "Can't stake");
    }

    return canStake;
}

bool IsBelowMinAge(const COutput& output, const unsigned int nTimeBlockFrom, const unsigned int nTimeTx)
{
    return (output.nDepth < Params().GetCoinMaturity() && nTimeTx - nTimeBlockFrom < Params().GetStakeMinAge());
}

bool Stake(CStakeInput* stakeInput, unsigned int nBits, unsigned int nTimeBlockFrom, unsigned int& nTimeTx, CAmount stakeableBalance, const COutput& output)
{
    if (nTimeTx < nTimeBlockFrom)
        return error("Stake() : nTime violation => nTimeTx=%d nTimeBlockFrom=%d", nTimeTx, nTimeBlockFrom );

    if (IsBelowMinAge(output, nTimeBlockFrom, nTimeTx))
      return error("Stake() : min age violation - nTimeBlockFrom=%d nStakeMinAge=%d nTimeTx=%d",
            nTimeBlockFrom, Params().GetStakeMinAge(), nTimeTx);

    // grab difficulty
    uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);

    // grab stake modifier (V3: 256-bit)
    uint256 nStakeModifier;
    if (!stakeInput->GetModifier(nStakeModifier))
        return error("failed to get kernel stake modifier");

    bool fSuccess = false;
    unsigned int nTryTime = 0;
    int nHeightStart = chainActive.Height();
    int nHashDrift = Params().GetTargetSpacing() * 0.75;
    CDataStream ssUniqueID = stakeInput->GetUniqueness();

    for (int i = 0; i < nHashDrift; i++) //iterate the hashing
    {
        // new block came in, move on
        if (chainActive.Height() != nHeightStart)
            break;

        // hash this iteration
        nTryTime = nTimeTx + nHashDrift - i;

        // if stake hash does not meet the target then continue to next iteration
        if (!CheckStake(ssUniqueID, stakeableBalance, nStakeModifier, bnTargetPerCoinDay, nTimeBlockFrom, nTryTime))
            continue;

        fSuccess = true; // if we make it this far then we have successfully created a stake hash

        nTimeTx = nTryTime;
        break;
    }

    mapHashedBlocks.clear();
    mapHashedBlocks[chainActive.Tip()->nHeight] = GetTime(); // store a time stamp of when we last hashed on this block
    return fSuccess;
}

// Check kernel hash target and coinstake signature
bool CheckProofOfStake(const CBlock block, uint256& hashProofOfStake, std::list<CKoreStake>& listStake, CAmount& stakedBalance)
{
    const CTransaction tx = block.vtx[1];
    if (!tx.IsCoinStake())
        return error("CheckProofOfStake(): called on non-coinstake %s", tx.GetHash().ToString().c_str());

    
    uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(block.nBits);
    if (bnTargetPerCoinDay > Params().ProofOfStakeLimit())
        return error("%s(): Target is easier than limit %s", __func__, bnTargetPerCoinDay.ToString());
        
    CTransaction originTx;
    uint256 hashBlock = 0;
    if (!GetTransaction(block.vtx[1].vin[0].prevout.hash, originTx, hashBlock, true))
        return error("%s(): Origin tx (%s) not found for block %s. Possible reorg underway so we are skipping a few checks.", __func__, block.GetHash().ToString(), block.vtx[1].vin[0].prevout.hash.ToString());
    
    uint160 lockPubKeyID;
    if (!ExtractDestination(originTx.vout[block.vtx[1].vin[0].prevout.n].scriptPubKey, lockPubKeyID))
        return error("%s(): Couldn't get destination from script: %s", __func__, originTx.vout[block.vtx[1].vin[0].prevout.n].scriptPubKey.ToString());

    // Second transaction must lock coins from same pubkey as coinbase
    uint160 pubKeyID;
    if (!ExtractDestination(block.vtx[0].vout[1].scriptPubKey, pubKeyID))
        return error("%s(): Couldn't get destination from script: %s", __func__, block.vtx[0].vout[1].scriptPubKey.ToString());

    if (lockPubKeyID != pubKeyID)
        return error("%s(): locking pubkey different from coinbase pubkey", __func__);

    // There must be only one pubkey on the locking transaction
    for (unsigned int i = 1; i < block.vtx[1].vin.size(); i++) {
        CTransaction otherOriginTx;
        uint256 otherHashBlock;
        pubKeyID.SetNull();
        if (!GetTransaction(block.vtx[1].vin[i].prevout.hash, otherOriginTx, otherHashBlock, true))
            return error("%s(): Other origin tx (%s) not found for block %s. Possible reorg underway so we are skipping a few checks.", __func__, block.GetHash().ToString(), block.vtx[1].vin[i].prevout.hash.ToString());
        
        if (!ExtractDestination(otherOriginTx.vout[block.vtx[1].vin[i].prevout.n].scriptPubKey, pubKeyID))
            return error("%s(): Couldn't get destination from script: %s", __func__, otherOriginTx.vout[block.vtx[1].vin[i].prevout.n].scriptPubKey.ToString());

        if (lockPubKeyID != pubKeyID)
            return error("%s(): more than one pubkey on lock", __func__);
    }

    // All the outputs pubkeys must be the same as the locking pubkey
    for (unsigned int i = 0; i < block.vtx[1].vout.size(); i++) {
        pubKeyID.SetNull();
        if (!ExtractDestination(block.vtx[1].vout[i].scriptPubKey, pubKeyID))
            return error("%s(): Couldn't get destination from script: %s", __func__, block.vtx[1].vout[i].scriptPubKey.ToString());

        if (pubKeyID != lockPubKeyID)
            return error("%s(): more than one pubkey on lock tx", __func__);
    }

    CKoreStake kernel;
    stakedBalance = 0;
    for (int i = 0; i < tx.vin.size(); i++)
    {
        const CTxIn& txin = tx.vin[i];

        // First try finding the previous transaction in database
        uint256 hashBlock;
        CTransaction txPrev;
        if (!GetTransaction(txin.prevout.hash, txPrev, hashBlock, true))
            return error("CheckProofOfStake(): INFO: read txPrev failed");

        // verify signature and script
        if (!VerifyScript(txin.scriptSig, txPrev.vout[txin.prevout.n].scriptPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&tx, i)))
            return error("CheckProofOfStake(): VerifySignature failed on coinstake %s", tx.GetHash().ToString().c_str());

        // Construct the stakeinput object
        CKoreStake koreInput;
        koreInput.SetInput(txPrev, txin.prevout.n);
        CBlockIndex* pindex = koreInput.GetIndexFrom();
        if (!pindex)
            return error("%s: Failed to find the block index", __func__);

        stakedBalance += koreInput.GetValue();

        listStake.emplace_back(koreInput);

        if (i == 0)
            kernel = koreInput;
    }

    CBlockIndex* pindex = kernel.GetIndexFrom();

    CTransaction kernelTxFrom;
    kernel.GetTxFrom(kernelTxFrom);
    if (kernelTxFrom.IsNull())
        return error("%s failed to get parent tx for stake input\n", __func__);

    // V3: Use 256-bit stake modifier
    uint256 nStakeModifier;
    if (!kernel.GetModifier(nStakeModifier))
        return error("%s failed to get modifier for stake input\n", __func__);

    unsigned int nTxTime = block.nTime;
    if (!CheckStake(kernel.GetUniqueness(), stakedBalance, nStakeModifier, bnTargetPerCoinDay, kernelTxFrom.nTime, nTxTime)) {
        return error("CheckProofOfStake(): INFO: check kernel failed on coinstake %s \n", tx.GetHash().GetHex());
    }

    return true;
}

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx)
{
    // v0.3 protocol
    return (nTimeBlock == nTimeTx);
}

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex)
{
    assert(pindex->pprev || pindex->GetBlockHash() == Params().HashGenesisBlock());
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (pindex->pprev)
        ss << pindex->pprev->nStakeModifierChecksum;
    ss << pindex->nFlags << pindex->hashProofOfStake << pindex->nStakeModifier;
    uint256 hashChecksum = Hash(ss.begin(), ss.end());
    hashChecksum >>= (256 - 32);
    return hashChecksum.Get64();
}

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum)
{
    if (Params().GetNetworkID() == CBaseChainParams::TESTNET) return true; // Testnet has no checkpoints
    if (mapStakeModifierCheckpoints.count(nHeight)) {
        return nStakeModifierChecksum == mapStakeModifierCheckpoints[nHeight];
    }
    return true;
}
