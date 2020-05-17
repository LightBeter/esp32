#ifndef _RINGBUFFER_H
#define _RINGBUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t *buffer;
    uint16_t  bufferSize;
    uint16_t  head;
    uint16_t  tail;
    uint16_t  count;
    uint32_t  total;
} ringbuffer_type;

void ringbuffer_init(ringbuffer_type *handle, uint16_t bufferSize);
void ringbuffer_deinit(ringbuffer_type *handle);
void ringbuffer_enter(ringbuffer_type *handle, uint16_t value);
uint16_t ringbuffer_avg(ringbuffer_type *handle);

#ifdef __cplusplus
}
#endif
#endif