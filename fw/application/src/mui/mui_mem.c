#include "mui_mem.h"

#include "stdlib.h"
#include "tlsf.h"

static uint8_t m_mem[MUI_MEM_BUFF_SIZE];
static tlsf_t m_tlsf = NULL;

static void mui_mem_walker(void *ptr, size_t size, int used, void *user)
{
    size_t *p_biggest = user;
    if (!used && (size > *p_biggest))
    {
        *p_biggest = size;
    }
}

void mui_mem_init()
{
    m_tlsf = tlsf_create_with_pool(m_mem, sizeof(m_mem));
}
void mui_mem_deinit()
{
    tlsf_destroy(m_tlsf);
    m_tlsf = NULL;
}
void *mui_mem_malloc(size_t size)
{
    return tlsf_malloc(m_tlsf, size);
}
void *mui_mem_realloc(void *p, size_t new_size)
{
    return tlsf_realloc(m_tlsf, p, new_size);
}
void mui_mem_free(void *p)
{
    tlsf_free(m_tlsf, p);
}

size_t mui_mem_biggest_free(void)
{
    size_t biggest = 0;
    tlsf_walk_pool(tlsf_get_pool(m_tlsf), mui_mem_walker, &biggest);
    return biggest;
}
