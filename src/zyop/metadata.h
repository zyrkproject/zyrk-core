#ifndef METADATA__H_
#define METADATA__H_

#include <arith_uint256.h>
#include <uint256.h>
#include <serialize.h>

namespace zyop {

class SpendMetaData {
public:

    MetaData(
        const arith_uint256& accumulatorId,
        const uint256& blockHash,
        const uint256& txHash);

    arith_uint256 accumulatorId; 

    uint256 blockHash;

    uint256 txHash; 
	ADD_SERIALIZE_METHODS;
	template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
		READWRITE(accumulatorId);
		READWRITE(blockHash);
		READWRITE(txHash);
	}
};

} 

#endif