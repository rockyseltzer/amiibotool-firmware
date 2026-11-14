#include "mui_event.h"

#include "app_util_platform.h"
#include "nrf_log.h"

#include "mui_defines.h"

void mui_event_queue_init(mui_event_queue_t *p_queue)
{
    p_queue->head = 0;
    p_queue->count = 0;
}
void mui_event_set_callback(mui_event_queue_t *p_queue, mui_event_handler_t dispatcher, void *context)
{
    p_queue->dispatch_context = context;
    p_queue->dispatcher = dispatcher;
}

void mui_event_post(mui_event_queue_t *p_queue, mui_event_t *p_event)
{
    bool queued = false;
    CRITICAL_REGION_ENTER();
    uint8_t tail = (p_queue->head + p_queue->count) % MAX_EVENT_MSG;
    uint8_t last = (tail + MAX_EVENT_MSG - 1) % MAX_EVENT_MSG;
    if (p_queue->count > 0 && p_event->id != MUI_EVENT_ID_INPUT && p_queue->events[last].id == p_event->id)
    {
        queued = true;
    }
    else if (p_queue->count < MAX_EVENT_MSG)
    {
        p_queue->events[tail] = *p_event;
        p_queue->count++;
        queued = true;
    }
    CRITICAL_REGION_EXIT();
    if (!queued)
    {
        NRF_LOG_WARNING("event buffer is FULL!!");
    }
}

void mui_event_dispatch(mui_event_queue_t *p_queue)
{
    mui_event_t event;
    while (true)
    {
        bool has_event = false;
        CRITICAL_REGION_ENTER();
        if (p_queue->count > 0)
        {
            event = p_queue->events[p_queue->head];
            p_queue->head = (p_queue->head + 1) % MAX_EVENT_MSG;
            p_queue->count--;
            has_event = true;
        }
        CRITICAL_REGION_EXIT();
        if (!has_event)
        {
            break;
        }
        p_queue->dispatcher(p_queue->dispatch_context, &event);
    }
}

void mui_event_dispatch_now(mui_event_queue_t *p_queue, mui_event_t *p_event)
{
    p_queue->dispatcher(p_queue->dispatch_context, p_event);
}
