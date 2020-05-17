#include <stdlib.h>
#include "ringbuffer.h"

void ringbuffer_init(ringbuffer_type *handle, uint16_t bufferSize)
{
    if (handle->buffer || 0 >= bufferSize) 
        return;

    handle->buffer     = (uint16_t*)malloc(bufferSize * sizeof(uint16_t));
    handle->bufferSize = bufferSize;
    handle->head  = 0;
    handle->tail  = 0;
    handle->total = 0;
    handle->count = 0;
}

void ringbuffer_deinit(ringbuffer_type *handle)
{
    if (!handle->buffer) 
        return;
    
    handle->head  = 0;
    handle->tail  = 0;
    handle->count = 0;
    handle->total = 0;
    handle->bufferSize = 0;
    free(handle->buffer);
    handle->buffer = NULL;
}

void ringbuffer_enter(ringbuffer_type *handle, uint16_t value)
{
    if (handle->count == handle->bufferSize) {
        handle->count -= 1;
        handle->total -= handle->buffer[handle->head];
        handle->head = (handle->head + 1) % handle->bufferSize;
    }
    handle->count += 1;
    handle->total += value;
    handle->buffer[handle->tail] = value;
    handle->tail = (handle->tail + 1) % handle->bufferSize;
}

uint16_t ringbuffer_avg(ringbuffer_type *handle)
{
    return handle->total / handle->count;
}