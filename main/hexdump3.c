#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

/*
 * 구버전은 strcat(strDump[64], ...)로 HEX를 누적해 버퍼 오버런이 가능했고,
 * 이는 TLSF 힙 손상·StoreProhibited로 이어질 수 있음.
 */
void hexdump3(char *title, void *pack, size_t size)
{
    const unsigned char *dump = (const unsigned char *)pack;

    if (size == 0 || pack == NULL) {
        return;
    }

    printf("***** \x1B[35m%s\x1B[0m \x1B[32m%zu\x1B[0m bytes *****\r\n",
           (title == NULL) ? "None" : title, size);

    for (size_t base = 0; base < size; base += 16) {
        char hexpart[72];
        char asc[20];
        int hp = 0;

        for (size_t j = 0; j < 16 && base + j < size; j++) {
            unsigned char c = dump[base + j];
            int n = snprintf(hexpart + hp, sizeof(hexpart) - (size_t)hp, "%02X ", c);
            if (n < 0) {
                break;
            }
            hp += n;
            if (j == 3 || j == 7 || j == 11) {
                n = snprintf(hexpart + hp, sizeof(hexpart) - (size_t)hp, " ");
                if (n < 0) {
                    break;
                }
                hp += n;
            }
        }

        int ap = 0;
        for (size_t j = 0; j < 16 && base + j < size; j++) {
            unsigned char c = dump[base + j];
            asc[ap++] = (c > 0x1F && c < 0x7F) ? (char)c : '.';
        }
        asc[ap] = '\0';

        printf("<0x%04zX> %s%s\r\n", base, hexpart, asc);
    }

    printf("\r\n");
}
