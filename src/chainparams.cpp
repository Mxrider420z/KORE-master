// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The KORE developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "amount.h"
#include "arith_uint256.h"
#include "kernel.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"
#include <assert.h>
#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBirthdayA, uint32_t nBirthdayB, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.nTime = nTime;
    txNew.nVersion = 1;

    if (pszTimestamp != NULL)
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    else
        txNew.vin[0].scriptSig = CScript() << 0 << OP_0;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime        = nTime;
    genesis.nBits        = nBits;
    genesis.nNonce       = nNonce;
    genesis.nBirthdayA   = nBirthdayA;
    genesis.nBirthdayB   = nBirthdayB;
    genesis.nVersion     = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = genesis.BuildMerkleTree();
    return genesis;
}

static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        pchMessageStart[0] = 0xe4;
        pchMessageStart[1] = 0x7b;
        pchMessageStart[2] = 0xb3;
        pchMessageStart[3] = 0x4a;
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 45);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 85);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 128);
        bnProofOfStakeLimit            = ~uint256(0) >> 16;
        bnProofOfWorkLimit             = ~uint256(0) >> 3;
        fDefaultConsistencyChecks      = false;
        fEnableBigReward               = false;
        fMineBlocksOnDemand            = false;
        fMiningRequiresPeers           = true;
        fRequireStandard               = true;
        fSkipProofOfWorkCheck          = false;
        nDefaultPort                   = 15743;
        nCoinMaturity                  = 25;
        nMaxMoneyOut                   = MAX_MONEY;
        nMaxReorganizationDepth        = 25;
        nMaxTipAge                     = 2 * 60 * 60;
        nMinerConfirmationWindow       = 50;
        nMinerThreads                  = 0;
        nPastBlocksMax                 = 128;
        nPastBlocksMin                 = 24;
        nPoolMaxTransactions           = 3;
        nPruneAfterHeight              = 100000;
        nRuleChangeActivationThreshold = 1916;
        nStakeLockInterval             = 2 * 60 * 60;
        nStakeMinAge                   = 2 * 60 * 60;
        nTargetSpacing                 = 1 * 60;
        nHeightToFork                  = 483063;
        nLastPOWBlock                  = 1000;
        strDevFundPubKey               = "02f6a9ccc5a81718a9abe524975cd60a73930ad047ba9d597b747f545e2fbafd9e";
        vAlertPubKey                   = ParseHex("042b0fb78026380244cc458a914dae461899b121f53bc42105d134158b9773e3fdadca67ca3015dc9c4ef9b9df91f2ef05b890a15cd2d2b85930d37376b2196002");
        vDeployments[DEPLOYMENT_CSV].bit        = 0;
        vDeployments[DEPLOYMENT_CSV].nStartTime = 1462060800;
        vDeployments[DEPLOYMENT_CSV].nTimeout   = 1493596800;

        CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis = CreateGenesisBlock(NULL, genesisOutputScript, 1508884606, 22, 12624920, 58284520, 0x201fffff, 1, pow (7,2) * COIN);

        nHashGenesisBlock = genesis.GetHash();
        assert(nHashGenesisBlock == uint256("0x0aab10677b4fe0371a67f99e78a69e7d9fa03a1c7d48747978da405dc5abeb99"));
        assert(genesis.hashMerkleRoot == uint256S("0x53e2105c87e985ab3a3a3b3c6921f660f18535f935e447760758d4ed7c4c748c"));

        vSeeds.push_back(CDNSSeedData("seed1", "puclxktvdiujyqb75bjs4n4cuhvh4eiwoptbsl5nvflifrdp3wia3vyd.onion"));
        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        checkpointData.mapCheckpoints.clear();
        checkpointData.mapCheckpoints[0] = nHashGenesisBlock;
        checkpointData.mapCheckpoints[5] = uint256S("0x00eaaa465402e6bcf745c00c38c0033a26e4dea19448d9109e4555943d677a31");

        chainTxData.nTime    = 1560981536;
        chainTxData.nTxCount = 1069012;
        chainTxData.dTxRate  = 0.02053413884846783;
    }
};
static CMainParams mainParams;

