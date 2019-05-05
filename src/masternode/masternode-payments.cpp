// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2019 The Zyrk Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "darksend.h"
#include "masternode-payments.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"
#include "netmessagemaker.h"
#include "chainparams.h"

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CMasternodePayments mnpayments;

CCriticalSection cs_vecPayees;
CCriticalSection cs_mapMasternodeBlocks;
CCriticalSection cs_mapMasternodePaymentVotes;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In Dash some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock &block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet) {
    strErrorRet = "";
    bool isBlockRewardValueMet = block.vtx[0]->IsCoinStake() ? true :  (block.vtx[0]->GetValueOut() <= blockReward);
    //LogPrintf("IsBlockValueValid(): value-out=%llf, block-reward=%llf \n", block.vtx[0]->GetValueOut(), blockReward);
    //if (fDebug) //LogPrint("block.vtx[0].GetValueOut() %lld <= blockReward %lld\n", block.vtx[0]->GetValueOut(), blockReward);


    if (!masternodeSync.IsSynced(chainActive.Height())) {

        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
                                    nBlockHeight, block.vtx[0]->GetValueOut(), blockReward);
        }
        // it MUST be a regular block otherwise
        return isBlockRewardValueMet;
    }

    // we are synced, let's try to check as much data as we can

    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
        //LogPrint("gobject", "IsBlockValueValid -- Superblocks are disabled, no superblocks allowed\n");
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
                                    nBlockHeight, block.vtx[0]->GetValueOut(), blockReward);
        }
    }

    // it MUST be a regular block
    return isBlockRewardValueMet;
}

bool IsBlockPayeeValid(const CTransaction &txNew, int nBlockHeight, CAmount blockReward) {
    // we can only check masternode payment /
    const Consensus::Params &consensusParams = Params().GetConsensus();

    if (nBlockHeight < consensusParams.nMasternodePaymentsStartBlock) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        //if (fDebug) //LogPrint("IsBlockPayeeValid -- masternode isn't start\n");
        return true;
    }
    if (!masternodeSync.IsSynced(chainActive.Height())) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        //if (fDebug) //LogPrint("IsBlockPayeeValid -- WARNING: Client not synced, skipping block payee checks\n");
        return true;
    }

    //check for masternode payee
    if (mnpayments.IsTransactionValid(txNew, nBlockHeight)) {
        //LogPrint("mnpayments", "IsBlockPayeeValid -- Valid masternode payment at height %d: %s", nBlockHeight, txNew.ToString());
        return true;
    } else {
        if(sporkManager.IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT)){
            return false;
        } else {
            //LogPrint("MasterNode payment enforcement is disabled, accepting block\n");
            return true;
        }
    }
}

void FillBlockPayments(CMutableTransaction &txNew, int nBlockHeight, CAmount masternodePayment, CTxOut &txoutMasternodeRet, std::vector <CTxOut> &voutSuperblockRet) {

    // FILL BLOCK PAYEE WITH MASTERNODE PAYMENT OTHERWISE
    mnpayments.FillBlockPayee(txNew, nBlockHeight, masternodePayment, txoutMasternodeRet);
    //LogPrint("mnpayments", "FillBlockPayments -- nBlockHeight %d masternodePayment %lld txoutMasternodeRet %s txNew %s",
             //nBlockHeight, masternodePayment, txoutMasternodeRet.ToString(), txNew.ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight) {
    // IF WE HAVE A ACTIVATED TRIGGER FOR THIS HEIGHT - IT IS A SUPERBLOCK, GET THE REQUIRED PAYEES
//    if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//        return CSuperblockManager::GetRequiredPaymentsString(nBlockHeight);
//    }

    // OTHERWISE, PAY MASTERNODE
    return mnpayments.GetRequiredPaymentsString(nBlockHeight);
}

void CMasternodePayments::Clear() {
    LOCK2(cs_mapMasternodeBlocks, cs_mapMasternodePaymentVotes);
    mapMasternodeBlocks.clear();
    mapMasternodePaymentVotes.clear();
}

bool CMasternodePayments::CanVote(COutPoint outMasternode, int nBlockHeight) {
    LOCK(cs_mapMasternodePaymentVotes);

    if (mapMasternodesLastVote.count(outMasternode) && mapMasternodesLastVote[outMasternode] == nBlockHeight) {
        return false;
    }

    //record this masternode voted
    mapMasternodesLastVote[outMasternode] = nBlockHeight;
    return true;
}

