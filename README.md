# Dynamic Memory Allocator

## Overview

This project implements a dynamic memory allocator in C that provides custom implementations of malloc, free, and realloc. The allocator manages heap memory efficiently, ensuring proper allocation, deallocation, and reallocation while maintaining alignment requirements.

## Memory Allocator Functions

`mm_init`

- Initializes the heap before any memory allocations occur.

- Called automatically before any other memory functions.

- Returns true on success and false if initialization fails.

`malloc`

- Allocates a memory block of at least size bytes.

- Ensures 16-byte alignment.

- Returns a pointer to the allocated memory or NULL if allocation fails.

`free`

- Deallocates the memory block pointed to by ptr.

- Does nothing if ptr is NULL.

- Only operates on memory returned by malloc, calloc, or realloc.

`realloc`

- Resizes an allocated block while preserving its contents.

- If ptr is NULL, behaves like malloc(size).

- If size is zero, behaves like free(ptr).

- Returns a pointer to the newly allocated block or NULL if reallocation fails.

## Heap Consistency Checker (`mm_checkheap`)

- A debugging tool that validates the integrity of the heap by checking for common memory allocation issues such as:

- Proper alignment of memory blocks.

- Coalescing of adjacent free blocks.

- Valid pointers in the free list.

- No overlapping memory regions.

- All allocated blocks residing within the heap boundaries.

## Support Routines

- The allocator relies on memlib.c, which simulates system memory. Key functions include:

- `mm_sbrk(int incr)`: Expands the heap.

- `mm_heap_lo()`: Returns the heap’s start address.

- `mm_heap_hi()`: Returns the heap’s end address.

- `mm_heapsize()`: Returns the heap’s total size.

- `mm_pagesize()`: Returns the system's page size.

## Implementation Details

- Uses an explicit free list for efficient block management.

- Implements boundary tags for coalescing adjacent free blocks.

- Ensures memory is efficiently allocated and freed to minimize fragmentation.

## Conclusion

This project demonstrates fundamental concepts of dynamic memory management, including allocation, deallocation, and heap integrity checking. The implementation provides a robust and efficient memory allocator that mimics standard libc functions.


