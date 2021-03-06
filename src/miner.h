// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016 The KORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H
#include "amount.h"

#include <stdint.h>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CChainParams;
class CReserveKey;
class CScript;
class CWallet;

static const bool DEFAULT_PRINTPRIORITY_LEGACY = false;

struct CBlockTemplate;

CAmount GetBlockReward(CBlockIndex* pindexPrev);
uint32_t GetNextTarget(const CBlockIndex* pindexLast, const CBlockHeader* pblock, bool fProofOfStake);
void UpdateTime(CBlockHeader* block, const CBlockIndex* pindexPrev, bool fProofOfStake = false);
CBlockTemplate* CreateNewBlock_Legacy(const CChainParams& chainparams, const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake);
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake);
CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet, bool fProofOfStake);
bool SignBlock_Legacy(CWallet* pwallet, CBlock* pblock);
bool ProcessBlockFound_Legacy(const CBlock* pblock, const CChainParams& chainparams);
bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey);
/** Modify the extranonce in a block */
void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
/** Run the miner threads */
void GenerateKores(bool fGenerate, int nThreads);
/** Run the staking thread */
void StakingCoins(bool fStaking);

void updateStaking2KoreConf( bool staking );

extern double dHashesPerMin;
extern int64_t nHPSTimerStart;

#endif // BITCOIN_MINER_H
