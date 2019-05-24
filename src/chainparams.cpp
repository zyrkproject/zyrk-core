// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Zyrk Project developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include <assert.h>
#include "arith_uint256.h"
#include <utilmoneystr.h>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << nBits << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "Bitcoin’s Price Closes In on First 3-Month Win Streak Since 2017";
    const CScript genesisOutputScript = CScript();
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

CAmount CChainParams::SubsidyValue(DynamicRewardSystem::key_type level, uint32_t nTime) const
{
    const auto& points = dynamicRewardSystem;

    DynamicRewardSystem::const_iterator point = points.upper_bound(level);

    if(point != dynamicRewardSystem.begin())
        point = std::prev(point);

    return point->second;
}

int64_t CChainParams::GetCoinYearReward(int64_t nTime) const
{
    if (strNetworkID == "main")
    {
        return 0 * CENT;
    }
    return nCoinYearReward;
}

int64_t CChainParams::GetProofOfStakeReward(const CBlockIndex *pindexPrev, int64_t nFees, bool allowInitial) const
{
    int64_t nSubsidy;
    nSubsidy = (pindexPrev->nMoneySupply / COIN) * (0 * CENT) / (365 * 24 * (60 * 60 / nTargetSpacing));
    return nSubsidy + nFees;
}


class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 556080;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0x00000d5449d5330a0c3083572a89e6b93bce65c6f0c4db9e5a70a917c914cded");
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.powLimit = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 2 * 60; 
        consensus.nPowTargetTimespan = consensus.nPowTargetSpacing;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1; 
        consensus.nMinerConfirmationWindow = 2;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1475020800; 
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1557057600; 
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1557057600; 
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1557057600; 

        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000082022924bba4e9");
        consensus.defaultAssumeValid = uint256S("0x00000000001bff2ac23b48189a8cbae78463562a0696a390c012233f8ff2ab63");

        consensus.nCoinMaturityReductionHeight = 9999999;
        consensus.nStartAnonymizeFeeDistribution = 1500000;
        consensus.nAnonymizeFeeDistributionCycle = 720;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMasternodePaymentsStartBlock = 1500; 
        consensus.nMasternodeInitialize = 1499;
        consensus.nPosTimeActivation = 9999999999;
        consensus.nPosHeightActivate = 1525600;
        nModifierInterval = 10 * 60;    
        nTargetSpacing = 2* 60;
        nTargetTimespan = 24 * 60;

        nMaxTipAge = 30 * 60 * 60;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60 * 60;
        strSporkPubKey = "04725af19a3748775a02c93acc190fa7ba3bd9a8b6f510c63fb58b98936165cdf6f345c22d0d9729698b09eb36effd0cb7e75cd4b99219c081aad22696ce5674e1";
        strMasternodePaymentsPubKey = "04725af19a3748775a02c93acc190fa7ba3bd9a8b6f510c63fb58b98936165cdf6f345c22d0d9729698b09eb36effd0cb7e75cd4b99219c081aad22696ce5674e1";

        dynamicRewardSystem = {
            {0          ,   25 * COIN},
            {5     * 1e9,   28 * COIN},
            {15    * 1e9,   35 * COIN},
            {25    * 1e9,   42 * COIN},
            {50    * 1e9,   50 * COIN},
            {100   * 1e9,   65 * COIN},
            {500   * 1e9,   80 * COIN},
            {1000  * 1e9,   95 * COIN},
            {5000  * 1e9,  110 * COIN},
            {20000 * 1e9,  125 * COIN},
        };
        assert(dynamicRewardSystem.size());

        pchMessageStart[0] = 0xe3;
        pchMessageStart[1] = 0xa3;
        pchMessageStart[2] = 0xa1;
        pchMessageStart[3] = 0xca;
        nDefaultPort = 19655;
        nBIP44ID = 0x800003cf;
        nPruneAfterHeight = 0;

        genesis = CreateGenesisBlock(1556927200, 1233327, 0x1e0ffff0, 1, 25 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000d8a24f1303b9d29566987e31c6ec289caa102657dda3a0ce6c4e4993035"));
        assert(genesis.hashMerkleRoot == uint256S("0xb6be95542e135a34f82818f5cc3a2376e75d0b143ea8899365ff8fd5176f0cfa"));

        vSeeds.emplace_back("peers.zyrk.io");
        vSeeds.emplace_back("seed.zyrk.io");
        vSeeds.emplace_back("eu.zyrk.io");
        vSeeds.emplace_back("ru.zyrk.io");
        vSeeds.emplace_back("oce.zyrk.io");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,75);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,80);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[PUBKEY_ADDRESS_256] = std::vector<unsigned char>(1,57);
        base58Prefixes[SCRIPT_ADDRESS_256] = {0x3d};
        base58Prefixes[STEALTH_ADDRESS]    = {0x4b}; 
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04, 0x88, 0xAD, 0xE4};
        base58Prefixes[EXT_KEY_HASH]       = {0x4b};
        base58Prefixes[EXT_ACC_HASH]       = {0x17};
        base58Prefixes[EXT_PUBLIC_KEY_BTC] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY_BTC] = {0x04, 0x88, 0xAD, 0xE4};

        bech32Prefixes[PUBKEY_ADDRESS].assign       ("zh","zh"+2);
        bech32Prefixes[SCRIPT_ADDRESS].assign       ("zr","zr"+2);
        bech32Prefixes[PUBKEY_ADDRESS_256].assign   ("zl","zl"+2);
        bech32Prefixes[SCRIPT_ADDRESS_256].assign   ("zj","zj"+2);
        bech32Prefixes[SECRET_KEY].assign           ("zx","zx"+2);
        bech32Prefixes[EXT_PUBLIC_KEY].assign       ("zen","zen"+3);
        bech32Prefixes[EXT_SECRET_KEY].assign       ("zex","zex"+3);
        bech32Prefixes[STEALTH_ADDRESS].assign      ("zg","zg"+2);
        bech32Prefixes[EXT_KEY_HASH].assign         ("zek","zek"+3);
        bech32Prefixes[EXT_ACC_HASH].assign         ("zea","zea"+3);

        bech32_hrp = "zyrk";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                { 0, uint256S("0x00000d8a24f1303b9d29566987e31c6ec289caa102657dda3a0ce6c4e4993035")},
                { 7225, uint256S("0x00000000001bff2ac23b48189a8cbae78463562a0696a390c012233f8ff2ab63")},
                { 13250, uint256S("0x000000000036708ff901cbcdb4c16b37ddb219acbbb50f63841f4ea573c45dcd")},
            }
        };

        chainTxData = ChainTxData{
            1558683596,
            15955,
            0.1 
        };
    }
};

