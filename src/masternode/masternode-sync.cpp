// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2019 The Zyrk Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "checkpoints.h"
#include "validation.h"
#include "masternode.h"
#include "masternode-payments.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"
#include "boost/foreach.hpp"
#include "netmessagemaker.h"

class CMasternodeSync;

CMasternodeSync masternodeSync;

bool CMasternodeSync::CheckNodeHeight(CNode *pnode, bool fDisconnectStuckNodes) {
    CNodeStateStats stats;
    if (!GetNodeStateStats(pnode->GetId(), stats) || stats.nCommonHeight == -1 || stats.nSyncHeight == -1) return false; // not enough info about this peer

    // Check blocks and headers, allow a small error margin of 1 block
    if (pCurrentBlockIndex->nHeight - 1 > stats.nCommonHeight) {
        // This peer probably stuck, don't sync any additional data from it
        if (fDisconnectStuckNodes) {
            // Disconnect to free this connection slot for another peer.
            pnode->fDisconnect = true;
            //LogPrint("CMasternodeSync::CheckNodeHeight -- disconnecting from stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      //pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->GetId());
        } else {
            //LogPrint("CMasternodeSync::CheckNodeHeight -- skipping stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      //pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->GetId());
        }
        return false;
    } else if (pCurrentBlockIndex->nHeight < stats.nSyncHeight - 1) {
        // This peer announced more headers than we have blocks currently
        //LogPrint("CMasternodeSync::CheckNodeHeight -- skipping peer, who announced more headers than we have blocks currently, nHeight=%d, nSyncHeight=%d, peer=%d\n",
                  //pCurrentBlockIndex->nHeight, stats.nSyncHeight, pnode->GetId());
        return false;
    }

    return true;
}

bool CMasternodeSync::IsBlockchainSynced(bool fBlockAccepted) {
    static bool fBlockchainSynced = false;
    static int64_t nTimeLastProcess = GetTime();
    static int nSkipped = 0;
    static bool fFirstBlockAccepted = false;

    if(chainActive.Height() <= Params().GetConsensus().nMasternodeInitialize)
        return true;
    // if the last call to this function was more than 60 minutes ago (client was in sleep mode) reset the sync process
    if (GetTime() - nTimeLastProcess > 60 * 60) {
        //LogPrint("CMasternodeSync::IsBlockchainSynced time-check fBlockchainSynced=%s\n", fBlockchainSynced);
        Reset();
        fBlockchainSynced = false;
    }

    if (!pCurrentBlockIndex || !pindexBestHeader || fImporting || fReindex) return false;

    if (fBlockAccepted) {
        // this should be only triggered while we are still syncing
        if (!IsSynced(chainActive.Height())) {
            // we are trying to download smth, reset blockchain sync status
            fFirstBlockAccepted = true;
            fBlockchainSynced = false;
            nTimeLastProcess = GetTime();
            return false;
        }
    } else {
        // skip if we already checked less than 1 tick ago
        if (GetTime() - nTimeLastProcess < MASTERNODE_SYNC_TICK_SECONDS) {
            nSkipped++;
            return fBlockchainSynced;
        }
    }

    //LogPrint("masternode-sync", "CMasternodeSync::IsBlockchainSynced -- state before check: %ssynced, skipped %d times\n", fBlockchainSynced ? "" : "not ", nSkipped);

    nTimeLastProcess = GetTime();
    nSkipped = 0;

    if (fBlockchainSynced){
        return true;
    }

    if (fCheckpointsEnabled && pCurrentBlockIndex->nHeight < Checkpoints::GetTotalBlocksEstimate(Params().Checkpoints())) {
        return false;
    }

    std::vector < CNode * > vNodesCopy = g_connman->CopyNodeVector();
    bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);
    int enough_peers = MASTERNODE_SYNC_ENOUGH_PEERS;
    if(fTestNet)
        enough_peers = MASTERNODE_SYNC_ENOUGH_PEERS_TESTNET;

    // We have enough peers and assume most of them are synced
    if (vNodesCopy.size() >= enough_peers) {
        // Check to see how many of our peers are (almost) at the same height as we are
        int nNodesAtSameHeight = 0;
        BOOST_FOREACH(CNode * pnode, vNodesCopy)
        {
            // Make sure this peer is presumably at the same height
            if (!CheckNodeHeight(pnode)) {
                continue;
            }
            nNodesAtSameHeight++;
            // if we have decent number of such peers, most likely we are synced now
            if (nNodesAtSameHeight >= enough_peers) {
                //LogPrint("CMasternodeSync::IsBlockchainSynced -- found enough peers on the same height as we are, done\n");
                fBlockchainSynced = true;
                g_connman->ReleaseNodeVector(vNodesCopy);
                return true;
            }
        }
    }
    g_connman->ReleaseNodeVector(vNodesCopy);

    // wait for at least one new block to be accepted
    if (!fFirstBlockAccepted) return false;

    // same as !IsInitialBlockDownload() but no cs_main needed here
    int64_t nMaxBlockTime = std::max(pCurrentBlockIndex->GetBlockTime(), pindexBestHeader->GetBlockTime());
    fBlockchainSynced = pindexBestHeader->nHeight - pCurrentBlockIndex->nHeight < 24 * 6 &&
                        GetTime() - nMaxBlockTime < Params().MaxTipAge();
    return fBlockchainSynced;
}

