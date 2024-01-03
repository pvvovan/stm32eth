#include <errno.h>
#include <stdint.h>


extern uint8_t _end; /* Symbol defined in the linker script */

/**
 * Pointer to the current high watermark of the heap usage
 */
static uint8_t *__sbrk_heap_end = &_end;

/**
 * @brief _sbrk() allocates memory to the newlib heap and is used by malloc
 *        and others from the C library
 *
 * This implementation starts allocating at the '_end' linker symbol
 *
 * @param incr Memory size
 * @return Pointer to allocated memory
 */
void *_sbrk(ptrdiff_t incr)
{
  extern uint8_t const *const _max_heap;
  uint8_t *prev_heap_end;

  /* Protect heap from out of memory region */
  if ((__sbrk_heap_end + incr) >= _max_heap) {
    errno = ENOMEM;
    return (void *)-1;
  }

  prev_heap_end = __sbrk_heap_end;
  __sbrk_heap_end += incr;

  return (void *)prev_heap_end;
}
