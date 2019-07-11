#include "./eth_api.h"
#include "../core/client/context.h"
#include "../core/client/keys.h"
#include "../core/util/log.h"
#include "../eth_basic/filter.h"
#include "../eth_nano/rlp.h"
#include "abi.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

// create the params as stringbuilder
#define rpc_init sb_t* params = sb_new("[");

// execute the request after the params have been set.
#define rpc_exec(METHOD, RETURN_TYPE, HANDLE_RESULT)                                      \
  errno              = 0;                                                                 \
  in3_ctx_t*  ctx    = in3_client_rpc_ctx(in3, (METHOD), sb_add_char(params, ']')->data); \
  d_token_t*  result = get_result(ctx);                                                   \
  RETURN_TYPE res;                                                                        \
  if (result)                                                                             \
    res = (HANDLE_RESULT);                                                                \
  else                                                                                    \
    memset(&res, 0, sizeof(RETURN_TYPE));                                                 \
  free_ctx(ctx);                                                                          \
  sb_free(params);                                                                        \
  return res;

// last error string
static char* last_error = NULL;
char*        eth_last_error() { return last_error; }

// sets the error and a message
static void set_errorn(int std_error, char* msg, int len) {
  errno = std_error;
  if (last_error) _free(last_error);
  last_error = _malloc(len + 1);
  memcpy(last_error, msg, len);
  last_error[len] = 0;
}

// sets the error and a message
static void set_error(int std_error, char* msg) {
  in3_log_error("Request failed due to %s - %s", strerror(std_error), msg);
  set_errorn(std_error, msg, strlen(msg));
}

/** copies bytes to a fixed length destination (leftpadding 0 if needed).*/
static void copy_fixed(uint8_t* dst, uint32_t len, bytes_t data) {
  if (data.len > len)
    memcpy(dst, data.data + data.len - len, len);
  else if (data.len == len)
    memcpy(dst, data.data, len);
  else if (data.len) {
    memcpy(dst + len - data.len, data.data, data.len);
    memset(dst, 0, len - data.len);
  } else
    memset(dst, 0, len);
}

uint256_t to_uint256(uint64_t value) {
  uint256_t data;
  memset(data.data, 0, 32);
  long_to_bytes(value, data.data + 24);
  return data;
}

/** converts a uint256 to a long double */
long double as_double(uint256_t d) {
  uint8_t* p = d.data;
  int      l = 32;
  optimize_len(p, l);
  if (l < 9)
    return bytes_to_long(p, l);
  else {
    long double val = 6277101735386680763835789423207666416102355444464034512896.0L * bytes_to_long(d.data, 8);
    val += 340282366920938463463374607431768211456.0L * bytes_to_long(d.data + 8, 8);
    val += 18446744073709551616.0L * bytes_to_long(d.data + 16, 8);
    return val + bytes_to_long(d.data + 24, 8);
  }
}
/** converts a uint256_t to a int (by taking the last 8 bytes) */
uint64_t as_long(uint256_t d) {
  return bytes_to_long(d.data + 32 - 8, 8);
}

/** creates a uin256_t from a flexible byte */
static uint256_t uint256_from_bytes(bytes_t bytes) {
  uint256_t d;
  copy_fixed(d.data, 32, bytes);
  return d;
}

/** returns the result from a previously executed ctx*/
static d_token_t* get_result(in3_ctx_t* ctx) {
  d_token_t* res = d_get(ctx->responses[0], K_RESULT);
  if (res) return res;                                // everthing is good, we have a result
  if (ctx->error)                                     // error means something went wrong during verification or a timeout occured.
    set_error(ETIMEDOUT, ctx->error);                 // so we copy the error as last_error
  else {                                              // but since we did not get a result and even without a error
    d_token_t* r = d_get(ctx->responses[0], K_ERROR); // we find the error in the response from the server
    if (d_type(r) == T_OBJECT) {                      // the response was correct but contains a error-object, which we convert into a string
      str_range_t s = d_to_json(r);                   // this will not work, if we used binary-format, since we don't know the propnames in this case!!!
      set_errorn(ETIMEDOUT, s.data, s.len);           // set error as json
    } else                                            // or we have a string
      set_errorn(ETIMEDOUT, d_string(r), d_len(r));   // and can simply copy it
  }
  return NULL;
}

