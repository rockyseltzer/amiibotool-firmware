#ifndef DB_HEADER_H
#define DB_HEADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t head;
    uint32_t tail;
    const char *name_en;

} db_amiibo_t;

struct _db_link_t;

typedef struct _db_game_t
{
    uint8_t game_id;
    uint8_t parent_game_id;
    const char *name_en;

    uint8_t order;
    uint16_t link_cnt;
} db_game_t;

typedef struct _db_link_t
{
    uint8_t game_id;
    uint32_t head;
    uint32_t tail;
    const char *note_en;

} db_link_t;

extern const db_amiibo_t amiibo_list[];
extern const size_t amiibo_list_size;
extern const db_game_t game_list[];
extern const db_link_t link_list[];

const db_amiibo_t *get_amiibo_by_id(uint32_t head, uint32_t tail);
const db_link_t *get_link_by_id(uint8_t game_id, uint32_t head, uint32_t tail);
bool is_valid_amiibo_v3(uint32_t head, uint32_t tail);

const char *get_amiibo_display_name(const db_amiibo_t *amiibo);

#endif /* DB_HEADER_H */