std::string CMasternodePayee::ToString() const {
    CTxDestination address1;
    ExtractDestination(scriptPubKey, address1);
    CBitcoinAddress address2(address1);
    std::string str;
    str += "(address: ";
    str += address2.ToString();
    str += ")\n";
    return str;
}

/**
*   FillBlockPayee
*
*   Fill Masternode ONLY payment block
*/

void CMasternodePayments::FillBlockPayee(CMutableTransaction &txNew, int nBlockHeight, CAmount masternodePayment, CTxOut &txoutMasternodeRet) {
    // make sure it's not filled yet
    txoutMasternodeRet = CTxOut();

    CScript payee;
    bool foundMaxVotedPayee = true;

    if (!mnpayments.GetBlockPayee(nBlockHeight, payee)) {
        // no masternode detected...
        //LogPrint("No MasterNode detected...\n");
        foundMaxVotedPayee = false;
        int nCount = 0;
        CMasternode *winningNode = mnodeman.GetNextMasternodeInQueueForPayment(nBlockHeight, true, nCount);
        if (!winningNode) {
            // ...and we can't calculate it on our own
            //LogPrint("CMasternodePayments::FillBlockPayee -- Failed to detect masternode to pay\n");
            return;
        }
        // fill payee with locally calculated winner and hope for the best
        payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
        //LogPrint("payee=%s\n", winningNode->ToString());
    }
    txoutMasternodeRet = CTxOut(masternodePayment, payee);
    txNew.vout.push_back(txoutMasternodeRet);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);
    if (foundMaxVotedPayee) {
        //LogPrint("CMasternodePayments::FillBlockPayee::foundMaxVotedPayee -- Masternode payment %lld to %s\n", masternodePayment, address2.ToString());
    } else {
        //LogPrint("CMasternodePayments::FillBlockPayee -- Masternode payment %lld to %s\n", masternodePayment, address2.ToString());
    }

}

int CMasternodePayments::GetMinMasternodePaymentsProto() {
    if(chainActive.Height() > Params().GetConsensus().nStartAnonymizeFeeDistribution)
        return MIN_MASTERNODE_PAYMENT_PROTO_VERSION_2;

    return sporkManager.IsSporkActive(SPORK_10_MASTERNODE_PAY_UPDATED_NODES)
           ? MIN_MASTERNODE_PAYMENT_PROTO_VERSION_2
           : MIN_MASTERNODE_PAYMENT_PROTO_VERSION_1;
}

