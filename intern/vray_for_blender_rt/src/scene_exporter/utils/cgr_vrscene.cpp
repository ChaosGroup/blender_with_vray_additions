/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * * ***** END GPL LICENSE BLOCK *****
 */

#include "cgr_config.h"

#include "BLI_math.h"
#include "MEM_guardedalloc.h"

#include "cgr_vrscene.h"

#include <zlib.h>


struct TraceTransform {
    float  m[3][3];
    double v[3];
};


inline char letr2char(int d)
{
    if (d>=0 && d<26) return 'A'+d;
    d-=26;
    if (d>=0 && d<=9) return '0'+d;
    d-=10;
    if (d>=0 && d<26) return 'a'+d;
    return '?';
}


inline void word2str(u_int16_t w, char *str)
{
    int d0=w%41;
    w/=41;
    int d1=w%41;
    w/=41;
    int d2=w%41;
    *str=letr2char(d0);
    str++;
    *str=letr2char(d1);
    str++;
    *str=letr2char(d2);
}


static int hex2str(const u_int8_t *bytes, unsigned numBytes, char *str, unsigned strMaxLen)
{
    if (strMaxLen<numBytes*2/3+1)
        return -1;

    for (unsigned i=0; i<numBytes/2; i++) {
        word2str(*((u_int16_t*) (bytes+i*2)), str);
        str+=3;
    }
    if (numBytes&1) {
        u_int8_t b[2]={ bytes[numBytes-1], 0 };
        word2str(*((u_int16_t*) b), str);
        str+=3;
    }
    *str=0;

    return 0;
}


static char int2hexValue(char d)
{
    return d<=9 ? '0'+d : (d-10+'A');
}


static void int2hex(u_int32_t i, char *hex)
{
    u_int8_t f[4];
    *((u_int32_t*)&f)=i;
    for (int i=0; i<4; i++) {
        char val0=int2hexValue((f[i]>>4)&0xF);
        char val1=int2hexValue((f[i])&0xF);
        hex[i*2+0]=val0;
        hex[i*2+1]=val1;
    }
}


void getStringHex(const u_int8_t *buf, unsigned nBytes, char *pstr)
{
    for(unsigned i = 0; i < nBytes; ++i) {
        char val0 = int2hexValue((buf[i]>>4)&0xF);
        char val1 = int2hexValue(buf[i]&0xF);
        pstr[i*2+0] = val0;
        pstr[i*2+1] = val1;
    }
    pstr[nBytes*2] = 0;
}



char *GetHex(const u_int8_t *data, unsigned dataLen)
{
	char *buf = new char[dataLen*2+1];

	getStringHex(data, dataLen, buf);

	return buf;
}


char* GetStringZip(const u_int8_t *buf, unsigned bufLen)
{
    // First compress the data into a temporary buffer
    //
    uLongf  tempLen = bufLen+bufLen/10+12;
    char   *temp = new char[tempLen];

    int err = compress2((Bytef*)temp, &tempLen, (Bytef*)buf, bufLen, 1);
    if(err == Z_MEM_ERROR) {
        delete [] temp;
        return NULL;
    }
    if(err == Z_BUF_ERROR) {
        delete [] temp;
        return NULL;
    }

    // Convert the zipped buffer into an ASCII string
    //
    char     *pstr = NULL;
    unsigned  offset = 4+8+8;
    unsigned  strLen = (tempLen/2+1)*3+offset+1;

    pstr = new char[strLen];

    hex2str((u_int8_t*)temp, tempLen, &pstr[offset], strLen-offset);
    delete [] temp;

    // Add the Zip header
    pstr[0]='Z'; pstr[1]='I'; pstr[2]='P'; pstr[3]='B';
    pstr[strLen-1] = '\0';

    int2hex(bufLen, &pstr[4]);
    int2hex(tempLen, &pstr[8+4]);

    return pstr;
}


char* GetFloatArrayZip(float *data, size_t size)
{
	float  *ptr = new float[size];
	size_t  nBytes = size * sizeof(float);

	if(NOT(data))
		memset(ptr, 0, nBytes);
	else
		memcpy(ptr, data, nBytes);

	char *charBuf = GetStringZip((u_int8_t*)ptr, nBytes);

	delete [] ptr;

	return charBuf;
}


void GetDoubleHex(float f, char *buf)
{
    double d = double(f);
    const u_int8_t *d8 = (const u_int8_t*)&d;
    getStringHex(d8, sizeof(d), buf);
}


void GetFloatHex(float f, char *buf)
{
    float d = f;
    const u_int8_t *d8 = (const u_int8_t*)&d;
    getStringHex(d8, sizeof(d), buf);
}


void GetVectorHex(float f[3], char *buf)
{
	float d[3];
	copy_v3_v3(d, f);
	const u_int8_t *d8 = (const u_int8_t*)&d;
	getStringHex(d8, sizeof(d), buf);
}


void GetTransformHex(float m[4][4], char *buf)
{
    TraceTransform tm;
    copy_m3_m4(tm.m, m);
    copy_v3db_v3fl(tm.v, m[3]);

    const u_int8_t *tm8 = (const u_int8_t*)&tm;
    getStringHex(tm8, sizeof(tm), buf);
}


std::string GetTransformHex(const BLTransform &bl_tm)
{
	float m[4][4];
	::memcpy(m, bl_tm.data, 16 * sizeof(float));

	char buf[CGR_TRANSFORM_HEX_SIZE];
	GetTransformHex(m, buf);

	return buf;
}


int GetPythonAttrInt(PyObject *propGroup, const char *attrName, int def)
{
	PyObject *attr = PyObject_GetAttrString(propGroup, attrName);
	if(attr) {
		PyObject *value = PyNumber_Long(attr);
		if(PyNumber_Long(value))
			return PyLong_AsLong(value);
	}
	return def;
}


float GetPythonAttrFloat(PyObject *propGroup, const char *attrName, float def)
{
	PyObject *attr = PyObject_GetAttrString(propGroup, attrName);
	if(attr) {
		PyObject *value = PyNumber_Long(attr);
		if(PyNumber_Float(value))
			return PyLong_AsDouble(value);
	}
	return def;
}
