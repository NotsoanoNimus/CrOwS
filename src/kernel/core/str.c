#include "core/str.h"

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>



int
strlen(const char *str)
{
    if (NULL == str) return 0;

    int len = 0;
    for (; *str; ++str, ++len);

    return len;
}


int
snprintf(char *str,
         int limit,
         const char *fmt,
         ...)
{
    va_list args;
    int cursor = 0;
    int cursor_into = 0;

    int leading_zeros = 0;
    char *str_to_print;

    const char *s;
    unsigned char c;
    unsigned int hex;
    unsigned int u;
    int d;

    va_start(args, fmt);

    while (fmt[cursor] && cursor_into < limit) {
        if ('%' == fmt[cursor]) {
            leading_zeros = 0;
            str_to_print = NULL;

            ++cursor;

        snprintf__RepeatEvaluation:
            switch (fmt[cursor]) {
                case '%':
                    str[cursor_into] = '%';
                    ++cursor_into;
                    break;

                case '0':
                    ++cursor;
                    /* The value is multiplied by 10 here in case the condition is called again. */
                    while ('0' > fmt[cursor] || '9' < fmt[cursor]) {
                        leading_zeros = (leading_zeros * 10) + (fmt[cursor] - '0');
                        ++cursor;
                    }
                    goto snprintf__RepeatEvaluation;   /* check the following character */

                case 's':
                    s = va_arg(args, const char *);
                    str_to_print = (char *)s;
                    break;

                case 'c':
                    c = va_arg(args, int);   /* Note: implicit cast to `unsigned char` works here. */
                    str[cursor_into] = (char)c;
                    ++cursor_into;
                    str_to_print = NULL;
                    break;

                case 'x':
                case 'X':
                    // TODO: Add Pointer 'p' type here?
                    hex = va_arg(args, unsigned long long);

                    if (0 == hex) { str_to_print = "0"; break; }

                    char hex_as_str[16 + 1];
                    for (int i = 0; i < 9; ++i) hex_as_str[i] = 0;

                    for (unsigned int x = hex, i = 0; x > 0; x >>= 4, ++i) {
                        unsigned int rem = (x % 16);
                        hex_as_str[16 - 1 - i] = ((rem > 9) ? ((('X' == fmt[cursor] || 'P' == fmt[cursor]) ? 'A' : 'a') + (rem - 10)) : ('0' + rem));
                        str_to_print = &(hex_as_str[16 - 1 - i]);
                    }

                    break;

                case 'u':
                    u = va_arg(args, unsigned int);

                    if (0 == u) { str_to_print = "0"; break; }

                    char u_as_str[10 + 1];   /* '10' is the max width of a 32-bit number as a string */
                    for (int i = 0; i < 11; ++i) u_as_str[i] = 0;

                    for (unsigned int x = u, i = 0; x > 0; x /= 10, ++i) {
                        u_as_str[10 - 1 - i] = ('0' + (x % 10));
                        str_to_print = &(u_as_str[10 - 1 - i]);
                    }

                    break;

                case 'd':
                    d = va_arg(args, int);

                    if (0 == d) { str_to_print = "0"; break; }

                    char d_as_str[11 + 1];   /* '11' is the max width of a 32-bit number as a string with a '-' */
                    for (int i = 0; i < 11; ++i) d_as_str[i] = 0;

                    unsigned int d_at;
                    for (int x = d, i = 0; x > 0; x /= 10, ++i) {
                        d_at = (d < 0 ? 11 : 10) - 1 - i;
                        d_as_str[d_at] = ('0' + (x % 10));
                        str_to_print = &(d_as_str[d_at]);
                    }

                    if (d < 0) {
                        d_as_str[d_at - 1] = '-';
                        str_to_print = &(d_as_str[d_at - 1]);
                    }

                    break;

                default:
                    va_end(args);
                    return -1;   /* unsupported format token */
            }

            if (NULL != str_to_print) {
                // if ('s' != fmt[cursor] && 'c' != fmt[cursor]) {
                //     /* Account for possible leading zeros with numeric tokens. */
                //     for (int x = 0; x < (int)(leading_zeros - strlen(str_to_print)) && cursor_into < limit; ++x, ++cursor_into) {
                //         str[cursor_into] = '0';
                //     }
                // }

                for (int m = 0; m < strlen(str_to_print) && cursor_into < limit; ++m, ++cursor_into) {
                    str[cursor_into] = str_to_print[m];
                }
            }

            ++cursor;
        }
        
        else {
            str[cursor_into] = fmt[cursor];
            ++cursor;
            ++cursor_into;
        }
    }

    va_end(args);
    return cursor_into;
}
