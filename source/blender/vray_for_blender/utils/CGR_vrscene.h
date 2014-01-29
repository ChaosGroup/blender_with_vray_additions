#ifndef CGR_VRSCENE_H
#define CGR_VRSCENE_H

#include "CGR_config.h"
#include "utils/murmur3.h"


#define TRANSFORM_HEX_SIZE  129

MHash HashCode(const char* s);
void  GetDoubleHex(float f, char *buf);
void  GetFloatHex(float f, char *buf);
void  GetVectorHex(float f[3], char *buf);
void  GetTransformHex(float m[4][4], char *buf);

char* GetStringZip(const u_int8_t *buf, unsigned bufLen);

#endif // CGR_VRSCENE_H