void CMasternodePayments::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {

    //LogPrintf("CMasternodePayments::ProcessMessage strCommand=%s\n", strCommand);
    // Ignore any payments messages until masternode list is synced
    if (!masternodeSync.IsMasternodeListSynced()) return;

    if (fLiteMode) return; // disable all Dash specific functionality

    if (strCommand == NetMsgType::MASTERNODEPAYMENTSYNC) { //Masternode Payments Request Sync

        // Ignore such requests until we are fully synced.
        // We could start processing this after masternode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!masternodeSync.IsSynced(chainActive.Height())) return;

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::MASTERNODEPAYMENTSYNC)) {
            // Asking for the payments list multiple times in a short period of time is no good
            //LogPrintf("MASTERNODEPAYMENTSYNC -- peer already asked me for the list\n");
            Misbehaving(pfrom->GetId(), 20);
            return;
        }
        netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::MASTERNODEPAYMENTSYNC);

        Sync(pfrom);
        //LogPrintf("mnpayments MASTERNODEPAYMENTSYNC -- Sent Masternode payment votes to peer \n");

    } else if (strCommand == NetMsgType::MASTERNODEPAYMENTVOTE) { // Masternode Payments Vote for the Winner

        CMasternodePaymentVote vote;
        vRecv >> vote;

        if (pfrom->nVersion < GetMinMasternodePaymentsProto()) return;

        if (!pCurrentBlockIndex) return;

        uint256 nHash = vote.GetHash();

        pfrom->setAskFor.erase(nHash);

        {
            LOCK(cs_mapMasternodePaymentVotes);
            if (mapMasternodePaymentVotes.count(nHash)) {
                //LogPrintf("mnpayments MASTERNODEPAYMENTVOTE -- nHeight=%d seen\n", pCurrentBlockIndex->nHeight);
                return;
            }

            // Avoid processing same vote multiple times
            mapMasternodePaymentVotes[nHash] = vote;
            // but first mark vote as non-verified,
            // AddPaymentVote() below should take care of it if vote is actually ok
            mapMasternodePaymentVotes[nHash].MarkAsNotVerified();
        }

        int nFirstBlock = pCurrentBlockIndex->nHeight - GetStorageLimit();
        if (vote.nBlockHeight < nFirstBlock || vote.nBlockHeight > pCurrentBlockIndex->nHeight + 20) {
            //LogPrintf("mnpaymentsMASTERNODEPAYMENTVOTE -- vote out of range: nFirstBlock=%d, nBlockHeight=%d, nHeight=%d\n", nFirstBlock, vote.nBlockHeight, pCurrentBlockIndex->nHeight);
            return;
        }

        std::string strError = "";
        if (!vote.IsValid(pfrom, pCurrentBlockIndex->nHeight, strError)) {
            //LogPrintf("mnpayments MASTERNODEPAYMENTVOTE -- invalid message, error: %s\n", strError);
            return;
        }

        if (!CanVote(vote.vinMasternode.prevout, vote.nBlockHeight)) {
            //LogPrintf("MASTERNODEPAYMENTVOTE -- masternode already voted, masternode\n");
            return;
        }

        masternode_info_t mnInfo = mnodeman.GetMasternodeInfo(vote.vinMasternode);
        if (!mnInfo.fInfoValid) {
            // mn was not found, so we can't check vote, some info is probably missing
            //LogPrintf("MASTERNODEPAYMENTVOTE -- masternode is missing \n");
            mnodeman.AskForMN(pfrom, vote.vinMasternode);
            return;
        }

        int nDos = 0;
        if (!vote.CheckSignature(mnInfo.pubKeyMasternode, pCurrentBlockIndex->nHeight, nDos)) {
            if (nDos) {
                //LogPrintf("MASTERNODEPAYMENTVOTE -- ERROR: invalid signature\n");
                Misbehaving(pfrom->GetId(), nDos);
            } else {
                // only warn about anything non-critical (i.e. nDos == 0) in debug mode
                //LogPrintf("mnpayments MASTERNODEPAYMENTVOTE -- WARNING: invalid signature\n");
            }
            // Either our info or vote info could be outdated.
            // In case our info is outdated, ask for an update,
            mnodeman.AskForMN(pfrom, vote.vinMasternode);
            // but there is nothing we can do if vote info itself is outdated
            // (i.e. it was signed by a mn which changed its key),
            // so just quit here.
            return;
        }

        CTxDestination address1;
        ExtractDestination(vote.payee, address1);
        CBitcoinAddress address2(address1);

        //LogPrintf("mnpayments MASTERNODEPAYMENTVOTE -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), vote.nBlockHeight, pCurrentBlockIndex->nHeight, vote.vinMasternode.prevout.ToStringShort());

        if (AddPaymentVote(vote)) {
            vote.Relay();
            masternodeSync.AddedPaymentVote();
        }
    }
}

bool CMasternodePaymentVote::Sign() {
    std::string strError;
    std::string strMessage = vinMasternode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, activeMasternode.keyMasternode)) {
        //LogPrint("CMasternodePaymentVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(activeMasternode.pubKeyMasternode, vchSig, strMessage, strError)) {
        //LogPrint("CMasternodePaymentVote::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CMasternodePayments::GetBlockPayee(int nBlockHeight, CScript &payee) {
    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].GetBestPayee(payee);
    }

    return false;
}

// Is this masternode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CMasternodePayments::IsScheduled(CMasternode &mn, int nNotBlockHeight) {
    LOCK(cs_mapMasternodeBlocks);

    if (!pCurrentBlockIndex) return false;

    CScript mnpayee;
    mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = pCurrentBlockIndex->nHeight; h <= pCurrentBlockIndex->nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapMasternodeBlocks.count(h) && mapMasternodeBlocks[h].GetBestPayee(payee) && mnpayee == payee) {
            return true;
        }
    }

    return false;
}