/** adds a number as hex */
static void params_add_number(sb_t* sb, uint64_t num) {
  char tmp[30];
  if (sb->len > 1) sb_add_char(sb, ',');
  sprintf(tmp, "\"0x%" PRIx64 "\"", num);
  sb_add_chars(sb, tmp);
}

/** adding blocknumber, where bn==0 means 'latest' */
static void params_add_blocknumber(sb_t* sb, uint64_t bn) {
  if (bn)
    params_add_number(sb, bn);
  else {
    if (sb->len > 1) sb_add_char(sb, ',');
    sb_add_chars(sb, "\"latest\"");
  }
}

/** add ther raw bytes as hex*/
static void params_add_bytes(sb_t* sb, bytes_t data) {
  if (sb->len > 1) sb_add_char(sb, ',');
  sb_add_bytes(sb, "", &data, 1, false);
}

/** add a booleans as true or false */
static void params_add_bool(sb_t* sb, bool val) {
  if (sb->len > 1) sb_add_char(sb, ',');
  sb_add_chars(sb, val ? "true" : "false");
}

/** copy the data from the token to a eth_tx_t-object */
static uint32_t write_tx(d_token_t* t, eth_tx_t* tx) {
  bytes_t b             = d_to_bytes(d_get(t, K_INPUT));
  tx->signature[64]     = d_get_intk(t, K_V);
  tx->block_number      = d_get_longk(t, K_NUMBER);
  tx->gas               = d_get_longk(t, K_GAS);
  tx->gas_price         = d_get_longk(t, K_GAS_PRICE);
  tx->nonce             = d_get_longk(t, K_NONCE);
  tx->data              = bytes((uint8_t*) tx + sizeof(eth_tx_t), b.len);
  tx->transaction_index = d_get_intk(t, K_TRANSACTION_INDEX);
  memcpy(tx + sizeof(eth_tx_t), b.data, b.len); // copy the data right after the tx-struct.
  copy_fixed(tx->block_hash, 32, d_to_bytes(d_getl(t, K_BLOCK_HASH, 32)));
  copy_fixed(tx->from, 20, d_to_bytes(d_getl(t, K_FROM, 20)));
  copy_fixed(tx->to, 20, d_to_bytes(d_getl(t, K_TO, 20)));
  copy_fixed(tx->value.data, 32, d_to_bytes(d_getl(t, K_VALUE, 32)));
  copy_fixed(tx->hash, 32, d_to_bytes(d_getl(t, K_HASH, 32)));
  copy_fixed(tx->signature, 32, d_to_bytes(d_getl(t, K_R, 32)));
  copy_fixed(tx->signature + 32, 32, d_to_bytes(d_getl(t, K_S, 32)));

  return sizeof(eth_tx_t) + b.len;
}

/** calculate the tx size as struct+data */
static uint32_t get_tx_size(d_token_t* tx) { return d_to_bytes(d_get(tx, K_INPUT)).len + sizeof(eth_tx_t); }

/** 
 * allocates memory for the block and all required lists like the transactions and copies the data.
 * 
 * this allocates more memory than just the block-struct, but does it with one malloc!
 * so the structure looksm like this:
 * 
 * struct {
 *   eth_block_t block;
 *   uint8_t[]   extra_data;
 *   bytes_t[]   seal_fields;
 *   uint8_t[]   seal_fields bytes-datas;
 *   eth_tx_t[]  transactions including their data;
 * }
 */
