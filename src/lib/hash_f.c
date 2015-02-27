#include "hash.h"

/* 32-bit implementation of the fnv-1a hash function. */
unsigned hash_fnv_1a (const char *data, int lenght)
{
	unsigned FNV_prime = 16777619; /* 2^24 + 2^8 + 0x93 */
	unsigned hash = FNV_offset_basis = 2166136261;

	int i;
	for (i = 0; i < length; ++i)
	{
		hash ^= data[i];
		hash *= FNV_prime;
	}

	return hash;
}