void CMasternodeSync::Fail() {
    nTimeLastFailure = GetTime();
    nRequestedMasternodeAssets = MASTERNODE_SYNC_FAILED;
}

void CMasternodeSync::Reset() {
    nRequestedMasternodeAssets = MASTERNODE_SYNC_INITIAL;
    nRequestedMasternodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    nTimeLastMasternodeList = GetTime();
    nTimeLastPaymentVote = GetTime();
    nTimeLastGovernanceItem = GetTime();
    nTimeLastFailure = 0;
    nCountFailures = 0;
}

std::string CMasternodeSync::GetAssetName() {
    switch (nRequestedMasternodeAssets) {
        case (MASTERNODE_SYNC_INITIAL):
            return "MASTERNODE_SYNC_INITIAL";
        case (MASTERNODE_SYNC_SPORKS):
            return "MASTERNODE_SYNC_SPORKS";
        case (MASTERNODE_SYNC_LIST):
            return "MASTERNODE_SYNC_LIST";
        case (MASTERNODE_SYNC_MNW):
            return "MASTERNODE_SYNC_MNW";
        case (MASTERNODE_SYNC_FAILED):
            return "MASTERNODE_SYNC_FAILED";
        case MASTERNODE_SYNC_FINISHED:
            return "MASTERNODE_SYNC_FINISHED";
        default:
            return "UNKNOWN";
    }
}

void CMasternodeSync::SwitchToNextAsset() {
    switch (nRequestedMasternodeAssets) {
        case (MASTERNODE_SYNC_FAILED):
            throw std::runtime_error("Can't switch to next asset from failed, should use Reset() first!");
            break;
        case (MASTERNODE_SYNC_INITIAL):
            ClearFulfilledRequests();
            nRequestedMasternodeAssets = MASTERNODE_SYNC_SPORKS;
            //LogPrint("CMasternodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (MASTERNODE_SYNC_SPORKS):
            nTimeLastMasternodeList = GetTime();
            nRequestedMasternodeAssets = MASTERNODE_SYNC_LIST;
            //LogPrint("CMasternodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (MASTERNODE_SYNC_LIST):
            nTimeLastPaymentVote = GetTime();
            nRequestedMasternodeAssets = MASTERNODE_SYNC_MNW;
            //LogPrint("CMasternodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;

        case (MASTERNODE_SYNC_MNW):
            nTimeLastGovernanceItem = GetTime();
            //LogPrint("CMasternodeSync::SwitchToNextAsset -- Sync has finished\n");
            nRequestedMasternodeAssets = MASTERNODE_SYNC_FINISHED;
            break;
    }
    nRequestedMasternodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
}

std::string CMasternodeSync::GetSyncStatus() {
    switch (masternodeSync.nRequestedMasternodeAssets) {
        case MASTERNODE_SYNC_INITIAL:
            return _("Synchronization pending...");
        case MASTERNODE_SYNC_SPORKS:
            return _("Synchronizing sporks...");
        case MASTERNODE_SYNC_LIST:
            return _("Synchronizing masternodes...");
        case MASTERNODE_SYNC_MNW:
            return _("Synchronizing masternode payments...");
        case MASTERNODE_SYNC_FAILED:
            return _("Synchronization failed");
        case MASTERNODE_SYNC_FINISHED:
            return _("Synchronization finished");
        default:
            return "";
    }
}