static eth_block_t* eth_getBlock(d_token_t* result, bool include_tx) {
  if (result) {
    if (d_type(result) == T_NULL)
      set_error(EAGAIN, "Block does not exist");
    else {
      d_token_t* sealed = d_get(result, K_SEAL_FIELDS);
      d_token_t* txs    = d_get(result, K_TRANSACTIONS);
      bytes_t    extra  = d_to_bytes(d_get(result, K_EXTRA_DATA));

      // calc size
      uint32_t s = sizeof(eth_block_t);
      if (include_tx) {
        for (d_iterator_t it = d_iter(txs); it.left; d_iter_next(&it))
          s += get_tx_size(it.token); // add all struct-size for each transaction
      } else                          // or
        s += 32 * d_len(txs);         // just the transaction hashes
      s += extra.len;                 // extra-data
      for (d_iterator_t sf = d_iter(sealed); sf.left; d_iter_next(&sf)) {
        bytes_t t = d_to_bytes(sf.token);
        rlp_decode(&t, 0, &t);
        s += t.len + sizeof(bytes_t); // for each field in the selad-fields we need a bytes_t-struct in the array + the data itself
      }

      // copy data
      eth_block_t* b = malloc(s);
      if (!b) {
        set_error(ENOMEM, "Not enough memory");
        return NULL;
      }
      uint8_t* p = (uint8_t*) b + sizeof(eth_block_t); // pointer where we add the next data after the block-struct
      copy_fixed(b->author, 20, d_to_bytes(d_getl(result, K_AUTHOR, 20)));
      copy_fixed(b->difficulty.data, 32, d_to_bytes(d_get(result, K_DIFFICULTY)));
      copy_fixed(b->hash, 32, d_to_bytes(d_getl(result, K_HASH, 32)));
      copy_fixed(b->logsBloom, 256, d_to_bytes(d_getl(result, K_LOGS_BLOOM, 256)));
      copy_fixed(b->parent_hash, 32, d_to_bytes(d_getl(result, K_PARENT_HASH, 32)));
      copy_fixed(b->transaction_root, 32, d_to_bytes(d_getl(result, K_TRANSACTIONS_ROOT, 32)));

      d_token_t* t = NULL;
      if ((t = d_getl(result, K_RECEIPT_ROOT, 32)) || (t = d_getl(result, K_RECEIPTS_ROOT, 32)))
        copy_fixed(b->receipts_root, 32, d_to_bytes(t));

      copy_fixed(b->sha3_uncles, 32, d_to_bytes(d_getl(result, K_SHA3_UNCLES, 32)));
      copy_fixed(b->state_root, 32, d_to_bytes(d_getl(result, K_STATE_ROOT, 32)));
      b->gasLimit          = d_get_longk(result, K_GAS_LIMIT);
      b->gasUsed           = d_get_longk(result, K_GAS_USED);
      b->number            = d_get_longk(result, K_NUMBER);
      b->timestamp         = d_get_longk(result, K_TIMESTAMP);
      b->tx_count          = d_len(txs);
      b->seal_fields_count = d_len(sealed);
      b->extra_data        = bytes(p, extra.len);
      memcpy(p, extra.data, extra.len);
      p += extra.len;
      b->seal_fields = (void*) p;
      p += sizeof(bytes_t) * b->seal_fields_count;
      for (d_iterator_t sf = d_iter(sealed); sf.left; d_iter_next(&sf)) {
        bytes_t t = d_to_bytes(sf.token);
        rlp_decode(&t, 0, &t);
        b->seal_fields[b->seal_fields_count - sf.left] = bytes(p, t.len);
        memcpy(p, t.data, t.len);
        p += t.len;
      }

      b->tx_data   = include_tx ? (eth_tx_t*) p : NULL;
      b->tx_hashes = include_tx ? NULL : (bytes32_t*) p;

      for (d_iterator_t it = d_iter(txs); it.left; d_iter_next(&it)) {
        if (include_tx)
          p += write_tx(it.token, (eth_tx_t*) p);
        else {
          copy_fixed(p, 32, d_to_bytes(it.token));
          p += 32;
        }
      }
      return b;
    }
  }
  return NULL;
}

eth_block_t* eth_getBlockByNumber(in3_t* in3, uint64_t number, bool include_tx) {
  rpc_init;
  params_add_blocknumber(params, number);
  params_add_bool(params, include_tx);
  rpc_exec("eth_getBlockByNumber", eth_block_t*, eth_getBlock(result, include_tx));
}

eth_block_t* eth_getBlockByHash(in3_t* in3, bytes32_t number, bool include_tx) {
  rpc_init;
  params_add_bytes(params, bytes(number, 32));
  params_add_bool(params, include_tx);
  rpc_exec("eth_getBlockByHash", eth_block_t*, eth_getBlock(result, include_tx));
}

uint256_t eth_getBalance(in3_t* in3, address_t account, uint64_t block) {
  rpc_init;
  params_add_bytes(params, bytes(account, 20));
  params_add_blocknumber(params, block);
  rpc_exec("eth_getBalance", uint256_t, uint256_from_bytes(d_to_bytes(result)));
}
bytes_t eth_getCode(in3_t* in3, address_t account, uint64_t block) {
  rpc_init;
  params_add_bytes(params, bytes(account, 20));
  params_add_blocknumber(params, block);
  rpc_exec("eth_getCode", bytes_t, cloned_bytes(d_to_bytes(result)));
}
uint256_t eth_getStorageAt(in3_t* in3, address_t account, bytes32_t key, uint64_t block) {
  rpc_init;
  params_add_bytes(params, bytes(account, 20));
  params_add_bytes(params, bytes(key, 32));
  params_add_blocknumber(params, block);
  rpc_exec("eth_getStorageAt", uint256_t, uint256_from_bytes(d_to_bytes(result)));
}

