#ifndef CGR_BASE64_H
#define CGR_BASE64_H

#include "CGR_config.h"


/// Helper function that calculates encoded buffer size based on the size of input binary data.
/// @return Returned decoded buffer size is large enought to hold encoded string plus padding characters if required.
int base64_getEncodedBufferMaxSize(int binaryDataSize);

/// Encodes binary data into ASCII string. Binary bytes are iterated in chunks of 3 which are converted into 4 bytes of characters.
/// The coding CAN'T be done inplace direclty on the binary buffer because 3 bytes are written into 4 bytes and binary data will be corrupted.
/// Use separate buffer that will hold the ASCII string.
/// Note that the function will not write closing NULL character to the resulting string.
/// @param data - Input encoded data. Size of input data must be multiple of 4.
/// @param dataLen - Length of input data to be decoded in bytes.
/// @param encoded - Pointer to the buffer that will receive decoded binary data. Both data and decoded can point to same memory.
/// @param b62 - Optional character to override the default value for the 62-th byte in encoding table.
/// @param b63 - Optional character to override the default value for the63-th byte in encoding table.
/// @param pad - Optional character to override the default value for the padding byte.
void base64_encode(const u_int8_t *data, int dataLen, u_int8_t *encoded, char b62='+', char b63='/', char pad='=');

#endif // CGR_BASE64_H
