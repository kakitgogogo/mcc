#pragma once

#include <inttypes.h>
#include "buffer.h"

int decode_utf8(uint32_t& r, char* s, char* end);
void encode_utf8(Buffer& buf, uint32_t u);

Buffer encode_utf16(char* s, int len);

Buffer encode_utf32(char* s, int len);
