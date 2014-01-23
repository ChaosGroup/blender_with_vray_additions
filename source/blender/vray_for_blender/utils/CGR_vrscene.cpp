#include "CGR_config.h"

#include "BLI_math.h"
#include "MEM_guardedalloc.h"

#include "CGR_base64.h"
#include "CGR_vrscene.h"

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


static void getStringHex(const u_int8_t *buf, unsigned nBytes, char *pstr)
{
    for(unsigned i = 0; i < nBytes; ++i) {
        char val0 = int2hexValue((buf[i]>>4)&0xF);
        char val1 = int2hexValue(buf[i]&0xF);
        pstr[i*2+0] = val0;
        pstr[i*2+1] = val1;
    }
    pstr[nBytes*2] = 0;
}


char* GetStringZip(const u_int8_t *buf, unsigned bufLen)
{
    // First compress the data into a temporary buffer
    //
    uLongf  tempLen = bufLen+bufLen/10+12;
    char   *temp = new char[tempLen];

    int err = compress((Bytef*)temp, &tempLen, (Bytef*)buf, bufLen);
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
