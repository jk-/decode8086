#include <assert.h>
#include <stdlib.h>

#include "bitmap.h"

#define BITS_PER_WORD (sizeof(uint32_t) * 8)
#define WORD_OFFSET(index) ((index) / BITS_PER_WORD)
#define BIT_OFFSET(index)  ((index) % BITS_PER_WORD)

int bitmap_init(struct bitmap *map, size_t bit_count)
{
	assert(map != NULL);
	assert(bit_count > 0);

	map->size = WORD_OFFSET(bit_count) + (BIT_OFFSET(bit_count) > 0);
	map->data = calloc(map->size, sizeof(*map->data));

	if (!map->data) return -1;
	return 0;
}

void bitmap_free(struct bitmap *map)
{
	free(map->data);
	map->data = NULL;
}

int bitmap_set_bit(struct bitmap *map, size_t bit_id)
{
	assert(map != NULL);
	assert(map->data != NULL);

	if (bit_id >= map->size * BITS_PER_WORD) return -1;

	map->data[WORD_OFFSET(bit_id)] |= (1 << BIT_OFFSET(bit_id));
	return 0;
}

int bitmap_clear_bit(struct bitmap *map, size_t bit_id)
{
	assert(map != NULL);
	assert(map->data != NULL);

	if (bit_id >= map->size * BITS_PER_WORD) return -1;

	map->data[WORD_OFFSET(bit_id)] &= ~(1 << BIT_OFFSET(bit_id));
	return 0;
}

int bitmap_get_bit(struct bitmap *map, size_t bit_id)
{
	int bit;

	assert(map != NULL);
	assert(map->data != NULL);

	if (bit_id >= map->size * BITS_PER_WORD) return -1;

	bit = map->data[WORD_OFFSET(bit_id)] & (1 << BIT_OFFSET(bit_id));
	return bit != 0;
}