class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID    = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0x18;
        pchMessageStart[1] = 0x15;
        pchMessageStart[2] = 0x14;
        pchMessageStart[3] = 0x88;
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 105);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 190);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 233);
        fEnableBigReward               = true;
        nDefaultPort                   = 21743;
        nHeightToFork                  = 101;
        nLastPOWBlock                  = 100;
        vAlertPubKey                   = ParseHex("04cd7ce93858b4257079f4ed9150699bd9f66437ff76617690d1cc180321e94ea391bbccf3bccdcf2edaf0429e32c07b53354e9cecf458cca3fe71dc277f11d9c5");
        strDevFundPubKey               = "04fb16faf70501f5292a630bced3ec5ff4df277d637e855d129896066854e1d2c9d7cab8dbd5b98107594e74a005e127c66c13a918be477fd3827b872b33d25e03";
        vDeployments[DEPLOYMENT_CSV].bit        = 0;
        vDeployments[DEPLOYMENT_CSV].nStartTime = 0;
        vDeployments[DEPLOYMENT_CSV].nTimeout   = 0;
        nTargetSpacing                          = 1 * 57;

        CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        
        // LOCKED GENESIS (Dec 22, 2025)
        // Time: 1766436786, Nonce: 82026
        genesis = CreateGenesisBlock(NULL, genesisOutputScript, 1766436786, 82026, 2500634, 64441706, 0x1e0ffff0, 1, 49 * COIN);

        nHashGenesisBlock = genesis.GetHash();
        assert(nHashGenesisBlock == uint256S("0x00000eef716955bf46b1fe8bb16f3fa8fff39eed2271e9d62b446626f215917a"));
        assert(genesis.hashMerkleRoot == uint256S("0xf17f603406f28cad494ccc4c03f1832abe2df5d0cd3c766f305fc9ad84139cba"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("seed1", "puclxktvdiujyqb75bjs4n4cuhvh4eiwoptbsl5nvflifrdp3wia3vyd.onion"));
        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        checkpointData.mapCheckpoints.clear();
        checkpointData.mapCheckpoints[0] = nHashGenesisBlock;

        chainTxData.nTime    = 1766436786;
        chainTxData.nTxCount = 0;
        chainTxData.dTxRate  = 0.0;
    }
};
static CTestNetParams testNetParams;

class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID    = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xcf;
        pchMessageStart[1] = 0x05;
        pchMessageStart[2] = 0x6a;
        pchMessageStart[3] = 0xe1;
        genesis.nTime      = 1453993470;
        genesis.nBits      = 0x207fffff;
        genesis.nNonce     = 12345;
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 105);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 190);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 233);
        bnProofOfWorkLimit             = ~uint256(0) >> 1;
        fDefaultConsistencyChecks      = true;
        fMineBlocksOnDemand            = true;
        fMiningRequiresPeers           = true;
        fRequireStandard               = false;
        nDefaultPort                   = 18444;
        nHashGenesisBlock              = genesis.GetHash();
        nHeightToFork                  = 900000;
        nMinerThreads                  = 1;
        nTargetSpacing                 = 1 * 60;
        vFixedSeeds.clear();
        vSeeds.clear();
        
        checkpointData.mapCheckpoints.clear();
        checkpointData.mapCheckpoints[0] = uint256S("0x001");

        chainTxData.nTime    = 1454124731;
        chainTxData.nTxCount = 0;
        chainTxData.dTxRate  = 100;
    }
};
static CRegTestParams regTestParams;

class CUnitTestParams : public CMainParams, public CModifiableParams
{
public:
    CUnitTestParams()
    {
        networkID    = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        fDefaultConsistencyChecks = true;
        fMineBlocksOnDemand       = true;
        fMiningRequiresPeers      = false;
        fSkipProofOfWorkCheck     = true;
        nDefaultPort              = 51478;
        nTargetSpacing            = 10;
        nCoinMaturity             = nTargetSpacing + 1;
        nPastBlocksMax            = 128;
        nPastBlocksMin            = 32;
        nStakeLockInterval        = 32;
        nStakeMinAge              = 5;
        vFixedSeeds.clear();
        vSeeds.clear();
    }
    virtual void setCoinMaturity(int aCoinMaturity) { nCoinMaturity = aCoinMaturity; }
    virtual void setEnableBigRewards(bool afBigRewards) { fEnableBigReward = afBigRewards; };
    virtual void setHeightToFork(int aHeightToFork) { nHeightToFork = aHeightToFork; };
    virtual void setLastPowBlock(int aLastPOWBlock) { nLastPOWBlock = aLastPOWBlock; };
    virtual void setStakeLockInterval(int aStakeLockInterval) { nStakeLockInterval = aStakeLockInterval; };
    virtual void setStakeMinAge(int aStakeMinAge) { nStakeMinAge = aStakeMinAge; };
    virtual void setTargetSpacing(uint32_t aTargetSpacing) { nTargetSpacing = aTargetSpacing; };
};
static CUnitTestParams unitTestParams;

static CChainParams* pCurrentParams = 0;

CModifiableParams* ModifiableParams()
{
    assert(pCurrentParams);
    assert(pCurrentParams == &unitTestParams);
    return (CModifiableParams*)&unitTestParams;
}

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    case CBaseChainParams::UNITTEST:
        return unitTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;
    SelectParams(network);
    return true;
}
