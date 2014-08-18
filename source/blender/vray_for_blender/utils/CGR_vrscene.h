#ifndef CGR_VRSCENE_H
#define CGR_VRSCENE_H

#include "CGR_config.h"

#include <Python.h>


void  GetDoubleHex(float f, char *buf);
void  GetFloatHex(float f, char *buf);
void  GetVectorHex(float f[3], char *buf);
void  GetTransformHex(float m[4][4], char *buf);

char* GetHex(const u_int8_t *buf, unsigned bufLen);
char* GetStringZip(const u_int8_t *buf, unsigned bufLen);
char* GetFloatArrayZip(float *data, size_t size);

int   GetPythonAttrInt(PyObject *propGroup, const char *attrName, int def=0);
float GetPythonAttrFloat(PyObject *propGroup, const char *attrName, float def=0.0f);

#endif // CGR_VRSCENE_H