uint64_t eth_blockNumber(in3_t* in3) {
  rpc_init;
  rpc_exec("eth_blockNumber", uint64_t, d_long(result));
}

uint64_t eth_gasPrice(in3_t* in3) {
  rpc_init;
  rpc_exec("eth_gasPrice", uint64_t, d_long(result));
}

static eth_log_t* parse_logs(d_token_t* result) {
  eth_log_t *prev, *first;
  prev = first = NULL;
  for (d_iterator_t it = d_iter(result); it.left; d_iter_next(&it)) {
    eth_log_t* log         = _calloc(1, sizeof(*log));
    log->removed           = d_get_intk(it.token, K_REMOVED);
    log->log_index         = d_get_intk(it.token, K_LOG_INDEX);
    log->transaction_index = d_get_intk(it.token, K_TRANSACTION_INDEX);
    log->block_number      = d_get_longk(it.token, K_BLOCK_NUMBER);
    log->data.len          = d_len(d_get(it.token, K_DATA));
    log->data.data         = _malloc(sizeof(uint8_t) * log->data.len);
    log->topics            = _malloc(sizeof(bytes32_t) * d_len(d_get(it.token, K_TOPICS)));
    copy_fixed(log->address, 20, d_to_bytes(d_getl(it.token, K_ADDRESS, 20)));
    copy_fixed(log->transaction_hash, 32, d_to_bytes(d_getl(it.token, K_TRANSACTION_HASH, 32)));
    copy_fixed(log->block_hash, 32, d_to_bytes(d_getl(it.token, K_BLOCK_HASH, 32)));
    copy_fixed(log->data.data, log->data.len, d_to_bytes(d_get(it.token, K_DATA)));
    size_t i = 0;
    for (d_iterator_t t = d_iter(d_getl(it.token, K_TOPICS, 32)); t.left; d_iter_next(&t), i++) {
      copy_fixed(log->topics[i], 32, d_to_bytes(t.token));
      log->topic_count += 1;
    }
    log->next = NULL;
    if (first == NULL)
      first = log;
    else if (prev != NULL)
      prev->next = log;
    prev = log;
  }
  return first;
}

eth_log_t* eth_getLogs(in3_t* in3, char* fopt) {
  rpc_init;
  sb_add_chars(params, fopt);
  rpc_exec("eth_getLogs", eth_log_t*, parse_logs(result));
}

static json_ctx_t* parse_call_result(call_request_t* req, d_token_t* result) {
  json_ctx_t* res = req_parse_result(req, d_to_bytes(result));
  req_free(req);
  return res;
}

json_ctx_t* eth_call_fn(in3_t* in3, address_t contract, char* fn_sig, ...) {
  rpc_init;
  int             res = 0;
  call_request_t* req = parseSignature(fn_sig);
  if (req->in_data->type == A_TUPLE) {
    json_ctx_t* in_data = json_create();
    d_token_t*  args    = json_create_array(in_data);
    va_list     ap;
    va_start(ap, fn_sig);
    var_t* p = req->in_data + 1;
    for (int i = 0; i < req->in_data->type_len; i++, p = t_next(p)) {
      switch (p->type) {
        case A_BOOL:
          json_array_add_value(args, json_create_bool(in_data, va_arg(ap, int)));
          break;
        case A_ADDRESS:
          json_array_add_value(args, json_create_bytes(in_data, bytes(va_arg(ap, uint8_t*), 20)));
          break;
        case A_BYTES:
          json_array_add_value(args, json_create_bytes(in_data, va_arg(ap, bytes_t)));
          break;
        case A_STRING:
          json_array_add_value(args, json_create_string(in_data, va_arg(ap, char*)));
          break;
        case A_INT:
        case A_UINT: {
          if (p->type_len <= 4)
            json_array_add_value(args, json_create_int(in_data, va_arg(ap, uint32_t)));
          else if (p->type_len <= 8)
            json_array_add_value(args, json_create_int(in_data, va_arg(ap, uint64_t)));
          else
            json_array_add_value(args, json_create_bytes(in_data, bytes(va_arg(ap, uint256_t).data, 32)));
          break;
        }
        default:
          req->error = "unsuported token-type!";
          res        = -1;
      }
    }
    va_end(ap);

    if ((res = set_data(req, args, req->in_data)) < 0) req->error = "could not set the data";
    free_json(in_data);
  }
  if (res >= 0) {
    bytes_t to = bytes(contract, 20);
    sb_add_chars(params, "{\"to\":");
    sb_add_bytes(params, "", &to, 1, false);
    sb_add_chars(params, ", \"data\":");
    sb_add_bytes(params, "", &req->call_data->b, 1, false);
    sb_add_char(params, '}');
  } else {
    set_error(0, req->error ? req->error : "Error parsing the request-data");
    sb_free(
        params);
    req_free(req);
    return NULL;
  }

  if (res >= 0) {
    rpc_exec("eth_call", json_ctx_t*, parse_call_result(req, result));
  }
  return NULL;
}