bool CMasternodePayments::AddPaymentVote(const CMasternodePaymentVote &vote) {
    //LogPrintf("\nmasternode-payments CMasternodePayments::AddPaymentVote\n");
    uint256 blockHash = uint256();
    if (!GetBlockHash(blockHash, vote.nBlockHeight - 100)){
        LogPrintf("\nmasternode-payments CMasternodePayments::Invalid Hash\n");
        return false;
    }
    if (HasVerifiedPaymentVote(vote.GetHash())) return false;

    LOCK2(cs_mapMasternodeBlocks, cs_mapMasternodePaymentVotes);

    mapMasternodePaymentVotes[vote.GetHash()] = vote;

    if (!mapMasternodeBlocks.count(vote.nBlockHeight)) {
        CMasternodeBlockPayees blockPayees(vote.nBlockHeight);
        mapMasternodeBlocks[vote.nBlockHeight] = blockPayees;
    }

    mapMasternodeBlocks[vote.nBlockHeight].AddPayee(vote);

    return true;
}

bool CMasternodePayments::HasVerifiedPaymentVote(uint256 hashIn) {
    LOCK(cs_mapMasternodePaymentVotes);
    std::map<uint256, CMasternodePaymentVote>::iterator it = mapMasternodePaymentVotes.find(hashIn);
    return it != mapMasternodePaymentVotes.end() && it->second.IsVerified();
}

void CMasternodeBlockPayees::AddPayee(const CMasternodePaymentVote &vote) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CMasternodePayee & payee, vecPayees)
    {
        if (payee.GetPayee() == vote.payee) {
            payee.AddVoteHash(vote.GetHash());
            return;
        }
    }
    CMasternodePayee payeeNew(vote.payee, vote.GetHash());
    vecPayees.push_back(payeeNew);
}

bool CMasternodeBlockPayees::GetBestPayee(CScript &payeeRet) {
    LOCK(cs_vecPayees);
    //LogPrint("mnpayments", "CMasternodeBlockPayees::GetBestPayee, vecPayees.size()=%s\n", vecPayees.size());
    if (!vecPayees.size()) {
        //LogPrint("mnpayments", "CMasternodeBlockPayees::GetBestPayee -- ERROR: couldn't find any payee\n");
        return false;
    }

    int nVotes = -1;
    BOOST_FOREACH(CMasternodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() > nVotes) {
            payeeRet = payee.GetPayee();
            nVotes = payee.GetVoteCount();
        }
    }

    return (nVotes > -1);
}

bool CMasternodeBlockPayees::HasPayeeWithVotes(CScript payeeIn, int nVotesReq) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CMasternodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nVotesReq && payee.GetPayee() == payeeIn) {
            return true;
        }
    }

//    //LogPrint("mnpayments", "CMasternodeBlockPayees::HasPayeeWithVotes -- ERROR: couldn't find any payee with %d+ votes\n", nVotesReq);
    return false;
}

bool CMasternodeBlockPayees::IsTransactionValid(const CTransaction &txNew) {
    LOCK(cs_vecPayees);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";

    CAmount nMasternodePayment = GetMasternodePayment(nBlockHeight, txNew.GetValueOut());

    //require at least MNPAYMENTS_SIGNATURES_REQUIRED signatures

    BOOST_FOREACH(CMasternodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nMaxSignatures) {
            nMaxSignatures = payee.GetVoteCount();
        }
    }

    // if we don't have at least MNPAYMENTS_SIGNATURES_REQUIRED signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    bool hasValidPayee = false;

    BOOST_FOREACH(CMasternodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            hasValidPayee = true;

            BOOST_FOREACH(CTxOut txout, txNew.vout) {
                if (payee.GetPayee() == txout.scriptPubKey && nMasternodePayment == txout.nValue) {
                    //LogPrint("mnpayments", "CMasternodeBlockPayees::IsTransactionValid -- Found required payment\n");
                    return true;
                }
            }

            CTxDestination address1;
            ExtractDestination(payee.GetPayee(), address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible = address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    if (!hasValidPayee) return true;

    return false;
}

std::string CMasternodeBlockPayees::GetRequiredPaymentsString() {
    LOCK(cs_vecPayees);

    std::string strRequiredPayments = "Unknown";

    BOOST_FOREACH(CMasternodePayee & payee, vecPayees)
    {
        CTxDestination address1;
        ExtractDestination(payee.GetPayee(), address1);
        CBitcoinAddress address2(address1);

        if (strRequiredPayments != "Unknown") {
            strRequiredPayments += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        } else {
            strRequiredPayments = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        }
    }

    return strRequiredPayments;
}

std::string CMasternodePayments::GetRequiredPaymentsString(int nBlockHeight) {
    LOCK(cs_mapMasternodeBlocks);

    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CMasternodePayments::IsTransactionValid(const CTransaction &txNew, int nBlockHeight) {
    LOCK(cs_mapMasternodeBlocks);

    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].IsTransactionValid(txNew);
    }

    return true;
}