static CMainParams mainParams;

class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 262080;
        consensus.BIP16Height = 0; 
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0x");
        consensus.BIP65Height = 0; 
        consensus.BIP66Height = 0;
        consensus.powLimit = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 60;
        consensus.nPowTargetSpacing = 120;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1; 
        consensus.nMinerConfirmationWindow = 2; 

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1475020800;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1557057600;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1557057600;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000; 
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1557057600; 

        consensus.nMinimumChainWork = uint256S("0x0");

        consensus.nMasternodeMinimumConfirmations = 5;
        consensus.nMasternodePaymentsStartBlock = 5000;
        consensus.nMasternodeInitialize = 1080;
        consensus.nPosTimeActivation = 9999999999; 
        consensus.nPosHeightActivate = 500000;
        consensus.nCoinMaturityReductionHeight = 999999;
        consensus.nStartAnonymizeFeeDistribution = 150000;
        consensus.nAnonymizeFeeDistributionCycle = 720;
        nModifierInterval = 10 * 60; 
        nTargetSpacing = 2 * 60;           
        nTargetTimespan = 24 * 60; 

        nMaxTipAge = 0x7fffffff;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5*60; 
        strSporkPubKey = "04725af19a3748775a02c93acc190fa7ba3bd9a8b6f510c63fb58b98936165cdf6f345c22d0d9729698b09eb36effd0cb7e75cd4b99219c081aad22696ce5674e1";
        strMasternodePaymentsPubKey = "04725af19a3748775a02c93acc190fa7ba3bd9a8b6f510c63fb58b98936165cdf6f345c22d0d9729698b09eb36effd0cb7e75cd4b99219c081aad22696ce5674e1";

        dynamicRewardSystem = {
            {0          ,   25 * COIN},
            {5     * 1e9,   28 * COIN},
            {15    * 1e9,   35 * COIN},
            {25    * 1e9,   42 * COIN},
            {50    * 1e9,   50 * COIN},
            {100   * 1e9,   65 * COIN},
            {500   * 1e9,   80 * COIN},
            {1000  * 1e9,   95 * COIN},
            {5000  * 1e9,  110 * COIN},
            {20000 * 1e9,  125 * COIN},
        };
        assert(dynamicRewardSystem.size());

        pchMessageStart[0] = 0xca;
        pchMessageStart[1] = 0xb7;
        pchMessageStart[2] = 0xe1;
        pchMessageStart[3] = 0xa3;
        nDefaultPort = 29655;
        nBIP44ID = 0x80000001;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1556927201, 2475818, 0x1e0ffff0, 1, 25 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0000034659940876fc9d81473a19af022bd93ab188f7e0e642e7ff66e28c2feb"));
        assert(genesis.hashMerkleRoot == uint256S("0xb6be95542e135a34f82818f5cc3a2376e75d0b143ea8899365ff8fd5176f0cfa"));

        vFixedSeeds.clear();
        vSeeds.clear();

        vSeeds.emplace_back("testnet.zyrk.io");
        vSeeds.emplace_back("testnet-zyop.zyrk.io");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,1);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,3);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[PUBKEY_ADDRESS_256] = std::vector<unsigned char>(1,57);
        base58Prefixes[SCRIPT_ADDRESS_256] = {0x3d};
        base58Prefixes[STEALTH_ADDRESS]    = {0x0c};
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04, 0x88, 0xAD, 0xE4};
        base58Prefixes[EXT_KEY_HASH]       = {0x4b};
        base58Prefixes[EXT_ACC_HASH]       = {0x17};
        base58Prefixes[EXT_PUBLIC_KEY_BTC] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY_BTC] = {0x04, 0x88, 0xAD, 0xE4};

        bech32Prefixes[PUBKEY_ADDRESS].assign       ("th","th"+2);
        bech32Prefixes[SCRIPT_ADDRESS].assign       ("tr","tr"+2);
        bech32Prefixes[PUBKEY_ADDRESS_256].assign   ("tl","tl"+2);
        bech32Prefixes[SCRIPT_ADDRESS_256].assign   ("tj","tj"+2);
        bech32Prefixes[SECRET_KEY].assign           ("tx","tx"+2);
        bech32Prefixes[EXT_PUBLIC_KEY].assign       ("ten","ten"+3);
        bech32Prefixes[EXT_SECRET_KEY].assign       ("tex","tex"+3);
        bech32Prefixes[STEALTH_ADDRESS].assign      ("tg","tg"+2);
        bech32Prefixes[EXT_KEY_HASH].assign         ("tek","tek"+3);
        bech32Prefixes[EXT_ACC_HASH].assign         ("tea","tea"+3);

        bech32_hrp = "tzyrk";

        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {0, uint256S("0x0000034659940876fc9d81473a19af022bd93ab188f7e0e642e7ff66e28c2feb")},
            }
        };

        chainTxData = ChainTxData{
            1556927201,
            1,
            1
        };

    }
};

