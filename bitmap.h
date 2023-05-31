#if !defined BITMAP_H
#define BITMAP_H

#include <stddef.h>
#include <stdint.h>

struct bitmap
{
	uint32_t *data;
	size_t    size;
};

extern int  bitmap_init(struct bitmap *map, size_t bit_count);
extern void bitmap_free(struct bitmap *map);

extern int bitmap_set_bit(struct bitmap *map, size_t bit_id);
extern int bitmap_clear_bit(struct bitmap *map, size_t bit_id);
extern int bitmap_get_bit(struct bitmap *map, size_t bit_id);

#endif // BITMAP_H
