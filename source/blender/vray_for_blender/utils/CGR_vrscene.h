#ifndef CGR_VRSCENE_H
#define CGR_VRSCENE_H

#include "CGR_config.h"

#include "utils/murmur3.h"

#include <Python.h>


#define ATTR_BOOL  0
#define ATTR_INT   1
#define ATTR_FLOAT 2

#define TRANSFORM_HEX_SIZE  129


MHash HashCode(const char* s);
void  GetDoubleHex(float f, char *buf);
void  GetFloatHex(float f, char *buf);
void  GetVectorHex(float f[3], char *buf);
void  GetTransformHex(float m[4][4], char *buf);

char* GetStringZip(const u_int8_t *buf, unsigned bufLen);
char* GetFloatArrayZip(float *data, size_t size);

int   GetPythonAttrInt(PyObject *propGroup, const char *attrName, int def=0);
float GetPythonAttrFloat(PyObject *propGroup, const char *attrName, float def=0.0f);

#endif // CGR_VRSCENE_H
