#include "mui_view_port.h"

#include "mui_core.h"

mui_view_port_t *mui_view_port_create()
{
    mui_view_port_t *p_vp = mui_mem_malloc(sizeof(mui_view_port_t));
    return p_vp;
}
void mui_view_port_free(mui_view_port_t *p_vp)
{
    mui_mem_free(p_vp);
}

void mui_view_port_enable_set(mui_view_port_t *p_vp, bool enabled)
{
    p_vp->enabled = enabled;
}

void mui_view_port_update(mui_view_port_t *p_vp)
{
    if (p_vp->enabled)
    {
        mui_update(mui());
    }
}
