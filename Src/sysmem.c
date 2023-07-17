#include <errno.h>
#include <stdio.h>


extern int errno;

/* Increase program data space. Malloc and related functions depend on it. */
void *_sbrk(int incr)
{
	extern char end __asm("end");
	static char *heap_end;
	char *prev_heap_end;
	register char *stack_ptr __asm("sp");

	if (heap_end == 0) {
		heap_end = &end;
	}

	prev_heap_end = heap_end;
	if (heap_end + incr > stack_ptr) {
		errno = ENOMEM;
		return (void *)(-1);
	}

	heap_end += incr;

	return (void *)prev_heap_end;
}
