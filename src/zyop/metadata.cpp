#include <zyop/metadata.h>

namespace zyop {

MetaData::MetaData(
        const arith_uint256& accumulatorId,
        const uint256& blockHash,
        const uint256& txHash)
    : accumulatorId(accumulatorId)
    , blockHash(blockHash)
    , txHash(txHash)
{

}

}