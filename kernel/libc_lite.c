#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) { while (n--) *d++ = *s++; }
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++)) { }
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char *)s; s++; }
    return (c == 0) ? (char *)s : NULL;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) { if (*s == (char)c) last = s; s++; }
    if (c == 0) return (char *)s;
    return (char *)last;
}

double strtod(const char *s, char **endptr) {
    const char *p = s;
    while (*p == ' ') p++;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; } else if (*p == '+') p++;
    double val = 0;
    while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; }
    if (*p == '.') {
        p++;
        double scale = 0.1;
        while (*p >= '0' && *p <= '9') { val += (*p - '0') * scale; scale *= 0.1; p++; }
    }
    val *= sign;
    if (endptr) *endptr = (char *)p;
    return val;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *arr = (unsigned char *)base;
    unsigned char tmp[512];
    if (size > sizeof(tmp)) return;
    for (size_t i = 1; i < nmemb; i++) {
        memcpy(tmp, arr + i * size, size);
        size_t j = i;
        while (j > 0 && compar(arr + (j - 1) * size, tmp) > 0) {
            memcpy(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }
        memcpy(arr + j * size, tmp, size);
    }
}

static int vsscanf_lite(const char *str, const char *fmt, va_list ap) {
    int count = 0;
    const char *s = str;
    const char *f = fmt;
    while (*f) {
        if (*f == '%') {
            f++;
            if (*f == 'd') {
                while (*s == ' ') s++;
                int sign = 1;
                if (*s == '-') { sign = -1; s++; } else if (*s == '+') s++;
                if (*s < '0' || *s > '9') break;
                int val = 0;
                while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
                *va_arg(ap, int *) = val * sign;
                count++;
                f++;
            } else if (*f == 'u') {
                while (*s == ' ') s++;
                if (*s < '0' || *s > '9') break;
                unsigned val = 0;
                while (*s >= '0' && *s <= '9') { val = val * 10 + (unsigned)(*s - '0'); s++; }
                *va_arg(ap, unsigned *) = val;
                count++;
                f++;
            } else if (*f == 's') {
                while (*s == ' ') s++;
                char *out = va_arg(ap, char *);
                while (*s && *s != ' ' && *s != '\n') *out++ = *s++;
                *out = '\0';
                count++;
                f++;
            } else if (*f == 'c') {
                char *out = va_arg(ap, char *);
                *out = *s++;
                count++;
                f++;
            } else {
                f++;
            }
        } else if (*f == ' ') {
            while (*s == ' ') s++;
            f++;
        } else {
            if (*s != *f) break;
            s++; f++;
        }
    }
    return count;
}

int sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf_lite(str, fmt, ap);
    va_end(ap);
    return r;
}

int __isoc99_sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf_lite(str, fmt, ap);
    va_end(ap);
    return r;
}

static void out_char(char *buf, size_t size, size_t *pos, char c) {
    if (size > 0 && *pos < size - 1) buf[*pos] = c;
    (*pos)++;
}

static void write_uint(char *buf, size_t size, size_t *pos, unsigned long long val,
                        int base, int width, char pad, bool upper) {
    char tmp[32];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (val == 0) tmp[n++] = '0';
    while (val > 0) { tmp[n++] = digits[val % (unsigned)base]; val /= (unsigned)base; }
    while (n < width) tmp[n++] = pad;
    for (int i = n - 1; i >= 0; i--) out_char(buf, size, pos, tmp[i]);
}

static void write_int(char *buf, size_t size, size_t *pos, long long val, int width, char pad) {
    if (val < 0) {
        out_char(buf, size, pos, '-');
        write_uint(buf, size, pos, (unsigned long long)(-val), 10, width > 0 ? width - 1 : 0, pad, false);
    } else {
        write_uint(buf, size, pos, (unsigned long long)val, 10, width, pad, false);
    }
}

static int count_int_digits(unsigned long long v) {
    int n = 1;
    while (v >= 10) { v /= 10; n++; }
    return n;
}

