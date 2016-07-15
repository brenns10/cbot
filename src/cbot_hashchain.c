/***************************************************************************//**

  @file         cbot.c

  @author       Stephen Brennan

  @date         Created Wednesday, 14 July 2016

  @brief        CBot Hash Chain Implementation

  @copyright    Copyright (c) 2016, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/evp.h>

#include "cbot_private.h"

/**
   @brief Verify that h comes directly before tip in a hash chain.
   @param h Pointer to a hash.
   @param tip Pointer to the "tip" hash.
   @param hash Hash algorithm to use.
   @param True if hash(h) == tip
*/
static bool hash_chain_verify(const void *h, const void *tip, const EVP_MD *hash)
{
  EVP_MD_CTX *ctx;
  int result;
  int digest_len = EVP_MD_size(hash);
  void *data = malloc(digest_len);

  ctx = EVP_MD_CTX_create();
  EVP_DigestInit_ex(ctx, hash, NULL);
  EVP_DigestUpdate(ctx, h, digest_len);
  EVP_DigestFinal_ex(ctx, data, NULL);
  EVP_MD_CTX_destroy(ctx);

  result = memcmp(data, tip, digest_len);
  free(data);

  return result == 0;
}

/**
   @brief Base64 decode a string. You must free the return value.
   @param str Some base64 encoded data.
   @param explen The expected length of the data you're reading.
   @returns Newly allocated pointer to buffer of length explen.
*/
void *base64_decode(const char *str, int explen)
{
  uint8_t *buf = malloc(explen);
  BIO *b = BIO_new_mem_buf(str, -1);
  BIO *b64 = BIO_new(BIO_f_base64());
  BIO_push(b64, b);
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO_read(b64, buf, explen);
  BIO_free_all(b64);
  return buf;
}

int cbot_is_authorized(cbot_t *cbot, const char *message)
{
  const EVP_MD *hash = EVP_get_digestbyname("sha1");
  int digest_len = EVP_MD_size(hash);
  void *qhash = base64_decode(message, digest_len);
  if (!qhash) {
    return 0;
  }
  if (hash_chain_verify(qhash, cbot->hash, hash)) {
    memcpy(cbot->hash, qhash, digest_len);
    free(qhash);
    int i = 0;
    // get through the hash
    while (message[i] && !isspace(message[i])) {
      i++;
    }
    // get through all whitespace
    while (message[i] && isspace(message[i])) {
      i++;
    }
    return i;
  } else {
    free(qhash);
    return 0;
  }
}
