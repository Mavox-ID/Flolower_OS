#ifndef UTF8_H
#define UTF8_H

static inline int utf8_decode(const unsigned char *p, unsigned int *out_cp) {
    if (p[0] < 0x80) {
        *out_cp = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        *out_cp = ((unsigned int)(p[0] & 0x1F) << 6) | (unsigned int)(p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        *out_cp = ((unsigned int)(p[0] & 0x0F) << 12) | ((unsigned int)(p[1] & 0x3F) << 6) | (unsigned int)(p[2] & 0x3F);
        return 3;
    }
    *out_cp = '?';
    return 1;
}

#endif
