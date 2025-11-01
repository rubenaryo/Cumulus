/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/10
Description : Material class for shader information
----------------------------------------------*/
#ifndef EASEL_HASH_UTIL_H
#define EASEL_HASH_UTIL_H

#include <stdint.h>

// Helper function for hashing c strings
inline uint32_t fnv1a(const char* text, uint32_t hash = 0x811C9DC5, uint32_t prime = 0x01000193)
{
    const unsigned char* ptr = (const unsigned char*)text;
    while (*ptr)
        hash = (*ptr++ ^ hash) * prime;

    return hash;
}

inline uint32_t fnv1a(const wchar_t* text, uint32_t hash = 0x811C9DC5, uint32_t prime = 0x01000193)
{
    const wchar_t* ptr = (const wchar_t*)text;
    while (*ptr)
        hash = (*ptr++ ^ hash) * prime;

    return hash;
}

#endif