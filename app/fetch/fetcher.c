#include "fetcher.h"

#include "../../core/logger.h"
#include "../../ndn/nni.h"

INIT_ZF_LOG(Fetcher);

#define FETCHER_TX_BURST_SIZE 64

static uint8_t
Fetcher_ChooseTpl0(Fetcher* fetcher, uint64_t segNum)
{
  return 0;
}

static uint8_t
Fetcher_ChooseTplDiv(Fetcher* fetcher, uint64_t segNum)
{
  return segNum % fetcher->nTpls;
}

static uint8_t
Fetcher_ChooseTplMask(Fetcher* fetcher, uint64_t segNum)
{
  return segNum & (fetcher->nTpls - 1);
}

static void
Fetcher_SetChooseTpl(Fetcher* fetcher)
{
  assert(fetcher->nTpls > 0);
  if (fetcher->nTpls == 1) {
    fetcher->chooseTpl = Fetcher_ChooseTpl0;
  } else if (rte_is_power_of_2(fetcher->nTpls)) {
    fetcher->chooseTpl = Fetcher_ChooseTplMask;
  } else {
    fetcher->chooseTpl = Fetcher_ChooseTplDiv;
  }
}

static void
Fetcher_Encode(Fetcher* fetcher, Packet* npkt, uint64_t segNum)
{
  uint8_t suffix[10];
  suffix[0] = TT_SegmentNameComponent;
  suffix[1] = EncodeNni(&suffix[2], segNum);
  LName nameSuffix = { .length = suffix[1] + 2, .value = suffix };

  struct rte_mbuf* pkt = Packet_ToMbuf(npkt);
  uint8_t tplIndex = (*fetcher->chooseTpl)(fetcher, segNum);
  uint32_t nonce = NonceGen_Next(&fetcher->nonceGen);
  EncodeInterest(pkt, &fetcher->tpl[tplIndex], nameSuffix, nonce);

  Packet_SetL3PktType(npkt, L3PktType_Interest); // for stats; no PInterest*
  Packet_InitLpL3Hdr(npkt)->pitToken = fetcher->pitTokenBase | tplIndex;
}

static void
Fetcher_TxBurst(Fetcher* fetcher)
{
  uint64_t segNums[FETCHER_TX_BURST_SIZE];
  size_t count =
    FetchLogic_TxInterestBurst(&fetcher->logic, segNums, FETCHER_TX_BURST_SIZE);
  if (unlikely(count == 0)) {
    return;
  }

  Packet* npkts[FETCHER_TX_BURST_SIZE];
  int res = rte_pktmbuf_alloc_bulk(
    fetcher->interestMp, (struct rte_mbuf**)npkts, count);
  if (unlikely(res != 0)) {
    ZF_LOGW("%p interestMp-full", fetcher);
    return;
  }

  for (int i = 0; i < count; ++i) {
    Fetcher_Encode(fetcher, npkts[i], segNums[i]);
  }
  Face_TxBurst(fetcher->face, npkts, count);
}

static bool
Fetcher_Decode(Fetcher* fetcher, Packet* npkt, FetchLogicRxData* lpkt)
{
  if (unlikely(Packet_GetL3PktType(npkt) != L3PktType_Data)) {
    return false;
  }
  LpL3* lpl3 = Packet_GetLpL3Hdr(npkt);
  lpkt->congMark = lpl3->congMark;

  uint8_t tplIndex = lpl3->pitToken;
  const InterestTemplate* tpl = &fetcher->tpl[tplIndex];

  const PData* data = Packet_GetDataHdr(npkt);
  const uint8_t* seqNumComp = RTE_PTR_ADD(data->name.v, tpl->prefixL);
  return data->name.p.nOctets > tpl->prefixL + 1 &&
         memcmp(data->name.v, tpl->prefixV, tpl->prefixL + 1) == 0 &&
         DecodeNni(seqNumComp[1], &seqNumComp[2], &lpkt->segNum) ==
           NdnError_OK &&
         (*fetcher->chooseTpl)(fetcher, lpkt->segNum) == tplIndex;
}

static void
Fetcher_RxBurst(Fetcher* fetcher)
{
  Packet* npkts[PKTQUEUE_BURST_SIZE_MAX];
  uint32_t nRx = PktQueue_Pop(&fetcher->rxQueue,
                              (struct rte_mbuf**)npkts,
                              PKTQUEUE_BURST_SIZE_MAX,
                              rte_get_tsc_cycles())
                   .count;

  FetchLogicRxData lpkts[PKTQUEUE_BURST_SIZE_MAX];
  size_t count = 0;
  for (uint16_t i = 0; i < nRx; ++i) {
    bool ok = Fetcher_Decode(fetcher, npkts[i], &lpkts[count]);
    if (likely(ok)) {
      ++count;
    }
  }
  FetchLogic_RxDataBurst(&fetcher->logic, lpkts, count);
  FreeMbufs((struct rte_mbuf**)npkts, nRx);
}

int
Fetcher_Run(Fetcher* fetcher)
{
  Fetcher_SetChooseTpl(fetcher);
  while (ThreadStopFlag_ShouldContinue(&fetcher->stop)) {
    if (unlikely(FetchLogic_Finished(&fetcher->logic))) {
      return FETCHER_COMPLETED;
    }
    MinSched_Trigger(fetcher->logic.sched);
    Fetcher_TxBurst(fetcher);
    Fetcher_RxBurst(fetcher);
  }
  return FETCHER_STOPPED;
}
