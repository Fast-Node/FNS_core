// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2018 The FASTNODE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "spork.h"
#include "base58.h"
#include "key.h"
#include "main.h"
#include "masternode-budget.h"
#include "net.h"
#include "protocol.h"
#include "sync.h"
#include "sporkdb.h"
#include "util.h"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

class CSporkMessage;
class CSporkManager;

CSporkManager sporkManager;

std::map<uint256, CSporkMessage> mapSporks;
std::map<int, CSporkMessage> mapSporksActive;

// FASTNODE: on startup load spork values from previous session if they exist in the sporkDB
void LoadSporksFromDB()
{
    for (int i = SPORK_START; i <= SPORK_END; ++i) {
        // Since not all spork IDs are in use, we have to exclude undefined IDs
        std::string strSpork = sporkManager.GetSporkNameByID(i);
        if (strSpork == "Unknown") continue;

        // attempt to read spork from sporkDB
        CSporkMessage spork;
        if (!pSporkDB->ReadSpork(i, spork)) {
            LogPrintf("%s : no previous value for %s found in database\n", __func__, strSpork);
            continue;
        }

        // add spork to memory
        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        std::time_t result = spork.nValue;
        // If SPORK Value is greater than 1,000,000 assume it's actually a Date and then convert to a more readable format
        if (spork.nValue > 1000000) {
            LogPrintf("%s : loaded spork %s with value %d : %s", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue,
                      std::ctime(&result));
        } else {
            LogPrintf("%s : loaded spork %s with value %d\n", __func__,
                      sporkManager.GetSporkNameByID(spork.nSporkID), spork.nValue);
        }
    }
}

void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return; //disable all obfuscation/masternode related functionality

    if (strCommand == "spork") {
        //LogPrintf("ProcessSpork::spork\n");
        CDataStream vMsg(vRecv);
        CSporkMessage spork;
        vRecv >> spork;

        if (chainActive.Tip() == NULL) return;

        // Ignore spork messages about unknown/deleted sporks
        std::string strSpork = sporkManager.GetSporkNameByID(spork.nSporkID);
        if (strSpork == "Unknown") return;

        uint256 hash = spork.GetHash();
        if (mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                if (fDebug) LogPrintf("%s : seen %s block %d \n", __func__, hash.ToString(), chainActive.Tip()->nHeight);
                return;
            } else {
                if (fDebug) LogPrintf("%s : got updated spork %s block %d \n", __func__, hash.ToString(), chainActive.Tip()->nHeight);
            }
        }

        LogPrintf("%s : new %s ID %d Time %d bestHeight %d\n", __func__, hash.ToString(), spork.nSporkID, spork.nValue, chainActive.Tip()->nHeight);

        if (!sporkManager.CheckSignature(spork)) {
            LogPrintf("%s : Invalid Signature\n", __func__);
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        sporkManager.Relay(spork);

        // FASTNODE: add to spork database.
        pSporkDB->WriteSpork(spork.nSporkID, spork);
    }
    if (strCommand == "getsporks") {
        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();

        while (it != mapSporksActive.end()) {
            pfrom->PushMessage("spork", it->second);
            it++;
        }
    }
}


