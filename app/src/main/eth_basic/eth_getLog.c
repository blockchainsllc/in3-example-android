
#include "../eth_nano/eth_nano.h"
#include "../eth_nano/merkle.h"
#include "../eth_nano/rlp.h"
#include "../eth_nano/serialize.h"
#include "eth_basic.h"
#include "trie.h"
#include <client/context.h>
#include <client/keys.h>
#include <string.h>
#include <util/data.h>
#include <util/log.h>
#include <util/mem.h>
#include <util/utils.h>

typedef struct receipt {
  bytes32_t tx_hash;
  bytes_t   data;
  bytes_t   block_number;
  bytes32_t block_hash;
  uint32_t  transaction_index;
} receipt_t;

static bool matches_filter_address(d_token_t* tx_params, bytes_t addrs) {
  d_token_t* jaddrs = d_getl(tx_params, K_ADDRESS, 20);
  if (jaddrs == NULL) {
    return true; // address param is optional
  } else if (d_type(jaddrs) == T_BYTES) {
    return !!bytes_cmp(addrs, d_to_bytes(jaddrs));
  } else if (d_type(jaddrs) == T_ARRAY) { // must match atleast one in array
    int l = d_len(jaddrs);
    for (int i = 0; i < l; i++, jaddrs = d_next(jaddrs)) {
      if (bytes_cmp(addrs, d_to_bytes(jaddrs))) return true;
    }
  }
  return false;
}

static bool matches_filter_from_to(d_token_t* tx_params, const d_key_t k, uint64_t blockno) {
  d_token_t* jrange = d_get(tx_params, k);
  if (d_type(jrange) == T_INTEGER || d_type(jrange) == T_BYTES) {
    if (k == K_FROM_BLOCK)
      return blockno >= d_long(jrange);
    else if (k == K_TO_BLOCK)
      return blockno <= d_long(jrange);
  }
  return true;
}
static bool matches_filter_range(d_token_t* tx_params, uint64_t blockno, bytes_t blockhash) {
  d_token_t* jblkhash = d_getl(tx_params, K_BLOCK_HASH, 32);
  if (jblkhash == NULL)
    // check block from/to
    return matches_filter_from_to(tx_params, K_FROM_BLOCK, blockno) && matches_filter_from_to(tx_params, K_TO_BLOCK, blockno);
  else if (d_type(jblkhash) == T_BYTES)
    // checl blockhash
    return !!bytes_cmp(blockhash, d_to_bytes(jblkhash));
  else
    // we have a blockhash-property, which is not a bytes-type
    return false;
  return true;
}

static bool matches_filter_topics(d_token_t* tx_params, d_token_t* topics) {
  d_token_t* jts = d_get(tx_params, K_TOPICS);
  int        l   = d_len(jts);
  if (jts == NULL)
    return true; // topics param is optional
  else if (d_type(jts) != T_ARRAY)
    return false; // Unlikely
  else if (l == 0)
    return true; // [] matches anything
  else if (d_type(topics) != T_ARRAY || d_len(topics) > 4 || l > d_len(topics))
    return false;

  d_iterator_t it1 = d_iter(jts), it2 = d_iter(topics);
  for (int i = 0; i < l; i++, d_iter_next(&it1), d_iter_next(&it2)) {
    if (d_type(it1.token) == T_NULL)
      continue; // null matches anything in this position
    else if (d_type(it1.token) == T_BYTES && !bytes_cmp(d_to_bytes(it1.token), d_to_bytes(it2.token)))
      return false;
    else if (d_type(it1.token) == T_ARRAY) { // must match atleast one in array
      bool found = false;
      for (d_iterator_t it_ = d_iter(it1.token); it_.left; d_iter_next(&it_)) {
        if (d_type(it_.token) != T_BYTES) {
          return false;
        } else if (bytes_cmp(d_to_bytes(it_.token), d_to_bytes(it2.token))) {
          found = true;
          break;
        }
      }
      if (!found) return false;
    }
  }
  return true;
}

bool matches_filter(d_token_t* req, bytes_t addrs, uint64_t blockno, bytes_t blockhash, d_token_t* topics) {
  d_token_t* tx_params = d_get(req, K_PARAMS);
  if (!tx_params || d_type(tx_params + 1) != T_OBJECT) return false;
  if (!matches_filter_address(tx_params + 1, addrs)) {
    in3_log_error("filter address mismatch");
    return false;
  } else if (!matches_filter_range(tx_params + 1, blockno, blockhash)) {
    in3_log_error("filter range mismatch");
    return false;
  } else if (!matches_filter_topics(tx_params + 1, topics)) {
    in3_log_error("filter topics mismatch");
    return false;
  } else {
    return true;
  }
}

