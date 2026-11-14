#ifndef MUI_MEM_H
#define MUI_MEM_H

#include <stddef.h>
#include <stdint.h>

#include "mui_conf.h"

void mui_mem_init();
void mui_mem_deinit();

void *mui_mem_malloc(size_t size);
void *mui_mem_realloc(void *p, size_t new_size);
void mui_mem_free(void *);

size_t mui_mem_biggest_free(void);

#endif /* MUI_MEM_H */
