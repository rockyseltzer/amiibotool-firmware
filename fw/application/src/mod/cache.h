#ifndef CACHE_H
#define CACHE_H

#include "ntag_def.h"
#include "vfs.h"

#define CACHEDATASIZE 312

typedef struct
{
    int32_t enabled;
    uint32_t id;
    ntag_t ntag;
    uint8_t retain_data[CACHEDATASIZE];
} cache_data_t;

int32_t cache_clean();
int32_t cache_save();
cache_data_t *cache_get_data();
bool cache_valid();

#endif /* CACHE_H */
