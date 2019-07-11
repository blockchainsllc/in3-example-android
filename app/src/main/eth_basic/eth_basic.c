#include "eth_basic.h"
#include "../eth_nano/eth_nano.h"
#include "../eth_nano/merkle.h"
#include "../eth_nano/rlp.h"
#include "../eth_nano/serialize.h"
#include "filter.h"
#include "signer.h"
#include <client/context.h>
#include <client/keys.h>
#include <crypto/ecdsa.h>
#include <crypto/secp256k1.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <util/data.h>
#include <util/mem.h>
#include <util/utils.h>

#define RESPONSE_START()                                                             \
  do {                                                                               \
    *response = _malloc(sizeof(in3_response_t));                                     \
    sb_init(&response[0]->result);                                                   \
    sb_init(&response[0]->error);                                                    \
    sb_add_chars(&response[0]->result, "{\"id\":1,\"jsonrpc\":\"2.0\",\"result\":"); \
  } while (0)

#define RESPONSE_END() \
  do { sb_add_char(&response[0]->result, '}'); } while (0)

in3_ret_t in3_verify_eth_basic(in3_vctx_t* vc) {
  char* method = d_get_stringk(vc->request, K_METHOD);

  // make sure we want to verify
  if (vc->config->verification == VERIFICATION_NEVER) return IN3_OK;

  // do we have a result? if not it is a vaslid error-response
  if (!vc->result || d_type(vc->result) == T_NULL) return IN3_OK;

  // do we support this request?
  if (!method) return vc_err(vc, "No Method in request defined!");

  if (strcmp(method, "eth_getTransactionByHash") == 0)
    // for txReceipt, we need the txhash
    return eth_verify_eth_getTransaction(vc, d_get_bytes_at(d_get(vc->request, K_PARAMS), 0));
  else if (strcmp(method, "eth_getBlockByNumber") == 0)
    // for txReceipt, we need the txhash
    return eth_verify_eth_getBlock(vc, NULL, d_get_long_at(d_get(vc->request, K_PARAMS), 0));
  else if (strcmp(method, "eth_getBlockByHash") == 0)
    // for txReceipt, we need the txhash
    return eth_verify_eth_getBlock(vc, d_get_bytes_at(d_get(vc->request, K_PARAMS), 0), 0);
  else if (strcmp(method, "eth_getBalance") == 0 || strcmp(method, "eth_getCode") == 0 || strcmp(method, "eth_getStorageAt") == 0 || strcmp(method, "eth_getTransactionCount") == 0)
    // for txReceipt, we need the txhash
    return eth_verify_account_proof(vc);
  else if (strcmp(method, "eth_gasPrice") == 0)
    return IN3_OK;
  else if (!strcmp(method, "eth_newFilter") || !strcmp(method, "eth_newBlockFilter") || !strcmp(method, "eth_newPendingFilter") || !strcmp(method, "eth_uninstallFilter") || !strcmp(method, "eth_getFilterChanges"))
    return IN3_OK;
  else if (strcmp(method, "eth_getLogs") == 0) // for txReceipt, we need the txhash
    return eth_verify_eth_getLog(vc, d_len(vc->result));
  else if (strcmp(method, "eth_sendRawTransaction") == 0) {
    bytes32_t hash;
    sha3_to(d_get_bytes_at(d_get(vc->request, K_PARAMS), 0), hash);
    return bytes_cmp(*d_bytes(vc->result), bytes(hash, 32)) ? IN3_OK : vc_err(vc, "the transactionHash of the response does not match the raw transaction!");
  } else
    return in3_verify_eth_nano(vc);
}

