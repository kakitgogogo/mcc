#include <inttypes.h>
#include "encode.h"
#include "error.h"
#include "buffer.h"

/*
utf-8 编码：

码点范围                码点位数	字节1	        字节2	      字节3	    字节4
U+0000 ~ U+007F	        7	    0xxxxxxx			
U+0080 ~ U+07FF	        11	    110xxxxx	10xxxxxx		
U+0800 ~ U+FFFF	        16	    1110xxxx	10xxxxxx	10xxxxxx	
U+10000 ~ U+10FFFF	    21	    11110xxx	10xxxxxx	10xxxxxx	10xxxxxx

*/

// return number of bytes, r record code-point
int decode_utf8(uint32_t& r, char* s, char* end) { 
    int bytes = 8;
    for(int i = 7; i >= 0; --i) {
        if((s[0] & (1 << i)) == 0) {
            bytes = 7 - i;
            break;
        }
    }

    if(bytes == 0) {
        r = s[0];
        return 1;
    }

    if(s + bytes > end) {
        error("invalid UTF-8 sequence: unexpected end");
    }
    for(int i = 1; i < bytes; ++i) {
        if((s[i] & 0xC0) != 0x80) {
            error("invalid UTF-8 continuation byte");
        }
    }

    switch(bytes) {
    case 2:
        r = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    case 3:
        r = ((s[0] & 0xF) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    case 4:
        r = ((s[0] & 0x7) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    error("invalid UTF-8 sequence");
    return bytes;
}

void encode_utf8(Buffer& buf, uint32_t u) {
    if(u <= 0x7F) 
        buf.write(u & 0xFF);
    else if(u <= 0x7FF) {
        buf.write(0xC0 | ((u >> 6) & 0xFF));
        buf.write(0x80 | ( u       & 0x3F));
    }
    else if(u <= 0xFFFF) {
        buf.write(0xE0 | ((u >> 12) & 0xFF));
        buf.write(0x80 | ((u >>  6) & 0x3F));
        buf.write(0x80 | ( u        & 0x3F));
    }
    else if(u <= 0x10FFFF) {
        buf.write(0xF0 | ((u >> 18) & 0xFF));
        buf.write(0x80 | ((u >> 12) & 0x3F));
        buf.write(0x80 | ((u >>  6) & 0x3F));
        buf.write(0x80 | ( u        & 0x3F));
    }
    else {
        error("invalid UCS character: \\U%08x", u);
    }
}

/*
utf-16 编码：
1. U+D800 ~ U+DFFF（代理区）
2. U+0000 ~ U+D7FF 以及 U+E000 ~ U+FFFF
    为单个16比特长的码元，直接编码
3. U+10000 ~ U+10FFFF
    码位减去0x10000，得到的值的范围为20比特长的0..0xFFFFF
    高位的10比特的值（值的范围为0..0x3FF）被加上0xD800得到第一个码元，
    低位的10比特的值（值的范围也是0..0x3FF）被加上0xDC00得到第二个码元，
    最终的UTF-16的编码就是：110110yyyyyyyyyy 110111xxxxxxxxxx
*/

static void write16(Buffer& buf, uint16_t x) {
    buf.write(x & 0xFF);
    buf.write(x >> 8);
}

static void write32(Buffer& buf, uint32_t x) {
    write16(buf, x & 0xFFFF);
    write16(buf, x >> 16);
}

// utf8 -> utf16
Buffer encode_utf16(char* s, int len) {
    Buffer buf;
    char* end = s + len;
    while(s != end) {
        uint32_t u;
        s += decode_utf8(u, s, end);
        if (u < 0x10000) {
            write16(buf, u);
        } else {
            write16(buf, (u >> 10) + 0xD7C0);
            write16(buf, (u & 0x3FF) + 0xDC00);
        }
    }
    return buf; // maybe rvo
}

// utf8 -> utf32
Buffer encode_utf32(char* s, int len) {
    Buffer buf;
    char* end = s + len;
    while(s != end) {
        uint32_t u;
        s += decode_utf8(u, s, end);
        write32(buf, u);
    }
    return buf;
}