static void write_g(char *buf, size_t size, size_t *pos, double val, int sig) {
    bool neg = val < 0;
    if (neg) val = -val;
    unsigned long long ip = (unsigned long long)val;
    int idigits = count_int_digits(ip);
    int decimals = sig - idigits;
    if (decimals < 0) decimals = 0;
    if (decimals > 15) decimals = 15;

    char tmp[64];
    size_t tp = 0;
    if (neg) tmp[tp++] = '-';
    {
        char idig[24]; int n = 0;
        unsigned long long t = ip;
        if (t == 0) idig[n++] = '0';
        while (t > 0) { idig[n++] = (char)('0' + (t % 10)); t /= 10; }
        for (int i = n - 1; i >= 0; i--) tmp[tp++] = idig[i];
    }
    if (decimals > 0) {
        tmp[tp++] = '.';
        double frac = val - (double)ip;
        for (int i = 0; i < decimals; i++) {
            frac *= 10;
            int d = (int)frac;
            tmp[tp++] = (char)('0' + d);
            frac -= d;
        }
        while (tp > 0 && tmp[tp - 1] == '0') tp--;
        if (tp > 0 && tmp[tp - 1] == '.') tp--;
    }
    for (size_t i = 0; i < tp; i++) out_char(buf, size, pos, tmp[i]);
}

static void write_f(char *buf, size_t size, size_t *pos, double val, int precision) {
    if (val < 0) { out_char(buf, size, pos, '-'); val = -val; }
    unsigned long long ip = (unsigned long long)val;
    double frac = val - (double)ip;
    write_uint(buf, size, pos, ip, 10, 0, '0', false);
    if (precision > 0) {
        out_char(buf, size, pos, '.');
        for (int i = 0; i < precision; i++) {
            frac *= 10;
            int d = (int)frac;
            out_char(buf, size, pos, (char)('0' + d));
            frac -= d;
        }
    }
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t pos = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { out_char(buf, size, &pos, *p); continue; }
        p++;
        if (*p == '\0') break;
        if (*p == '%') { out_char(buf, size, &pos, '%'); continue; }

        char pad = ' ';
        int width = 0;
        bool has_precision = false;
        int precision = 0;
        bool dyn_precision = false;

        if (*p == '0') { pad = '0'; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        if (*p == '.') {
            p++;
            has_precision = true;
            if (*p == '*') { dyn_precision = true; p++; }
            else { while (*p >= '0' && *p <= '9') { precision = precision * 10 + (*p - '0'); p++; } }
        }

        bool is_long_long = false;
        if (p[0] == 'l' && p[1] == 'l') { is_long_long = true; p += 2; }
        else if (*p == 'l') { p++; }

        if (dyn_precision) precision = va_arg(ap, int);

        switch (*p) {
            case 'd': case 'i': {
                long long v = is_long_long ? va_arg(ap, long long) : va_arg(ap, int);
                write_int(buf, size, &pos, v, width, pad);
                break;
            }
            case 'u': {
                unsigned long long v = is_long_long ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned int);
                write_uint(buf, size, &pos, v, 10, width, pad, false);
                break;
            }
            case 'x': case 'X': {
                unsigned long long v = is_long_long ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned int);
                write_uint(buf, size, &pos, v, 16, width, pad, *p == 'X');
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                out_char(buf, size, &pos, c);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                int n = 0;
                while (s[n]) n++;
                if (has_precision && precision < n) n = precision;
                for (int i = 0; i < n; i++) out_char(buf, size, &pos, s[i]);
                break;
            }
            case 'f': {
                double v = va_arg(ap, double);
                write_f(buf, size, &pos, v, has_precision ? precision : 6);
                break;
            }
            case 'g': case 'G': {
                double v = va_arg(ap, double);
                write_g(buf, size, &pos, v, has_precision ? precision : 6);
                break;
            }
            default:
                out_char(buf, size, &pos, '%');
                out_char(buf, size, &pos, *p);
                break;
        }
    }
    if (size > 0) buf[pos < size ? pos : size - 1] = '\0';
    return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}