// grab the value of the spork on the network, or the default
int64_t GetSporkValue(int nSporkID)
{
    int64_t r = -1;

    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_2_SWIFTTX) r = SPORK_2_SWIFTTX_DEFAULT;
        if (nSporkID == SPORK_3_SWIFTTX_BLOCK_FILTERING) r = SPORK_3_SWIFTTX_BLOCK_FILTERING_DEFAULT;
        if (nSporkID == SPORK_5_MAX_VALUE) r = SPORK_5_MAX_VALUE_DEFAULT;
        if (nSporkID == SPORK_7_MASTERNODE_SCANNING) r = SPORK_7_MASTERNODE_SCANNING_DEFAULT;
        if (nSporkID == SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT) r = SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT) r = SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_10_MASTERNODE_PAY_UPDATED_NODES) r = SPORK_10_MASTERNODE_PAY_UPDATED_NODES_DEFAULT;
        if (nSporkID == SPORK_13_ENABLE_SUPERBLOCKS) r = SPORK_13_ENABLE_SUPERBLOCKS_DEFAULT;
        if (nSporkID == SPORK_14_NEW_PROTOCOL_ENFORCEMENT) r = SPORK_14_NEW_PROTOCOL_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2) r = SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2_DEFAULT;
        if (nSporkID == SPORK_16_ZEROCOIN_MAINTENANCE_MODE) r = SPORK_16_ZEROCOIN_MAINTENANCE_MODE_DEFAULT;
        if (nSporkID == SPORK_17_REQUIRED_MN_COLLATERAL) r = SPORK_17_REQUIRED_MN_COLLATERAL_DEFAULT;
        if (nSporkID == SPORK_18_COLLATERAL_1000) r = SPORK_18_COLLATERAL_1000_DEFAULT;
        if (nSporkID == SPORK_19_COLLATERAL_1500) r = SPORK_19_COLLATERAL_1500_DEFAULT;
        if (nSporkID == SPORK_20_COLLATERAL_2000) r = SPORK_20_COLLATERAL_2000_DEFAULT;
        if (nSporkID == SPORK_21_COLLATERAL_2500) r = SPORK_21_COLLATERAL_2500_DEFAULT;
        if (nSporkID == SPORK_22_COLLATERAL_3000) r = SPORK_22_COLLATERAL_3000_DEFAULT;
        if (nSporkID == SPORK_23_COLLATERAL_3500) r = SPORK_23_COLLATERAL_3500_DEFAULT;
        if (nSporkID == SPORK_24_COLLATERAL_4000) r = SPORK_24_COLLATERAL_4000_DEFAULT;
        if (nSporkID == SPORK_25_COLLATERAL_4500) r = SPORK_25_COLLATERAL_4500_DEFAULT;
        if (nSporkID == SPORK_26_COLLATERAL_5500) r = SPORK_26_COLLATERAL_5500_DEFAULT;
        if (nSporkID == SPORK_27_COLLATERAL_6500) r = SPORK_27_COLLATERAL_6500_DEFAULT;
        if (nSporkID == SPORK_28_COLLATERAL_7500) r = SPORK_28_COLLATERAL_7500_DEFAULT;
        if (nSporkID == SPORK_29_COLLATERAL_8500) r = SPORK_29_COLLATERAL_8500_DEFAULT;
        if (nSporkID == SPORK_30_COLLATERAL_9500) r = SPORK_30_COLLATERAL_9500_DEFAULT;
        if (nSporkID == SPORK_31_COLLATERAL_10000) r = SPORK_31_COLLATERAL_10000_DEFAULT;
        if (nSporkID == SPORK_32_COLLATERAL_20000) r = SPORK_32_COLLATERAL_20000_DEFAULT;


        if (r == -1) LogPrintf("%s : Unknown Spork %d\n", __func__, nSporkID);
    }

    return r;
}

// grab the spork value, and see if it's off
bool IsSporkActive(int nSporkID)
{
    int64_t r = GetSporkValue(nSporkID);
    if (r == -1) return false;
    return r < GetTime();
}


void ReprocessBlocks(int nBlocks)
{
    std::map<uint256, int64_t>::iterator it = mapRejectedBlocks.begin();
    while (it != mapRejectedBlocks.end()) {
        //use a window twice as large as is usual for the nBlocks we want to reset
        if ((*it).second > GetTime() - (nBlocks * 60 * 5)) {
            BlockMap::iterator mi = mapBlockIndex.find((*it).first);
            if (mi != mapBlockIndex.end() && (*mi).second) {
                LOCK(cs_main);

                CBlockIndex* pindex = (*mi).second;
                LogPrintf("ReprocessBlocks - %s\n", (*it).first.ToString());

                CValidationState state;
                ReconsiderBlock(state, pindex);
            }
        }
        ++it;
    }

    CValidationState state;
    {
        LOCK(cs_main);
        DisconnectBlocksAndReprocess(nBlocks);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }
}

