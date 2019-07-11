#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/mem.h>
#include <util/utils.h>
//#include <zephyr.h>

//#include "fsm.h"
#include "merkle.h"
#include "rlp.h"

static int nibble_len(uint8_t* a) {
  int i = 0;
  for (i = 0;; i++) {
    if (a[i] == 0xFF) return i;
  }
  return -1;
}

int trie_matching_nibbles(uint8_t* a, uint8_t* b) {
  int i = 0;
  for (i = 0;; i++) {
    if (a[i] == 0xff || b[i] == 0xff || a[i] != b[i]) return i;
  }
}

// converts the byte array to nibles of 4 bit each
uint8_t* trie_path_to_nibbles(bytes_t path, int use_prefix) {
  uint8_t* n = _malloc(1 + (path.len * 2));
  size_t   j = 0, i = 0;
  for (; i < path.len; i++) {
    n[j++] = path.data[i] >> 4;
    n[j++] = path.data[i] & 0x0F;
    if (i == 0 && use_prefix)
      n[0] = n[(j = n[0] & 1 ? 1 : 0)];
  }

  n[j] = 0xFF;
  return n;
}

static int check_node(bytes_t* raw_node, uint8_t** key, bytes_t* expectedValue, int is_last_node, bytes_t* last_value, uint8_t* next_hash, size_t* depth) {
  bytes_t node, val;
  (*depth)++;
  if (*depth > MERKLE_DEPTH_MAX)
    return 0;

  // decode the list into war values
  rlp_decode(raw_node, 0, &node);
  switch (rlp_decode_len(&node)) {

    case 17: // branch
      if (**key == 0xFF) {

        // if this is no the last node or the value is an embedded, which means more to come.
        if (!is_last_node || rlp_decode(&node, 16, &node) != 1)
          return 0;

        last_value->data = node.data;
        last_value->len  = node.len;
        return 1;
      }

      if (rlp_decode(&node, **key, &val) == 2) {
        rlp_decode(&node, (**key) - 1, &node);
        *key += 1;

        // we have an embedded node as next
        node.data += node.len;
        node.len = val.data + val.len - node.data;

        // check the embedded
        return check_node(&node, key, expectedValue, *(*key + 1) == 0xFF, last_value, next_hash, depth);

      } else if (val.len != 32) // no hash, so we make sure the next hash is an empty hash
        memset(next_hash, 0, 32);
      else
        memcpy(next_hash, val.data, 32);
      *key += 1;
      return 1;

    case 2: // leaf or extension
      if (rlp_decode(&node, 0, &val) != 1)
        return 0;
      else {
        uint8_t* path_nibbles  = trie_path_to_nibbles(val, 1);
        int      matching      = trie_matching_nibbles(path_nibbles, *key);
        int      node_path_len = nibble_len(path_nibbles);
        int      is_leaf       = val.data[0] & 32;
        _free(path_nibbles);

        // if the relativeKey in the leaf does not math our rest key, we throw!
        if (node_path_len != matching)
          // so we have a wrong leaf here, if we actually expected this node to not exist,
          // the last node in this path may be a different leaf or a branch with a empty hash
          return expectedValue == NULL && is_last_node;

        *key += node_path_len;
        if (rlp_decode(&node, 1, &val) == 2) { // this is an embedded node
          rlp_decode(&node, 0, &node);
          node.data += node.len;
          node.len = val.data + val.len - node.data;

          // check the embedded node
          return check_node(&node, key, expectedValue, *(key + 1) == NULL, last_value, next_hash, depth);

        } else if (**key == 0xFF) {
          // readed the end, if this is the last node, it is ok.
          if (!is_last_node) return 0;

          // if we are proven a value which shouldn't exist this must throw an error
          if (expectedValue == NULL && is_leaf)
            return 0;
        } else if (is_leaf && expectedValue != NULL)
          return 0;
      }

      // copy the leafs data as last_value and next_hash
      last_value->data = val.data;
      last_value->len  = val.len;
      memcpy(next_hash, val.data, (val.len >= 32) ? 32 : val.len);
      return 1;

    default: // empty node
      // only if we expect no value we accept a empty node as last node
      return (expectedValue == NULL && is_last_node);
  }
  return 1;
}

int trie_verify_proof(bytes_t* rootHash, bytes_t* path, bytes_t** proof, bytes_t* expectedValue) {
  int      res      = 1;
  uint8_t* full_key = trie_path_to_nibbles(*path, 0);
  uint8_t *key      = full_key, expected_hash[32], node_hash[32];
  bytes_t  last_value = {0};

  // start with root hash
  memcpy(expected_hash, rootHash->data, 32);

  size_t depth = 0;
  for (; *proof; proof += 1) {
    // create and check the hash of node
    if (!(res = sha3_to(*proof, node_hash) == 0 && memcmp(expected_hash, node_hash, 32) == 0)) break;
    // check embedded nodes and find the next expected hash
    if (!(res = check_node(*proof, &key, expectedValue, *(proof + 1) == NULL, &last_value, expected_hash, &depth))) break;
  }

  if (res && expectedValue != NULL) {
    // expectedValue->data == NULL means we simply set and return the value.
    if (expectedValue->data == NULL) {
      // do we have a value to set?
      if (last_value.data) {
        expectedValue->data = last_value.data;
        expectedValue->len  = last_value.len;
      }
      // if not we need ensure we have the same last value as expected.
    } else if (last_value.data == NULL || !b_cmp(expectedValue, &last_value))
      res = 0;
  }

  if (full_key) _free(full_key);
  return res;
}

void trie_free_proof(bytes_t** proof) {
  for (bytes_t** p = proof; *p; p += 1) b_free(*p);
  _free(proof);
}