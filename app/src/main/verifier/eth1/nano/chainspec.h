// @PUBLIC_HEADER
/** @file
 * Ethereum chain specification
 * */

#ifndef in3_eth_chainspec_h__
#define in3_eth_chainspec_h__
#include "../../../core/util/data.h"
#include "../../../core/util/error.h"
#include <stdint.h>

#define BLOCK_LATEST 0xFFFFFFFFFFFFFFFF

/**
 * defines the flags for the current activated EIPs.
 * Since it does not make sense to support a evm defined before Homestead, homestead EIP is always turned on!
 * 
 */
typedef struct __attribute__((__packed__)) eip_ {

  unsigned int eip140 : 1;  /**< REVERT instruction */
  unsigned int eip145 : 1;  /**< Bitwise shifting instructions in EVM */
  unsigned int eip150 : 1;  /**< Gas cost changes for IO-heavy operations */
  unsigned int eip155 : 1;  /**< Simple replay attack protection */
  unsigned int eip160 : 1;  /**< EXP cost increase */
  unsigned int eip170 : 1;  /**< Contract code size limit */
  unsigned int eip196 : 1;  /**< Precompiled contracts for addition and scalar multiplication on the elliptic curve alt_bn128	 */
  unsigned int eip197 : 1;  /**< Precompiled contracts for optimal ate pairing check on the elliptic curve alt_bn128	*/
  unsigned int eip198 : 1;  /**< Big integer modular exponentiation		*/
  unsigned int eip211 : 1;  /**< New opcodes: RETURNDATASIZE and RETURNDATACOPY	*/
  unsigned int eip214 : 1;  /**< New opcode STATICCALL	 */
  unsigned int eip658 : 1;  /**< Embedding transaction status code in receipts */
  unsigned int eip1014 : 1; /**< Skinny CREATE2	 */
  unsigned int eip1052 : 1; /**< EXTCODEHASH opcode */
  unsigned int eip1283 : 1; /**< Net gas metering for SSTORE without dirty maps	 */

} eip_t;

/** the consensus type.
 * 
*/
typedef enum {
  ETH_POW        = 0, /**< Pro of Work (Ethash) */
  ETH_POA_AURA   = 1, /**< Proof of Authority using Aura */
  ETH_POA_CLIQUE = 2  /**< Proof of Authority using clique */
} eth_consensus_type_t;

typedef struct transition_eip_ {
  uint64_t transition_block;
  eip_t    eips;
} eip_transition_t;

typedef struct transition_consensus_ {
  uint64_t             transition_block;
  eth_consensus_type_t type;
  bytes_t              validators;
  uint8_t*             contract;
} consensus_transition_t;

typedef struct chainspec_ {
  uint64_t                network_id;
  uint64_t                account_start_nonce;
  uint32_t                eip_transitions_len;
  eip_transition_t*       eip_transitions;
  uint32_t                consensus_transitions_len;
  consensus_transition_t* consensus_transitions;
} chainspec_t;

chainspec_t*            chainspec_create_from_json(d_token_t* data);
eip_t                   chainspec_get_eip(chainspec_t* spec, uint64_t block_number);
consensus_transition_t* chainspec_get_consensus(chainspec_t* spec, uint64_t block_number);
int                     chainspec_to_bin(chainspec_t* spec, bytes_builder_t* bb);
chainspec_t*            chainspec_from_bin(void* raw);
chainspec_t*            chainspec_get(uint64_t chain_id);
void                    chainspec_put(uint64_t chain_id, chainspec_t* spec);

#endif // in3_eth_chainspec_h__