static CTestNetParams testNetParams;

class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 262080;
        consensus.BIP16Height = 0; 
        consensus.BIP34Height = 1; 
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 0; 
        consensus.BIP66Height = 0; 
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; 
        consensus.nPowTargetSpacing = 1;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 1; 
        consensus.nMinerConfirmationWindow = 2;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.nMasternodePaymentsStartBlock = 720;
        consensus.nMasternodeInitialize = 600;
        consensus.nPosTimeActivation = 9999999999; 
        consensus.nPosHeightActivate = 500;
        nModifierInterval = 10 * 60;    
        nTargetSpacing = 2 * 60;           
        nTargetTimespan = 24 * 60;  

        nMaxTipAge = 30 * 60 * 60; 

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60*60; 
        strSporkPubKey = "04725af19a3748775a02c93acc190fa7ba3bd9a8b6f510c63fb58b98936165cdf6f345c22d0d9729698b09eb36effd0cb7e75cd4b99219c081aad22696ce5674e1";
        strMasternodePaymentsPubKey = "04725af19a3748775a02c93acc190fa7ba3bd9a8b6f510c63fb58b98936165cdf6f345c22d0d9729698b09eb36effd0cb7e75cd4b99219c081aad22696ce5674e1";

        consensus.nMinimumChainWork = uint256S("0x");
        consensus.defaultAssumeValid = uint256S("0x");

        dynamicRewardSystem = {
            {0          ,   25 * COIN},
            {5     * 1e9,   28 * COIN},
            {15    * 1e9,   35 * COIN},
            {25    * 1e9,   42 * COIN},
            {50    * 1e9,   50 * COIN},
            {100   * 1e9,   65 * COIN},
            {500   * 1e9,   80 * COIN},
            {1000  * 1e9,   95 * COIN},
            {5000  * 1e9,  110 * COIN},
            {20000 * 1e9,  125 * COIN},
        };
        assert(dynamicRewardSystem.size());

        pchMessageStart[0] = 0xba;
        pchMessageStart[1] = 0xab;
        pchMessageStart[2] = 0xda;
        pchMessageStart[3] = 0xca;
        nDefaultPort = 39655;
        nBIP44ID = 0x80000001;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1556927202, 2596214, 0x1e0ffff0, 1, 25 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000415acc96306e7962fd378c9bc2c030b4d7826e44d9b0cea8dd7de6e1a14"));
        assert(genesis.hashMerkleRoot == uint256S("0xb6be95542e135a34f82818f5cc3a2376e75d0b143ea8899365ff8fd5176f0cfa"));

        vFixedSeeds.clear(); 
        vSeeds.clear();     

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                {0, uint256S("0x00000415acc96306e7962fd378c9bc2c030b4d7826e44d9b0cea8dd7de6e1a14")},
            }
        };

        chainTxData = ChainTxData{
            1556927202,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,3);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,53);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[PUBKEY_ADDRESS_256] = std::vector<unsigned char>(1,57);
        base58Prefixes[SCRIPT_ADDRESS_256] = {0x3d};
        base58Prefixes[STEALTH_ADDRESS]    = {0x0c};
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04, 0x88, 0xAD, 0xE4};
        base58Prefixes[EXT_KEY_HASH]       = {0x4b};
        base58Prefixes[EXT_ACC_HASH]       = {0x17};
        base58Prefixes[EXT_PUBLIC_KEY_BTC] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY_BTC] = {0x04, 0x88, 0xAD, 0xE4};

        bech32Prefixes[PUBKEY_ADDRESS].assign       ("rh","rh"+2);
        bech32Prefixes[SCRIPT_ADDRESS].assign       ("rr","rr"+2);
        bech32Prefixes[PUBKEY_ADDRESS_256].assign   ("rl","rl"+2);
        bech32Prefixes[SCRIPT_ADDRESS_256].assign   ("rj","rj"+2);
        bech32Prefixes[SECRET_KEY].assign           ("rx","rx"+2);
        bech32Prefixes[EXT_PUBLIC_KEY].assign       ("ren","ren"+3);
        bech32Prefixes[EXT_SECRET_KEY].assign       ("rex","rex"+3);
        bech32Prefixes[STEALTH_ADDRESS].assign      ("rg","rg"+2);
        bech32Prefixes[EXT_KEY_HASH].assign         ("rek","rek"+3);
        bech32Prefixes[EXT_ACC_HASH].assign         ("rea","rea"+3);

        bech32_hrp = "rzyrk";
    }
};