bool CSporkManager::CheckSignature(CSporkMessage& spork, bool fCheckSigner)
{
    //note: need to investigate why this is failing
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);
    CPubKey pubkeynew(ParseHex(Params().SporkKey()));
    std::string errorMessage = "";

    bool fValidWithNewKey = obfuScationSigner.VerifyMessage(pubkeynew, spork.vchSig,strMessage, errorMessage);

    if (fCheckSigner && !fValidWithNewKey)
        return false;

    return fValidWithNewKey;
}

bool CSporkManager::Sign(CSporkMessage& spork)
{
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!obfuScationSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("CMasternodePayments::Sign - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage);
        return false;
    }

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, spork.vchSig, key2)) {
        LogPrintf("CMasternodePayments::Sign - Sign message failed");
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubkey2, spork.vchSig, strMessage, errorMessage)) {
        LogPrintf("CMasternodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue)
{
    CSporkMessage msg;
    msg.nSporkID = nSporkID;
    msg.nValue = nValue;
    msg.nTimeSigned = GetTime();

    if (Sign(msg)) {
        Relay(msg);
        mapSporks[msg.GetHash()] = msg;
        mapSporksActive[nSporkID] = msg;
        return true;
    }

    return false;
}

void CSporkManager::Relay(CSporkMessage& msg)
{
    CInv inv(MSG_SPORK, msg.GetHash());
    RelayInv(inv);
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage msg;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(msg);

    if (CheckSignature(msg, true)) {
        LogPrintf("CSporkManager::SetPrivKey - Successfully initialized as spork signer\n");
        return true;
    } else {
        return false;
    }
}

int CSporkManager::GetSporkIDByName(std::string strName)
{
    if (strName == "SPORK_2_SWIFTTX") return SPORK_2_SWIFTTX;
    if (strName == "SPORK_3_SWIFTTX_BLOCK_FILTERING") return SPORK_3_SWIFTTX_BLOCK_FILTERING;
    if (strName == "SPORK_5_MAX_VALUE") return SPORK_5_MAX_VALUE;
    if (strName == "SPORK_7_MASTERNODE_SCANNING") return SPORK_7_MASTERNODE_SCANNING;
    if (strName == "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT") return SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT") return SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT;
    if (strName == "SPORK_10_MASTERNODE_PAY_UPDATED_NODES") return SPORK_10_MASTERNODE_PAY_UPDATED_NODES;
    if (strName == "SPORK_13_ENABLE_SUPERBLOCKS") return SPORK_13_ENABLE_SUPERBLOCKS;
    if (strName == "SPORK_14_NEW_PROTOCOL_ENFORCEMENT") return SPORK_14_NEW_PROTOCOL_ENFORCEMENT;
    if (strName == "SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2") return SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2;
    if (strName == "SPORK_16_ZEROCOIN_MAINTENANCE_MODE") return SPORK_16_ZEROCOIN_MAINTENANCE_MODE;
    if (strName == "SPORK_17_REQUIRED_MN_COLLATERAL") return SPORK_17_REQUIRED_MN_COLLATERAL;
    if (strName == "SPORK_18_COLLATERAL_1000") return SPORK_18_COLLATERAL_1000;
    if (strName == "SPORK_19_COLLATERAL_1500") return SPORK_19_COLLATERAL_1500;
    if (strName == "SPORK_20_COLLATERAL_2000") return SPORK_20_COLLATERAL_2000;
    if (strName == "SPORK_21_COLLATERAL_2500") return SPORK_21_COLLATERAL_2500;
    if (strName == "SPORK_22_COLLATERAL_3000") return SPORK_22_COLLATERAL_3000;
    if (strName == "SPORK_23_COLLATERAL_3500") return SPORK_23_COLLATERAL_3500;
    if (strName == "SPORK_24_COLLATERAL_4000") return SPORK_24_COLLATERAL_4000;
    if (strName == "SPORK_25_COLLATERAL_4500") return SPORK_25_COLLATERAL_4500;
    if (strName == "SPORK_26_COLLATERAL_5500") return SPORK_26_COLLATERAL_5500;
    if (strName == "SPORK_27_COLLATERAL_6500") return SPORK_27_COLLATERAL_6500;
    if (strName == "SPORK_28_COLLATERAL_7500") return SPORK_28_COLLATERAL_7500;
    if (strName == "SPORK_29_COLLATERAL_8500") return SPORK_29_COLLATERAL_8500;
    if (strName == "SPORK_30_COLLATERAL_9500") return SPORK_30_COLLATERAL_9500;
    if (strName == "SPORK_31_COLLATERAL_10000") return SPORK_31_COLLATERAL_10000;
    if (strName == "SPORK_32_COLLATERAL_20000") return SPORK_32_COLLATERAL_20000;

    return -1;
}

std::string CSporkManager::GetSporkNameByID(int id)
{
    if (id == SPORK_2_SWIFTTX) return "SPORK_2_SWIFTTX";
    if (id == SPORK_3_SWIFTTX_BLOCK_FILTERING) return "SPORK_3_SWIFTTX_BLOCK_FILTERING";
    if (id == SPORK_5_MAX_VALUE) return "SPORK_5_MAX_VALUE";
    if (id == SPORK_7_MASTERNODE_SCANNING) return "SPORK_7_MASTERNODE_SCANNING";
    if (id == SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT) return "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT";
    if (id == SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT) return "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT";
    if (id == SPORK_10_MASTERNODE_PAY_UPDATED_NODES) return "SPORK_10_MASTERNODE_PAY_UPDATED_NODES";
    if (id == SPORK_13_ENABLE_SUPERBLOCKS) return "SPORK_13_ENABLE_SUPERBLOCKS";
    if (id == SPORK_14_NEW_PROTOCOL_ENFORCEMENT) return "SPORK_14_NEW_PROTOCOL_ENFORCEMENT";
    if (id == SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2) return "SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2";
    if (id == SPORK_16_ZEROCOIN_MAINTENANCE_MODE) return "SPORK_16_ZEROCOIN_MAINTENANCE_MODE";
    if (id == SPORK_17_REQUIRED_MN_COLLATERAL) return "SPORK_17_REQUIRED_MN_COLLATERAL";
    if (id == SPORK_18_COLLATERAL_1000) return "SPORK_18_COLLATERAL_1000";
    if (id == SPORK_19_COLLATERAL_1500) return "SPORK_19_COLLATERAL_1500";
    if (id == SPORK_20_COLLATERAL_2000) return "SPORK_20_COLLATERAL_2000";
    if (id == SPORK_21_COLLATERAL_2500) return "SPORK_21_COLLATERAL_2500";
    if (id == SPORK_22_COLLATERAL_3000) return "SPORK_22_COLLATERAL_3000";
    if (id == SPORK_23_COLLATERAL_3500) return "SPORK_23_COLLATERAL_3500";
    if (id == SPORK_24_COLLATERAL_4000) return "SPORK_24_COLLATERAL_4000";
    if (id == SPORK_25_COLLATERAL_4500) return "SPORK_25_COLLATERAL_4500";
    if (id == SPORK_26_COLLATERAL_5500) return "SPORK_26_COLLATERAL_5500";
    if (id == SPORK_27_COLLATERAL_6500) return "SPORK_27_COLLATERAL_6500";
    if (id == SPORK_28_COLLATERAL_7500) return "SPORK_28_COLLATERAL_7500";
    if (id == SPORK_29_COLLATERAL_8500) return "SPORK_29_COLLATERAL_8500";
    if (id == SPORK_30_COLLATERAL_9500) return "SPORK_30_COLLATERAL_9500";
    if (id == SPORK_31_COLLATERAL_10000) return "SPORK_31_COLLATERAL_10000";
    if (id == SPORK_32_COLLATERAL_20000) return "SPORK_32_COLLATERAL_20000";

    return "Unknown";
}