in3_ret_t eth_verify_eth_getLog(in3_vctx_t* vc, int l_logs) {
  in3_ret_t res = IN3_OK, i = 0;
  receipt_t receipts[l_logs];
  bytes_t   logddata, tmp, tops;

  // invalid result-token
  if (!vc->result || d_type(vc->result) != T_ARRAY) return vc_err(vc, "The result must be an array");
  // no results -> nothing to verify
  if (l_logs == 0) return IN3_OK;
  // we require proof
  if (!vc->proof) return vc_err(vc, "no proof for logs found");

  for (d_iterator_t it = d_iter(d_get(vc->proof, K_LOG_PROOF)); it.left; d_iter_next(&it)) {

    // verify the blockheader of the log entry
    bytes_t block = d_to_bytes(d_get(it.token, K_BLOCK)), tx_root, receipt_root;
    int     bl    = i;
    if (!block.len || eth_verify_blockheader(vc, &block, NULL) < 0) return vc_err(vc, "invalid blockheader");
    sha3_to(&block, receipts[i].block_hash);
    rlp_decode(&block, 0, &block);
    if (rlp_decode(&block, BLOCKHEADER_RECEIPT_ROOT, &receipt_root) != 1) return vc_err(vc, "invalid receipt root");
    if (rlp_decode(&block, BLOCKHEADER_TRANSACTIONS_ROOT, &tx_root) != 1) return vc_err(vc, "invalid tx root");
    if (rlp_decode(&block, BLOCKHEADER_NUMBER, &receipts[i].block_number) != 1) return vc_err(vc, "invalid block number");
    receipts[i].data = bytes(NULL, 0);

    // verify all receipts
    for (d_iterator_t receipt = d_iter(d_get(it.token, K_RECEIPTS)); receipt.left; d_iter_next(&receipt)) {
      if (i == l_logs) return vc_err(vc, "too many receipts in the proof");
      receipt_t* r = receipts + i;
      if (i != bl) memcpy(r, receipts + bl, sizeof(receipt_t)); // copy blocknumber and blockhash
      i++;

      // verify tx data first
      r->transaction_index = d_get_intk(receipt.token, K_TX_INDEX);
      bytes_t** proof      = d_create_bytes_vec(d_get(receipt.token, K_TX_PROOF));
      bytes_t*  path       = create_tx_path(r->transaction_index);

      if (!proof || !trie_verify_proof(&tx_root, path, proof, &r->data))
        res = vc_err(vc, "invalid tx merkle proof");
      if (proof) _free(proof);
      if (res != IN3_OK) {
        if (path) b_free(path);
        return res;
      }

      // check txhash

      sha3_to(&r->data, r->tx_hash);
      if (!bytes_cmp(d_to_bytes(d_getl(receipt.token, K_TX_HASH, 32)), bytes(r->tx_hash, 32))) {
        if (path) b_free(path);
        return vc_err(vc, "invalid tx hash");
      }

      // verify receipt data
      proof   = d_create_bytes_vec(d_get(receipt.token, K_PROOF));
      r->data = bytes(NULL, 0);

      if (!proof || !trie_verify_proof(&receipt_root, path, proof, &r->data))
        res = vc_err(vc, "invalid receipt proof");
      if (proof) _free(proof);
      if (path) b_free(path);
      if (res != IN3_OK) return res;
    }
  }

  if (i != l_logs) return vc_err(vc, "invalid receipts len in proof");
  for (d_iterator_t it = d_iter(vc->result); it.left; d_iter_next(&it)) {
    receipt_t* r = NULL;
    i            = 0;
    for (int n = 0; n < l_logs; n++) {
      if (bytes_cmp(d_to_bytes(d_get(it.token, K_TRANSACTION_HASH)), bytes(receipts[n].tx_hash, 32))) {
        r = receipts + n;
        break;
      }
    }

    if (!r) return vc_err(vc, "missing proof for log");
    d_token_t* topics = d_get(it.token, K_TOPICS);
    rlp_decode(&r->data, 0, &r->data);

    // verify the log-data
    if (rlp_decode(&r->data, 3, &logddata) != 2) return vc_err(vc, "invalid log-data");
    if (rlp_decode(&logddata, d_get_intk(it.token, K_TRANSACTION_LOG_INDEX), &logddata) != 2) return vc_err(vc, "invalid log index");

    // check address
    if (!rlp_decode(&logddata, 0, &tmp) || !bytes_cmp(tmp, d_to_bytes(d_getl(it.token, K_ADDRESS, 20)))) return vc_err(vc, "invalid address");
    if (!rlp_decode(&logddata, 2, &tmp) || !bytes_cmp(tmp, d_to_bytes(d_get(it.token, K_DATA)))) return vc_err(vc, "invalid data");
    if (rlp_decode(&logddata, 1, &tops) != 2) return vc_err(vc, "invalid topics");
    if (rlp_decode_len(&tops) != d_len(topics)) return vc_err(vc, "invalid topics len");

    for (d_iterator_t t = d_iter(topics); t.left; d_iter_next(&t)) {
      if (!rlp_decode(&tops, i++, &tmp) || !bytes_cmp(tmp, *d_bytesl(t.token, 32))) return vc_err(vc, "invalid topic");
    }

    if (d_get_longk(it.token, K_BLOCK_NUMBER) != bytes_to_long(r->block_number.data, r->block_number.len)) return vc_err(vc, "invalid blocknumber");
    if (!bytes_cmp(d_to_bytes(d_getl(it.token, K_BLOCK_HASH, 32)), bytes(r->block_hash, 32))) return vc_err(vc, "invalid blockhash");
    if (d_get_intk(it.token, K_REMOVED)) return vc_err(vc, "must be removed=false");
    if (d_get_intk(it.token, K_TRANSACTION_INDEX) != r->transaction_index) return vc_err(vc, "wrong transactionIndex");

    if (!matches_filter(vc->request, d_to_bytes(d_getl(it.token, K_ADDRESS, 20)), d_get_longk(it.token, K_BLOCK_NUMBER), d_to_bytes(d_getl(it.token, K_BLOCK_HASH, 32)), d_get(it.token, K_TOPICS))) return vc_err(vc, "filter mismatch");
  }

  return res;
}
