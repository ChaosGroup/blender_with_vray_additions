// MurmurHash3 was written by Austin Appleby, and is placed in the
// public domain. The author hereby disclaims copyright to this source
// code.
//
// Taken from https://github.com/PeterScott/murmur3
//

#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

#include "BLI_sys_types.h"

typedef u_int32_t MHash;

void MurmurHash3_x86_32 (const void *key, int len, u_int32_t seed, void *out);
void MurmurHash3_x86_128(const void *key, int len, u_int32_t seed, void *out);
void MurmurHash3_x64_128(const void *key, int len, u_int32_t seed, void *out);

#endif // _MURMURHASH3_H_
