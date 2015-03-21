#include "hash.h"
#include <stdint.h>

/* 32-bit implementation of the fnv-1a hash function. */
uint32_t hash_fnv_1a (const char *data, int length)
{
	unsigned FNV_prime = 16777619; /* 2^24 + 2^8 + 0x93 */
	uint32_t hash = 2166136261; /* FNV_offset_basis */

	int i;
	for (i = 0; i < length; ++i)
	{
		hash ^= data[i];
		hash *= FNV_prime;
	}

	return hash;
}
