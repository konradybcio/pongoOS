#include <stdarg.h>
#include <stdint.h>

#define min(a,b) __extension__\
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a < _b ? _a : _b; })

enum flag_itoa {
    FILL_ZERO = 1,
    PUT_PLUS = 2,
    PUT_MINUS = 4,
    BASE_2 = 8,
    BASE_10 = 16,
};

static char * sitoa(char * buf, unsigned int num, int width, enum flag_itoa flags)
{
    unsigned int base;
    if (flags & BASE_2)
        base = 2;
    else if (flags & BASE_10)
        base = 10;
    else
        base = 16;

    char tmp[32];
    char *p = tmp;
    do {
        int rem = num % base;
        *p++ = (rem <= 9) ? (rem + '0') : (rem + 'a' - 0xA);
    } while ((num /= base));
    width -= p - tmp;
    char fill = (flags & FILL_ZERO)? '0' : ' ';
    while (0 <= --width) {
        *(buf++) = fill;
    }
    if (flags & PUT_MINUS)
        *(buf++) = '-';
    else if (flags & PUT_PLUS)
        *(buf++) = '+';
    do
        *(buf++) = *(--p);
    while (tmp < p);
    return buf;
}
void barcode_space_narrow();
void barcode_space_wide();
void barcode_stripe_narrow();
void barcode_stripe_wide();
int my_vsprintf(char * buf, const char * fmt, va_list va)
{
    char c;
    const char *save = buf;

    while ((c  = *fmt++)) {
        int width = 0;
        enum flag_itoa flags = 0;
        if (c != '%') {
            *(buf++) = c;
            continue;
        }
    redo_spec:
        c  = *fmt++;

        switch (c) {
        case '%':
            *(buf++) = c;
            break;
        case 'c':;
            *(buf++) = va_arg(va, int);
            break;
        case 'd':;
            int num = va_arg(va, int);
            if (num < 0) {
                num = -num;
                flags |= PUT_MINUS;
            }
            buf = sitoa(buf, num, width, flags | BASE_10);
            break;
        case 'u':
            buf = sitoa(buf, va_arg(va, unsigned int), width, flags | BASE_10);
            break;
        case 'x':
            buf = sitoa(buf, va_arg(va, unsigned int), width, flags);
            break;
        case 'b':
            buf = sitoa(buf, va_arg(va, unsigned int), width, flags | BASE_2);
            break;
        case 's':;
            const char *p  = va_arg(va, const char *);
            if (p) {
                while (*p)
                    *(buf++) = *(p++);
            }
            break;
        case 'm':;
            const uint8_t *m  = va_arg(va, const uint8_t *);
            width = min(width, 64); // buffer limited to 256!
            if (m)
                for (;;) {
                    buf = sitoa(buf, *(m++), 2, FILL_ZERO);
                    if (--width <= 0)
                        break;
                    *(buf++) = ':';
                }
            break;
        case '0':
            if (!width)
                flags |= FILL_ZERO;
            // fall through
        case '1'...'9':
            width = width * 10 + c - '0';
            goto redo_spec;
        case '*':
            width = va_arg(va, unsigned int);
            goto redo_spec;
        case '+':
            flags |= PUT_PLUS;
            goto redo_spec;
        case '\0':
        default:
            *(buf++) = '?';
        }
        width = 0;
    }
    *buf = '\0';
    return buf - save;
}

int my_sprintf(char * buf, const char * fmt, ...)
{
    va_list va;
    va_start(va,fmt);
    int ret = my_vsprintf(buf, fmt, va);
    va_end(va);
    return ret;
}

#if TEST
int main(int argc, char *argv[])
{
    char b[256], *p = b;
    my_sprintf(b, "%x %d %b\n", 123, 123, 123);
    while (*p)
        putchar(*p++);
}
#endif