void CMasternodeSync::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {
    if (strCommand == NetMsgType::SYNCSTATUSCOUNT) { //Sync status count

        //do not care about stats if sync process finished or failed
        if (IsSynced(chainActive.Height())|| IsFailed()) return;

        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        //LogPrint("SYNCSTATUSCOUNT -- got inventory count: nItemID=%d  nCount=%d  peer=%d\n", nItemID, nCount, pfrom->GetId());
    }
}

void CMasternodeSync::ClearFulfilledRequests() {
    TRY_LOCK(g_connman->cs_vNodes, lockRecv);
    if (!lockRecv) return;

    BOOST_FOREACH(CNode * pnode, g_connman->vNodes)
    {
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "spork-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "masternode-list-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "masternode-payment-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "full-sync");
    }
}

void CMasternodeSync::ProcessTick() {
    static int nTick = 0;
    if (nTick++ % MASTERNODE_SYNC_TICK_SECONDS != 0) return;
    if (!pCurrentBlockIndex) return;

    //the actual count of masternodes we have currently
    int nMnCount = mnodeman.CountMasternodes();

    //LogPrint("ProcessTick", "CMasternodeSync::ProcessTick -- nTick %d nMnCount %d\n", nTick, nMnCount);

    // INITIAL SYNC SETUP / LOG REPORTING
    double nSyncProgress = double(nRequestedMasternodeAttempt + (nRequestedMasternodeAssets - 1) * 8) / (8 * 4);
    //LogPrint("ProcessTick", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d nRequestedMasternodeAttempt %d nSyncProgress %f\n", nTick, nRequestedMasternodeAssets, nRequestedMasternodeAttempt, nSyncProgress);
    uiInterface.NotifyAdditionalDataSyncProgressChanged(pCurrentBlockIndex->nHeight, nSyncProgress);

    // RESET SYNCING INCASE OF FAILURE
    {
        if (IsSynced(chainActive.Height())) {
            std::vector < CNode * > vNodesCopy = g_connman->CopyNodeVector();
            g_connman->ReleaseNodeVector(vNodesCopy);
            return;
        }

        //try syncing again
        if (IsFailed()) {
            if (nTimeLastFailure + (1 * 60) < GetTime()) { // 1 minute cooldown after failed sync
                Reset();
            }
            return;
        }
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !IsBlockchainSynced() && nRequestedMasternodeAssets > MASTERNODE_SYNC_SPORKS) {
        nTimeLastMasternodeList = GetTime();
        nTimeLastPaymentVote = GetTime();
        nTimeLastGovernanceItem = GetTime();
        return;
    }
    if (nRequestedMasternodeAssets == MASTERNODE_SYNC_INITIAL || (nRequestedMasternodeAssets == MASTERNODE_SYNC_SPORKS && IsBlockchainSynced())) {
        SwitchToNextAsset();
    }

    std::vector < CNode * > vNodesCopy = g_connman->CopyNodeVector();

    BOOST_FOREACH(CNode * pnode, vNodesCopy)
    {
        // Don't try to sync any data from outbound "masternode" connections -
        // they are temporary and should be considered unreliable for a sync process.
        // Inbound connection this early is most likely a "masternode" connection
        // initialted from another node, so skip it too.
        if (pnode->fMasternode || (fMasterNode && pnode->fInbound)) continue;

        // QUICK MODE (REGTEST ONLY!)
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
            if (nRequestedMasternodeAttempt <= 2) {
                const CNetMsgMaker msgMaker(pnode->GetSendVersion());
                g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETSPORKS)); //get current network sporks
            } else if (nRequestedMasternodeAttempt < 4) {
                mnodeman.DsegUpdate(pnode);
            } else if (nRequestedMasternodeAttempt < 6) {
                int nMnCount = mnodeman.CountMasternodes();
                const CNetMsgMaker msgMaker(pnode->GetSendVersion());
                g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::MASTERNODEPAYMENTSYNC,nMnCount)); //sync payment votes
            } else {
                nRequestedMasternodeAssets = MASTERNODE_SYNC_FINISHED;
            }
            nRequestedMasternodeAttempt++;
            g_connman->ReleaseNodeVector(vNodesCopy);
            return;
        }

        // NORMAL NETWORK MODE - TESTNET/MAINNET
        {
            if (netfulfilledman.HasFulfilledRequest(pnode->addr, "full-sync")) {
                // We already fully synced from this node recently,
                // disconnect to free this connection slot for another peer.
                pnode->fDisconnect = true;
                //LogPrint("CMasternodeSync::ProcessTick -- disconnecting from recently synced peer %d\n", pnode->GetId());
                continue;
            }

            // SPORK : ALWAYS ASK FOR SPORKS AS WE SYNC (we skip this mode now)

            if (!netfulfilledman.HasFulfilledRequest(pnode->addr, "spork-sync")) {
                // only request once from each peer
                netfulfilledman.AddFulfilledRequest(pnode->addr, "spork-sync");
                // get current network sporks
                const CNetMsgMaker msgMaker(pnode->GetSendVersion());
                g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETSPORKS));
                //LogPrint("CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- requesting sporks from peer %d\n", nTick, nRequestedMasternodeAssets, pnode->GetId());
                continue; // always get sporks first, switch to the next node without waiting for the next tick
            }

            // MNLIST : SYNC MASTERNODE LIST FROM OTHER CONNECTED CLIENTS

            if (nRequestedMasternodeAssets == MASTERNODE_SYNC_LIST) {
                // check for timeout first
                if (nTimeLastMasternodeList < GetTime() - MASTERNODE_SYNC_TIMEOUT_SECONDS) {
                    //LogPrint("CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- timeout\n", nTick, nRequestedMasternodeAssets);
                    if (nRequestedMasternodeAttempt == 0) {
                        //LogPrint("CMasternodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // there is no way we can continue without masternode list, fail here and try later
                        Fail();
                        g_connman->ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    g_connman->ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "masternode-list-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "masternode-list-sync");

                if (pnode->nVersion < mnpayments.GetMinMasternodePaymentsProto()) continue;
                nRequestedMasternodeAttempt++;

                mnodeman.DsegUpdate(pnode);

                g_connman->ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // MNW : SYNC MASTERNODE PAYMENT VOTES FROM OTHER CONNECTED CLIENTS

            if (nRequestedMasternodeAssets == MASTERNODE_SYNC_MNW) {
                //LogPrint("mnpayments", "CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d nTimeLastPaymentVote %lld GetTime() %lld diff %lld\n", nTick, nRequestedMasternodeAssets, nTimeLastPaymentVote, GetTime(), GetTime() - nTimeLastPaymentVote);
                // check for timeout first
                // This might take a lot longer than MASTERNODE_SYNC_TIMEOUT_SECONDS minutes due to new blocks,
                // but that should be OK and it should timeout eventually.
                if (nTimeLastPaymentVote < GetTime() - MASTERNODE_SYNC_TIMEOUT_SECONDS) {
                    //LogPrint("CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- timeout\n", nTick, nRequestedMasternodeAssets);
                    if (nRequestedMasternodeAttempt == 0) {
                        //LogPrint("CMasternodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // probably not a good idea to proceed without winner list
                        Fail();
                        g_connman->ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    g_connman->ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // check for data
                // if mnpayments already has enough blocks and votes, switch to the next asset
                // try to fetch data from at least two peers though
                if (nRequestedMasternodeAttempt > 1 && mnpayments.IsEnoughData()) {
                    //LogPrint("CMasternodeSync::ProcessTick -- nTick %d nRequestedMasternodeAssets %d -- found enough data\n", nTick, nRequestedMasternodeAssets);
                    SwitchToNextAsset();
                    g_connman->ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "masternode-payment-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "masternode-payment-sync");

                if (pnode->nVersion < mnpayments.GetMinMasternodePaymentsProto()) continue;
                nRequestedMasternodeAttempt++;

                // ask node for all payment votes it has (new nodes will only return votes for future payments)
                const CNetMsgMaker msgMaker(pnode->GetSendVersion());
                g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::MASTERNODEPAYMENTSYNC, mnpayments.GetStorageLimit()));
                // ask node for missing pieces only (old nodes will not be asked)
                mnpayments.RequestLowDataPaymentBlocks(pnode);

                g_connman->ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

        }
    }
    // looped through all nodes, release them
    g_connman->ReleaseNodeVector(vNodesCopy);
}

void CMasternodeSync::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
}
