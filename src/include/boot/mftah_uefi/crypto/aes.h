/*
 * From: tiny-AES-c, at https://github.com/kokke/tiny-AES-c
 *
 * This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <http://unlicense.org/>
 *
 */

/* NOTE: AES-256-CBC DECRYPT is explicitly used. Any other implementation is trimmed. */


#ifndef AES_H
#define AES_H



#include <stdint.h>
#include <stddef.h>


/* Block length in bytes. 128-bit blocks only. */
#define AES_BLOCKLEN 16
#define AES_KEYLEN 32
#define AES_keyExpSize 240

typedef struct AES_ctx {
    uint8_t RoundKey[AES_keyExpSize];
    uint8_t Iv[AES_BLOCKLEN];
} aes_ctx_t;


void
AES_init_ctx_iv(
    struct AES_ctx *ctx,
    const uint8_t  *key,
    const uint8_t  *iv
);

void
AES_ctx_set_iv(
    struct AES_ctx *ctx,
    const uint8_t  *iv
);


/*
 * The buffer size MUST be a mutiple of AES_BLOCKLEN.
 * NOTES:
 *   - Need to set IV in ctx via AES_init_ctx_iv() or AES_ctx_set_iv()
 *   - No IV should ever be reused with the same key 
 */
void
AES_CBC_decrypt_buffer(
    struct AES_ctx *ctx,
    uint8_t        *buf,
    size_t         length,
    void           (*progress)(const uint64_t*, const uint64_t*, void*),
    void           *progress_extra
);



#endif   /* AES_H */
