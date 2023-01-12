#include <cstdio>
#include <cstring>

unsigned int crc32_table[256];

void init_crc32_table()
{
    for (unsigned int i = 0; i < 256; i++)
    {
        unsigned int c = i;
        for (unsigned int j = 0; j < 8; j++)
        {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
}

unsigned int crc32(const void *data, size_t n_bytes)
{
    unsigned int crc = 0xFFFFFFFF;
    unsigned char *p = (unsigned char *)data;
    while (n_bytes--)
    {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}