in3_ret_t eth_handle_intern(in3_ctx_t* ctx, in3_response_t** response) {
  if (ctx->len > 1) return IN3_ENOTSUP; // internal handling is only possible for single requests (at least for now)
  d_token_t* req = ctx->requests[0];

  // check method
  if (strcmp(d_get_stringk(req, K_METHOD), "eth_sendTransaction") == 0) {
    // get the transaction-object
    d_token_t* tx_params = d_get(req, K_PARAMS);
    if (!tx_params || d_type(tx_params + 1) != T_OBJECT) return ctx_set_error(ctx, "invalid params", IN3_EINVAL);
    if (!ctx->client->signer) return ctx_set_error(ctx, "no signer set", IN3_EINVAL);

    // sign it.
    bytes_t raw = sign_tx(tx_params + 1, ctx);
    if (!raw.len) return ctx_set_error(ctx, "error signing the transaction", IN3_EINVAL);

    // build the RPC-request
    uint64_t id = d_get_longk(req, K_ID);
    sb_t*    sb = sb_new("{ \"jsonrpc\":\"2.0\", \"method\":\"eth_sendRawTransaction\", \"params\":[");
    sb_add_bytes(sb, "", &raw, 1, false);
    sb_add_chars(sb, "]");
    if (id) {
      char tmp[16];

#ifdef __ZEPHYR__
      char bufTmp[21];
      snprintk(tmp, sizeof(tmp), ", \"id\":%s", u64tostr(id, bufTmp, sizeof(bufTmp)));
#else
      snprintf(tmp, sizeof(tmp), ", \"id\":%" PRId64 "", id);
      // sprintf(tmp, ", \"id\":%" PRId64 "", id);
#endif
      sb_add_chars(sb, tmp);
    }
    sb_add_chars(sb, "}");

    // now that we included the signature in the rpc-request, we can free it + the old rpc-request.
    _free(raw.data);
    free_json(ctx->request_context);

    // set the new RPC-Request.
    ctx->request_context = parse_json(sb->data);
    ctx->requests[0]     = ctx->request_context->result;

    // we add the request-string to the cache, to make sure the request-string will be cleaned afterwards
    ctx->cache = in3_cache_add_entry(ctx->cache, bytes(NULL, 0), bytes((uint8_t*) sb->data, sb->len));
  } else if (strcmp(d_get_stringk(req, K_METHOD), "eth_newFilter") == 0) {
    d_token_t* tx_params = d_get(req, K_PARAMS);
    if (!tx_params || d_type(tx_params + 1) != T_OBJECT)
      return ctx_set_error(ctx, "invalid type of params, expected object", IN3_EINVAL);
    else if (!filter_opt_valid(tx_params + 1))
      return ctx_set_error(ctx, "filter option parsing failed", IN3_EINVAL);
    if (!tx_params->data) return ctx_set_error(ctx, "binary request are not supported!", IN3_ENOTSUP);

    char*     fopt = d_create_json(tx_params + 1);
    in3_ret_t res  = filter_add(ctx->client, FILTER_EVENT, fopt);
    if (res < 0) {
      _free(fopt);
      return ctx_set_error(ctx, "filter creation failed", res);
    }

    RESPONSE_START();
    sb_add_char(&response[0]->result, '"');
    sb_add_hexuint(&response[0]->result, res);
    sb_add_char(&response[0]->result, '"');
    RESPONSE_END();
    return IN3_OK;
  } else if (strcmp(d_get_stringk(req, K_METHOD), "eth_newBlockFilter") == 0) {
    in3_ret_t res = filter_add(ctx->client, FILTER_BLOCK, NULL);
    if (res < 0) return ctx_set_error(ctx, "filter creation failed", res);

    RESPONSE_START();
    sb_add_char(&response[0]->result, '"');
    sb_add_hexuint(&response[0]->result, res);
    sb_add_char(&response[0]->result, '"');
    RESPONSE_END();
  } else if (strcmp(d_get_stringk(req, K_METHOD), "eth_newPendingTransactionFilter") == 0) {
    return ctx_set_error(ctx, "pending filter not supported", IN3_ENOTSUP);
  } else if (strcmp(d_get_stringk(req, K_METHOD), "eth_uninstallFilter") == 0) {
    d_token_t* tx_params = d_get(req, K_PARAMS);
    if (!tx_params || d_len(tx_params) == 0 || d_type(tx_params + 1) != T_INTEGER)
      return ctx_set_error(ctx, "invalid type of params, expected filter-id as integer", IN3_EINVAL);

    uint64_t id = d_get_long_at(tx_params, 0);
    RESPONSE_START();
    sb_add_chars(&response[0]->result, filter_remove(ctx->client, id) ? "true" : "false");
    RESPONSE_END();
  } else if (strcmp(d_get_stringk(req, K_METHOD), "eth_getFilterChanges") == 0) {
    d_token_t* tx_params = d_get(req, K_PARAMS);
    if (!tx_params || d_len(tx_params) == 0 || d_type(tx_params + 1) != T_INTEGER)
      return ctx_set_error(ctx, "invalid type of params, expected filter-id as integer", IN3_EINVAL);

    uint64_t id = d_get_long_at(tx_params, 0);
    RESPONSE_START();
    in3_ret_t ret = filter_get_changes(ctx, id, &response[0]->result);
    if (ret != IN3_OK)
      return ctx_set_error(ctx, "failed to get filter changes", ret);
    RESPONSE_END();
  }
  return IN3_OK;
}

void in3_register_eth_basic() {
  in3_verifier_t* v = _calloc(1, sizeof(in3_verifier_t));
  v->type           = CHAIN_ETH;
  v->pre_handle     = eth_handle_intern;
  v->verify         = in3_verify_eth_basic;
  in3_register_verifier(v);
}