void CMasternodePayments::CheckAndRemove() {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_mapMasternodeBlocks, cs_mapMasternodePaymentVotes);

    int nLimit = GetStorageLimit();

    std::map<uint256, CMasternodePaymentVote>::iterator it = mapMasternodePaymentVotes.begin();
    while (it != mapMasternodePaymentVotes.end()) {
        CMasternodePaymentVote vote = (*it).second;

        if (pCurrentBlockIndex->nHeight - vote.nBlockHeight > nLimit) {
            //LogPrint("mnpayments", "CMasternodePayments::CheckAndRemove -- Removing old Masternode payment: nBlockHeight=%d\n", vote.nBlockHeight);
            mapMasternodePaymentVotes.erase(it++);
            mapMasternodeBlocks.erase(vote.nBlockHeight);
        } else {
            ++it;
        }
    }
    //LogPrint("CMasternodePayments::CheckAndRemove -- %s\n", ToString());
}

bool CMasternodePaymentVote::IsValid(CNode *pnode, int nValidationHeight, std::string &strError) {
    CMasternode *pmn = mnodeman.Find(vinMasternode);

    if (!pmn) {
        strError = strprintf("Unknown Masternode: prevout=%s", vinMasternode.prevout.ToStringShort());
        // Only ask if we are already synced and still have no idea about that Masternode
        if (masternodeSync.IsMasternodeListSynced()) {
            mnodeman.AskForMN(pnode, vinMasternode);
        }

        return false;
    }

    int nMinRequiredProtocol;
    if (nBlockHeight >= nValidationHeight) {
        // new votes must comply SPORK_10_MASTERNODE_PAY_UPDATED_NODES rules
        nMinRequiredProtocol = mnpayments.GetMinMasternodePaymentsProto();
    } else {
        // allow non-updated masternodes for old blocks
        nMinRequiredProtocol = MIN_MASTERNODE_PAYMENT_PROTO_VERSION_1;
    }

    if (pmn->nProtocolVersion < nMinRequiredProtocol) {
        strError = strprintf("Masternode protocol is too old: nProtocolVersion=%d, nMinRequiredProtocol=%d", pmn->nProtocolVersion, nMinRequiredProtocol);
        return false;
    }

    // Only masternodes should try to check masternode rank for old votes - they need to pick the right winner for future blocks.
    // Regular clients (miners included) need to verify masternode rank for future block votes only.
    if (!fMasterNode && nBlockHeight < nValidationHeight) return true;

    int nRank = mnodeman.GetMasternodeRank(vinMasternode, nBlockHeight - 100, nMinRequiredProtocol, false);

    if (nRank == -1) {
        //LogPrint("mnpayments", "CMasternodePaymentVote::IsValid -- Can't calculate rank for masternode %s\n",
                 //vinMasternode.prevout.ToStringShort());
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        // It's common to have masternodes mistakenly think they are in the top 10
        // We don't want to print all of these messages in normal mode, debug mode should print though
        strError = strprintf("Masternode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        // Only ban for new mnw which is out of bounds, for old mnw MN list itself might be way too much off
        if (nRank > MNPAYMENTS_SIGNATURES_TOTAL * 2 && nBlockHeight > nValidationHeight) {
            strError = strprintf("Masternode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, nRank);
            //LogPrint("CMasternodePaymentVote::IsValid -- Error: %s\n", strError);
            Misbehaving(pnode->GetId(), 20);
        }
        // Still invalid however
        return false;
    }

    return true;
}

bool CMasternodePayments::ProcessBlock(int nBlockHeight) {

    // DETERMINE IF WE SHOULD BE VOTING FOR THE NEXT PAYEE

    if (fLiteMode || !fMasterNode) {
        return false;
    }

    // We have little chances to pick the right winner if winners list is out of sync
    // but we have no choice, so we'll try. However it doesn't make sense to even try to do so
    // if we have not enough data about masternodes.
    if (!masternodeSync.IsMasternodeListSynced()) {
        return false;
    }

    int nRank = mnodeman.GetMasternodeRank(activeMasternode.vin, nBlockHeight - 100, GetMinMasternodePaymentsProto(), false);

    if (nRank == -1) {
        LogPrintf("mnpayments CMasternodePayments::ProcessBlock -- Unknown Masternode\n");
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrintf("mnpayments CMasternodePayments::ProcessBlock -- Masternode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        return false;
    }

    // LOCATE THE NEXT MASTERNODE WHICH SHOULD BE PAID

    //LogPrintf("CMasternodePayments::ProcessBlock -- Start: nBlockHeight=%d, masternode=%s\n", nBlockHeight, activeMasternode.vin.prevout.ToStringShort());

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    CMasternode *pmn = mnodeman.GetNextMasternodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == NULL) {
        LogPrintf("CMasternodePayments::ProcessBlock -- ERROR: Failed to find masternode to pay\n");
        return false;
    }

    //LogPrintf("CMasternodePayments::ProcessBlock -- Masternode found by GetNextMasternodeInQueueForPayment(): %s\n", pmn->vin.prevout.ToStringShort());


    CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());

    CMasternodePaymentVote voteNew(activeMasternode.vin, nBlockHeight, payee);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    // SIGN MESSAGE TO NETWORK WITH OUR MASTERNODE KEYS

    //LogPrintf("ProcessBlock -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), voteNew.nBlockHeight, pCurrentBlockIndex->nHeight, voteNew.vinMasternode.prevout.ToStringShort());

    if (voteNew.Sign()) {
        if (AddPaymentVote(voteNew)) {
            voteNew.Relay();
            return true;
        }
    }

    return false;
}

void CMasternodePaymentVote::Relay() {
    // do not relay until synced
    if (!masternodeSync.IsWinnersListSynced()) {
        //LogPrint("CMasternodePaymentVote::Relay - masternodeSync.IsWinnersListSynced() not sync\n");
        return;
    }
    CInv inv(MSG_MASTERNODE_PAYMENT_VOTE, GetHash());
    g_connman->RelayInv(inv);
}

bool CMasternodePaymentVote::CheckSignature(const CPubKey &pubKeyMasternode, int nValidationHeight, int &nDos) {
    // do not ban by default
    nDos = 0;

    std::string strMessage = vinMasternode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    std::string strError = "";
    if (!darkSendSigner.VerifyMessage(pubKeyMasternode, vchSig, strMessage, strError)) {
        // Only ban for future block vote when we are already synced.
        // Otherwise it could be the case when MN which signed this vote is using another key now
        // and we have no idea about the old one.
        if (masternodeSync.IsMasternodeListSynced() && nBlockHeight > nValidationHeight) {
            nDos = 20;
        }
        return error("CMasternodePaymentVote::CheckSignature -- Got bad Masternode payment signature, masternode=%s, error: %s", vinMasternode.prevout.ToStringShort().c_str(), strError);
    }

    return true;
}

std::string CMasternodePaymentVote::ToString() const {
    std::ostringstream info;

    info << vinMasternode.prevout.ToStringShort() <<
         ", " << nBlockHeight <<
         ", " << ScriptToAsmStr(payee) <<
         ", " << (int) vchSig.size();

    return info.str();
}

// Send only votes for future blocks, node should request every other missing payment block individually
void CMasternodePayments::Sync(CNode *pnode) {
    LOCK(cs_mapMasternodeBlocks);

    if (!pCurrentBlockIndex) return;

    int nInvCount = 0;

    for (int h = pCurrentBlockIndex->nHeight; h < pCurrentBlockIndex->nHeight + 20; h++) {
        if (mapMasternodeBlocks.count(h)) {
            BOOST_FOREACH(CMasternodePayee & payee, mapMasternodeBlocks[h].vecPayees)
            {
                std::vector <uint256> vecVoteHashes = payee.GetVoteHashes();
                BOOST_FOREACH(uint256 & hash, vecVoteHashes)
                {
                    if (!HasVerifiedPaymentVote(hash)) continue;
                    pnode->PushInventory(CInv(MSG_MASTERNODE_PAYMENT_VOTE, hash));
                    nInvCount++;
                }
            }
        }
    }

    //LogPrint("CMasternodePayments::Sync -- Sent %d votes to peer %d\n", nInvCount, pnode->GetId());
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::SYNCSTATUSCOUNT, MASTERNODE_SYNC_MNW, nInvCount));
}

