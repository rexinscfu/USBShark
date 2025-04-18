/**
 * USBShark - Military-grade USB protocol analyzer
 * Lock-free ringbuffer implementation optimized for interrupt contexts
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>

/* Size must be a power of 2 for efficient masking operations */
#define RINGBUF_SIZE 128
#define RINGBUF_MASK (RINGBUF_SIZE - 1)

/**
 * Lock-free ring buffer optimized for single producer/single consumer
 * Use from ISR (producer) and main thread (consumer)
 */
typedef struct {
    volatile uint8_t buffer[RINGBUF_SIZE];  // Data storage
    volatile uint8_t write_index;           // Producer index
    volatile uint8_t read_index;            // Consumer index
    volatile uint8_t overflow_count;        // Number of buffer overflows
} ringbuffer_t;

/**
 * Initialize ring buffer
 * @param rb Pointer to ringbuffer struct
 */
static inline void ringbuffer_init(ringbuffer_t *rb) {
    rb->write_index = 0;
    rb->read_index = 0;
    rb->overflow_count = 0;
}

/**
 * Check if buffer is empty
 * @param rb Pointer to ringbuffer struct
 * @return true if empty, false if data available
 */
static inline bool ringbuffer_empty(const ringbuffer_t *rb) {
    return rb->write_index == rb->read_index;
}

/**
 * Check if buffer is full
 * @param rb Pointer to ringbuffer struct
 * @return true if full, false if space available
 */
static inline bool ringbuffer_full(const ringbuffer_t *rb) {
    return ((rb->write_index + 1) & RINGBUF_MASK) == rb->read_index;
}

/**
 * Get number of items in buffer
 * @param rb Pointer to ringbuffer struct
 * @return Number of bytes available for reading
 */
static inline uint8_t ringbuffer_count(const ringbuffer_t *rb) {
    return (rb->write_index - rb->read_index) & RINGBUF_MASK;
}

/**
 * Get space available in buffer
 * @param rb Pointer to ringbuffer struct
 * @return Number of free bytes available for writing
 */
static inline uint8_t ringbuffer_free(const ringbuffer_t *rb) {
    return RINGBUF_SIZE - ringbuffer_count(rb) - 1;
}

/**
 * Push byte to buffer (from ISR/producer)
 * @param rb Pointer to ringbuffer struct
 * @param data Byte to add
 * @return true if successful, false if buffer full
 */
static inline bool ringbuffer_push(ringbuffer_t *rb, uint8_t data) {
    uint8_t next_write = (rb->write_index + 1) & RINGBUF_MASK;
    
    if (next_write == rb->read_index) {
        rb->overflow_count++;
        return false; // Buffer full
    }
    
    rb->buffer[rb->write_index] = data;
    rb->write_index = next_write; // Release the data to consumer
    
    return true;
}

/**
 * Pop byte from buffer (from main/consumer)
 * @param rb Pointer to ringbuffer struct
 * @param data Pointer to store popped byte
 * @return true if successful, false if buffer empty
 */
static inline bool ringbuffer_pop(ringbuffer_t *rb, uint8_t *data) {
    if (rb->read_index == rb->write_index) {
        return false; // Buffer empty
    }
    
    *data = rb->buffer[rb->read_index];
    rb->read_index = (rb->read_index + 1) & RINGBUF_MASK;
    
    return true;
}

/**
 * Peek byte from buffer without removing
 * @param rb Pointer to ringbuffer struct
 * @param offset Offset from read position
 * @param data Pointer to store peeked byte
 * @return true if successful, false if offset beyond available data
 */
static inline bool ringbuffer_peek(const ringbuffer_t *rb, uint8_t offset, uint8_t *data) {
    if (offset >= ringbuffer_count(rb)) {
        return false;
    }
    
    *data = rb->buffer[(rb->read_index + offset) & RINGBUF_MASK];
    return true;
}

/**
 * Reset buffer to empty state
 * @param rb Pointer to ringbuffer struct
 */
static inline void ringbuffer_reset(ringbuffer_t *rb) {
    rb->read_index = rb->write_index;
}

/**
 * Push multiple bytes to buffer
 * @param rb Pointer to ringbuffer struct
 * @param data Pointer to data array
 * @param len Number of bytes to push
 * @return Number of bytes actually pushed
 */
static inline uint8_t ringbuffer_push_multiple(ringbuffer_t *rb, const uint8_t *data, uint8_t len) {
    uint8_t count = 0;
    
    while (count < len && ringbuffer_push(rb, data[count])) {
        count++;
    }
    
    return count;
}

/**
 * Pop multiple bytes from buffer
 * @param rb Pointer to ringbuffer struct
 * @param data Pointer to data storage
 * @param len Maximum number of bytes to pop
 * @return Number of bytes actually popped
 */
static inline uint8_t ringbuffer_pop_multiple(ringbuffer_t *rb, uint8_t *data, uint8_t len) {
    uint8_t count = 0;
    
    while (count < len && ringbuffer_pop(rb, &data[count])) {
        count++;
    }
    
    return count;
}

#endif /* RINGBUFFER_H */ 