static CRegTestParams regTestParams;

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

CChainParams &Params(const std::string &chain) {
    if (chain == CBaseChainParams::MAIN)
        return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
        return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
        return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

const CChainParams *pParams() {
    return globalChainParams.get();
};
std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}

bool CChainParams::IsBech32Prefix(const std::vector<unsigned char> &vchPrefixIn) const
{
    for (auto &hrp : bech32Prefixes)
    {
        if (vchPrefixIn == hrp)
            return true;
    };

    return false;
};

bool CChainParams::IsBech32Prefix(const std::vector<unsigned char> &vchPrefixIn, CChainParams::Base58Type &rtype) const
{
    for (size_t k = 0; k < MAX_BASE58_TYPES; ++k)
    {
        auto &hrp = bech32Prefixes[k];
        if (vchPrefixIn == hrp)
        {
            rtype = static_cast<CChainParams::Base58Type>(k);
            return true;
        };
    };

    return false;
};

bool CChainParams::IsBech32Prefix(const char *ps, size_t slen, CChainParams::Base58Type &rtype) const
{
    for (size_t k = 0; k < MAX_BASE58_TYPES; ++k)
    {
        auto &hrp = bech32Prefixes[k];
        size_t hrplen = hrp.size();
        if (hrplen > 0
            && slen > hrplen
            && strncmp(ps, (const char*)&hrp[0], hrplen) == 0)
        {
            rtype = static_cast<CChainParams::Base58Type>(k);
            return true;
        };
    };

    return false;
};