// Request low data/unknown payment blocks in batches directly from some node instead of/after preliminary Sync.
void CMasternodePayments::RequestLowDataPaymentBlocks(CNode *pnode) {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_main, cs_mapMasternodeBlocks);

    std::vector <CInv> vToFetch;
    int nLimit = GetStorageLimit();

    const CBlockIndex *pindex = pCurrentBlockIndex;

    while (pCurrentBlockIndex->nHeight - pindex->nHeight < nLimit) {
        if (!mapMasternodeBlocks.count(pindex->nHeight)) {
            // We have no idea about this block height, let's ask
            vToFetch.push_back(CInv(MSG_MASTERNODE_PAYMENT_BLOCK, pindex->GetBlockHash()));
            // We should not violate GETDATA rules
            if (vToFetch.size() == MAX_INV_SZ) {
                //LogPrint("CMasternodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d blocks\n", pnode->GetId(), MAX_INV_SZ);
                const CNetMsgMaker msgMaker(pnode->GetSendVersion());
                g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETDATA, vToFetch));
                // Start filling new batch
                vToFetch.clear();
            }
        }
        if (!pindex->pprev) break;
        pindex = pindex->pprev;
    }

    std::map<int, CMasternodeBlockPayees>::iterator it = mapMasternodeBlocks.begin();

    while (it != mapMasternodeBlocks.end()) {
        int nTotalVotes = 0;
        bool fFound = false;
        BOOST_FOREACH(CMasternodePayee & payee, it->second.vecPayees)
        {
            if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
                fFound = true;
                break;
            }
            nTotalVotes += payee.GetVoteCount();
        }
        // A clear winner (MNPAYMENTS_SIGNATURES_REQUIRED+ votes) was found
        // or no clear winner was found but there are at least avg number of votes
        if (fFound || nTotalVotes >= (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2) {
            // so just move to the next block
            ++it;
            continue;
        }
        // DEBUG
//        DBG (
//            // Let's see why this failed
//            BOOST_FOREACH(CMasternodePayee& payee, it->second.vecPayees) {
//                CTxDestination address1;
//                ExtractDestination(payee.GetPayee(), address1);
//                CBitcoinAddress address2(address1);
//                printf("payee %s votes %d\n", address2.ToString().c_str(), payee.GetVoteCount());
//            }
//            printf("block %d votes total %d\n", it->first, nTotalVotes);
//        )
        // END DEBUG
        // Low data block found, let's try to sync it
        uint256 hash;
        if (GetBlockHash(hash, it->first)) {
            vToFetch.push_back(CInv(MSG_MASTERNODE_PAYMENT_BLOCK, hash));
        }
        // We should not violate GETDATA rules
        if (vToFetch.size() == MAX_INV_SZ) {
            //LogPrint("CMasternodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->GetId(), MAX_INV_SZ);
            // Start filling new batch
            const CNetMsgMaker msgMaker(pnode->GetSendVersion());
            g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETDATA, vToFetch));
            vToFetch.clear();
        }
        ++it;
    }
    // Ask for the rest of it
    if (!vToFetch.empty()) {
        //LogPrint("CMasternodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->GetId(), vToFetch.size());
        const CNetMsgMaker msgMaker(pnode->GetSendVersion());
        g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETDATA, vToFetch));
    }
}

std::string CMasternodePayments::ToString() const {
    std::ostringstream info;

    info << "Votes: " << (int) mapMasternodePaymentVotes.size() <<
         ", Blocks: " << (int) mapMasternodeBlocks.size();

    return info.str();
}

bool CMasternodePayments::IsEnoughData() {
    float nAverageVotes = (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2;
    int nStorageLimit = GetStorageLimit();
    return GetBlockCount() > nStorageLimit && GetVoteCount() > nStorageLimit * nAverageVotes;
}

int CMasternodePayments::GetStorageLimit() {
    return std::max(int(mnodeman.size() * nStorageCoeff), nMinBlocksToStore);
}

void CMasternodePayments::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
    //LogPrint("mnpayments", "CMasternodePayments::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);
    
    ProcessBlock(pindex->nHeight + 5);
}
