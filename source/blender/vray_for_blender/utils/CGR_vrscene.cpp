#include "BLI_math.h"
#include "MEM_guardedalloc.h"

#include "CGR_config.h"
#include "CGR_base64.h"
#include "CGR_vrscene.h"

#include <zlib.h>

#ifdef _WIN32
typedef unsigned char u_int8_t;
#endif


static char sbuf[MAX_PLUGIN_NAME];


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


inline int hex2str(const u_int8_t *bytes, unsigned numBytes, char *str, unsigned strMaxLen)
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
    for(int i = 0; i < 4; i++) {
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


// Checks if we should write the buffers in base64 or using the old format.
// By default it returns that base64 should be used.
#if CGR_USE_BASE64
static bool shouldWriteBase64Buffers() {
    static bool initialized=false;
    static bool flag=true;
    if (!initialized) {
        if (getenv("VRAY_WRITE_OLD_STYLE_BUFFERS")!=NULL)
            flag=false;
        initialized=true;
    }
    return flag;
}
#endif


char* GetStringZip(const u_int8_t *buf, unsigned bufLen)
{
    // First compress the data into a temporary buffer
    uLongf tempLen = bufLen+bufLen/10+12;
    char  *temp = (char*)malloc(tempLen);

    int res = compress((Bytef*)temp, &tempLen, (Bytef*)buf, bufLen);
    if(res == Z_MEM_ERROR) {
        free(temp);
        return NULL;
    }
    if(res == Z_BUF_ERROR) {
        free(temp);
        return NULL;
    }

    unsigned  offset=4+8+8;
    char     *pstr=NULL;

#if CGR_USE_BASE64
    if(!shouldWriteBase64Buffers()) {
#endif
        // Convert the zipped buffer into an ASCII string
        unsigned strLen=(tempLen/2+1)*3+offset;
        pstr = (char*)malloc(strLen);

        hex2str((u_int8_t*)temp, tempLen, &pstr[offset], strLen-offset);

        // Add the Zip header
        pstr[0]='Z'; pstr[1]='I'; pstr[2]='P'; pstr[3]='B';
#if CGR_USE_BASE64
    }
    else {
        // Convert the zipped buffer into a base64 encoded string
        unsigned strLen=base64_getEncodedBufferMaxSize(tempLen)+offset;
        pstr = (char*)malloc(strLen);

        base64_encode((const u_int8_t*)temp, tempLen, (u_int8_t*)&pstr[offset]);

        // Add the Zip header
        pstr[0]='Z'; pstr[1]='I'; pstr[2]='P'; pstr[3]='C';
    }
#endif

    free(temp);

    int2hex(bufLen, &pstr[4]);
    int2hex(tempLen, &pstr[8+4]);

    return pstr;
}


void GetDoubleHex(float f, char *str)
{
    double d = double(f);
    const u_int8_t *buf = (const u_int8_t*)&d;
    getStringHex(buf, sizeof(d), str);
}


void GetFloatHex(float f, char *str)
{
    float d = f;
    const u_int8_t *buf = (const u_int8_t*)&d;
    getStringHex(buf, sizeof(d), str);
}


char* GetTransformHex(float m[4][4])
{
    TraceTransform tm;
    copy_m3_m4(tm.m, m);
    copy_v3db_v3fl(tm.v, m[3]);

    const u_int8_t *buf = (const u_int8_t*)&tm;
    getStringHex(buf, sizeof(tm), sbuf);

    return sbuf;
}

