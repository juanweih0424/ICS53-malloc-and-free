#ifndef HELPERS_H
#define HELPERS_H
#include <stdio.h>
#include <stdlib.h>
/* Helper function declarations go here */
size_t alignment(size_t size);
void* create_epilogue();
void* create_prologue(void** heap);
void init_free_block(void** heap);
void* search(size_t size, size_t payload);
int moreHeap(void** heap);
int validPtr(void* ptr, void* heapSpace);
void setField(void* ptr,void* heapSpace);
#endif
