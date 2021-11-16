#include "icsmm.h"
#include "debug.h"
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * The allocator MUST store the head of its free list in this variable. 
 * Doing so will make it accessible via the extern keyword.
 * This will allow ics_freelist_print to access the value from a different file.
 */
ics_free_header *freelist_head = NULL;

/*
 * The allocator MUST use this pointer to refer to the position in the free list to 
 * starting searching from. 
 */
ics_free_header *freelist_next = NULL;

//global heap
void* heapSpace = NULL;


/*
 * This is your implementation of malloc. It acquires uninitialized memory from  
 * ics_inc_brk() that is 16-byte aligned, as needed.
 *
 * @param size The number of bytes requested to be allocated.
 *
 * @return If successful, the pointer to a valid region of memory of at least the
 * requested size is returned. Otherwise, NULL is returned and errno is set to 
 * ENOMEM - representing failure to allocate space for the request.
 * 
 * If size is 0, then NULL is returned and errno is set to EINVAL - representing
 * an invalid request.
 */
void *ics_malloc(size_t size) {
    if (size == 0 || size > 5*4096)
    {
        errno = EINVAL;
        return NULL;
    }
    void* heap;
    size_t alignedSize = alignment(size);
    int temp = (int)alignedSize - 4064;
    printf("aligned payload is %ld\n", alignedSize);
    
    if (heapSpace == NULL)
    {
        heap = ics_inc_brk();
        if (heap == (void*) -1)
        {
            errno = ENOMEM;
            return NULL;
        }
        heapSpace = heap;
        //create free block
        init_free_block(&heap);
        //set up prologue;
        ics_footer* prologue = create_prologue(&heap);
        //set up epilogue
        ics_header* epilogue = (ics_header*) create_epilogue();
        printf("address of heap is now placed at : %p\n", heap);
        printf("address of end of heap is now placed at : %p\n", ics_get_brk());
        printf("address of epilogue is now placed at : %p\n", epilogue);
        //make sure that we request enough page to store data
        
        if (temp > 0)
        {while (temp > 0)
        {
            printf("temp is %d\n", temp);
            int result = moreHeap(&heap);
            if (result == -1)
            {
                errno = ENOMEM;
                return NULL;
            }
            temp -= 4096;
        }}
        return search(size, alignedSize) + 8;
           
    }
    else
    {
        //inspect space first to evaluate whether we should add the page
        printf("else block is called\n");
        void * result = search(size, alignedSize);
        if (!result)
        {
            printf("we need to allocate more pages\n");
            printf("temp is %d\n", temp);
            while (temp+4064 > 0)
            {
                int result1 = moreHeap(&heap);
                if (result1 == -1)
                {
                    errno = ENOMEM;
                    return NULL;
                }
                temp -= 4096;
            }
            result = search(size, alignedSize);
        }
        return result+8;
    }
    
    return NULL;
}

/*
 * Marks a dynamically allocated block as no longer in use and coalesces with 
 * adjacent free blocks (as specified by Homework Document). 
 * Adds the block to the appropriate bucket according to the block placement policy.
 *
 * @param ptr Address of dynamically allocated memory returned by the function
 * ics_malloc.
 * 
 * @return 0 upon success, -1 if error and set errno accordingly.
 * 
 * If the address of the memory being freed is not valid, this function sets errno
 * to EINVAL. To determine if a ptr is not valid, (i) the header and footer are in
 * the managed  heap space, (ii) check the hid field of the ptr's header for
 * special value (iii) check the fid field of the ptr's footer for special value,
 * (iv) check that the block_size in the ptr's header and footer are equal, (v) 
 * the allocated bit is set in both ptr's header and footer, and (vi) the 
 * requested_size is identical in the header and footer.
 */
int ics_free(void *ptr) { 
    printf("free called\n");
    if (validPtr(ptr, heapSpace) == -1)
    {
        errno = EINVAL;
        return -1;
    }
    setField(ptr, heapSpace);
    return 0;
}

/*
 * Resizes the dynamically allocated memory, pointed to by ptr, to at least size 
 * bytes. See Homework Document for specific description.
 *
 * @param ptr Address of the previously allocated memory region.
 * @param size The minimum size to resize the allocated memory to.
 * @return If successful, the pointer to the block of allocated memory is
 * returned. Else, NULL is returned and errno is set appropriately.
 *
 * If there is no memory available ics_malloc will set errno to ENOMEM. 
 *
 * If ics_realloc is called with an invalid pointer, set errno to EINVAL. See ics_free
 * for more details.
 *
 * If ics_realloc is called with a valid pointer and a size of 0, the allocated     
 * block is free'd and return NULL.
 */
void *ics_realloc(void *ptr, size_t size) {
    void* result = ics_malloc(size);
    printf("ptr address is %p\n", ptr);
    if (result)
    {
        printf("mi,ber %d\n", ((ics_footer*)((void*)ptr-8))->block_size-16);
        for (int i=0;i<((ics_footer*)((void*)ptr-8))->block_size-17;i++)
        {
            printf("%c", *(char*)ptr);
            (void*)ptr++;
        }
        ics_free(ptr);
        return result;
    }
    
    return NULL;
}