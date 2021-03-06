#ifndef NDNDPDK_NDNI_DATA_H
#define NDNDPDK_NDNI_DATA_H

/** @file */

#include "name.h"

/** @brief Parsed Data packet. */
typedef struct PData
{
  PName name;
  uint32_t freshness; ///< FreshnessPeriod in millis
  bool hasDigest;
  uint8_t digest[32];
} PData;

/**
 * @brief Parse Data.
 * @param pkt a uniquely owned, possibly segmented, direct mbuf that contains Data TLV.
 * @return whether success.
 */
__attribute__((nonnull)) bool
PData_Parse(PData* data, struct rte_mbuf* pkt);

/** @brief Determine whether Data can satisfy Interest. */
__attribute__((nonnull)) DataSatisfyResult
PData_CanSatisfy(PData* data, PInterest* interest);

/**
 * @brief Prepare a crypto_op for Data digest computation.
 * @param npkt Data packet.
 * @param[out] op an allocated crypto_op; will be populated but not enqueued.
 */
__attribute__((nonnull)) void
DataDigest_Prepare(Packet* npkt, struct rte_crypto_op* op);

/**
 * @brief Finish Data digest computation.
 * @param op a dequeued crypto_op; will be freed.
 * @return the Data packet, or NULL if crypto_op was unsuccessful.
 */
__attribute__((nonnull)) Packet*
DataDigest_Finish(struct rte_crypto_op* op);

/**
 * @brief Data encoder optimized for traffic generator.
 *
 * DataGen* is struct rte_mbuf*.
 * Its packet buffer contains name suffix TLV-VALUE and fields after Name.
 * vlan_tci field indicates name suffix TLV-LENGTH.
 * Name TL and Data TL are not included.
 */
typedef struct DataGen
{
} DataGen;

/**
 * @brief Encode Data with DataGen template.
 * @param seg0 a uniquely owned, unsegmented, direct, empty mbuf.
 *             It must have @c DataGenDataroom buffer size.
 * @param seg1 segment 1 indirect mbuf. This is chained onto @p seg0 .
 * @return encoded packet, converted from @c seg0 .
 */
__attribute__((nonnull, returns_nonnull)) Packet*
DataGen_Encode(DataGen* gen, struct rte_mbuf* seg0, struct rte_mbuf* seg1, LName prefix);

#endif // NDNDPDK_NDNI_DATA_H
