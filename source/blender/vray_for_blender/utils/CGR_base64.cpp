#include "CGR_base64.h"
#include "stdio.h"


static u_int8_t * createEncodeTable(const char b62, const char b63) {
    static u_int8_t table[64];
    static int initialized = false;
    if (!initialized) {
        for (u_int8_t i = 0; i < 26; i++) {
            table[i] = 'A' + i;
            table[26+i] = 'a' + i;
        }
        for (u_int8_t i = 0; i < 10; i++)
            table[52+i] = '0' + i;
        initialized = true;
    }
    table[62] = b62;
    table[63] = b63;
    return table;
}


int base64_getEncodedBufferMaxSize(const int binaryDataSize) {
    int byteTriplets = binaryDataSize/3;
    if (binaryDataSize%3) {
        byteTriplets += 1;
    }
    return (byteTriplets * 4);
}


void base64_encode(const u_int8_t *data, const int dataLen, u_int8_t *encoded, const char b62, const char b63, const char pad) {
    if (data == NULL || dataLen <= 0) return;
    u_int8_t *table = createEncodeTable(b62, b63);
    int padding = (dataLen % 3) ? (3 - dataLen%3) : 0;
    int block;
    int offset;
    for (block = 0; block < dataLen / 3; block++) {
        u_int32_t bytes = (u_int32_t)data[block * 3] << 16;
        bytes |= (u_int32_t)data[block * 3 + 1] << 8;
        bytes |= (u_int32_t)data[block * 3 + 2];
        offset = block * 4;
        encoded[offset  ] = table[(bytes >> 18) & 0x3f];
        encoded[offset+1] = table[(bytes >> 12) & 0x3f];
        encoded[offset+2] = table[(bytes >> 6 ) & 0x3f];
        encoded[offset+3] = table[ bytes        & 0x3f];
    }
    if (padding) {
        u_int32_t bytes = (u_int32_t) data[block * 3] << 16;
        if (padding == 1)
            bytes |= (u_int32_t) data[1 + block * 3] << 8;
        offset = block * 4;
        encoded[offset  ] = table[(bytes >> 18) & 0x3f];
        encoded[offset+1] = table[(bytes >> 12) & 0x3f];
        encoded[offset+2] = table[(bytes >> 6 ) & 0x3f];
        encoded[offset+3] = pad;
        if (padding == 2)
            encoded[offset+2] = pad;
    }
}