static char* wait_for_receipt(in3_t* in3, char* params, int timeout, int count) {
  errno             = 0;
  in3_ctx_t* ctx    = in3_client_rpc_ctx(in3, "eth_getTransactionReceipt", params);
  d_token_t* result = get_result(ctx);
  if (result) {
    if (d_type(result) == T_NULL) {
      free_ctx(ctx);
      if (count) {
#if defined(_WIN32) || defined(WIN32)
        Sleep(timeout);
#else
        nanosleep((const struct timespec[]){{timeout / 1000, ((long) timeout % 1000) * 1000000L}}, NULL);
#endif
        return wait_for_receipt(in3, params, timeout + timeout, count - 1);
      } else {
        set_error(1, "timeout waiting for the receipt");
        return NULL;
      }
    } else {
      //
      char* c = d_create_json(result);
      free_ctx(ctx);
      return c;
    }
  }
  free_ctx(ctx);
  set_error(3, ctx->error ? ctx->error : "Error getting the Receipt!");
  return NULL;
}

char* eth_wait_for_receipt(in3_t* in3, bytes32_t tx_hash) {
  rpc_init;
  params_add_bytes(params, bytes(tx_hash, 32));
  char* data = wait_for_receipt(in3, sb_add_char(params, ']')->data, 1000, 8);
  sb_free(params);
  return data;
}

in3_ret_t eth_newFilter(in3_t* in3, json_ctx_t* options) {
  if (options == NULL || !filter_opt_valid(&options->result[0])) return IN3_EINVAL;
  char*     fopt = d_create_json(&options->result[0]);
  in3_ret_t res  = filter_add(in3, FILTER_EVENT, fopt);
  if (res < 0) _free(fopt);
  return res;
}

in3_ret_t eth_newBlockFilter(in3_t* in3) {
  return filter_add(in3, FILTER_BLOCK, NULL);
}

in3_ret_t eth_newPendingTransactionFilter(in3_t* in3) {
  return filter_add(in3, FILTER_PENDING, NULL);
}

bool eth_uninstallFilter(in3_t* in3, size_t id) {
  return filter_remove(in3, id);
}

in3_ret_t eth_getFilterChanges(in3_t* in3, size_t id, bytes32_t** block_hashes, eth_log_t** logs) {
  if (in3->filters == NULL)
    return IN3_EFIND;
  if (id == 0 || id > in3->filters->count)
    return IN3_EINVAL;

  uint64_t      blkno = eth_blockNumber(in3);
  in3_filter_t* f     = in3->filters->array[id - 1];
  switch (f->type) {
    case FILTER_EVENT:
      *logs         = eth_getLogs(in3, f->options);
      f->last_block = blkno + 1;
      return 0;
    case FILTER_BLOCK:
      if (blkno > f->last_block) {
        uint64_t blkcount = blkno - f->last_block;
        *block_hashes     = malloc(sizeof(bytes32_t) * blkcount);
        for (uint64_t i = f->last_block + 1, j = 0; i <= blkno; i++, j++) {
          eth_block_t* blk = eth_getBlockByNumber(in3, i, false);
          memcpy((*block_hashes)[j], blk->hash, 32);
          free(blk);
        }
        f->last_block = blkno;
        return (int) blkcount;
      } else {
        *block_hashes = NULL;
        return 0;
      }
    default:
      return IN3_ENOTSUP;
  }
}

void free_log(eth_log_t* log) {
  _free(log->data.data);
  _free(log->topics);
  _free(log);
}
