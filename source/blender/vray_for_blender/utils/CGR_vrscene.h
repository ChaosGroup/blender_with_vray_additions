#ifndef CGR_VRSCENE_H
#define CGR_VRSCENE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_sys_types.h"

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void  GetDoubleHex(float f, char *str);
void  GetFloatHex(float f, char *buf);
char* GetTransformHex(float m[4][4]);

char* GetStringZip(const u_int8_t *buf, unsigned bufLen);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CGR_VRSCENE_H
