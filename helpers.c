#include "helpers.h"
#include "debug.h"
#include "icsmm.h"
/* Helper function definitions go here */
size_t alignment(size_t size)
{
    size_t alignedSize = size;
    if (size % 16 != 0)
    {
        alignedSize = (((int)size/16)+1)*16;
    }
    return alignedSize;
}

void* create_epilogue()
{
    ics_header epilogue;
    epilogue.hid = HEADER_MAGIC;
    epilogue.block_size = 1;
    epilogue.requested_size = 0;
    ics_header* epiloguePtr = ics_get_brk() - sizeof(ics_header);
    *epiloguePtr = epilogue;
    return (void*) epiloguePtr;
}

void* create_prologue(void** heap)
{
    ics_footer prologue;
    prologue.fid = FOOTER_MAGIC;
    prologue.block_size = 1;
    prologue.requested_size = 0;
    ics_footer* prologuePtr = *heap;
    *prologuePtr = prologue;
    return (void*) prologuePtr;
}

void init_free_block(void** heap)
{
    ics_header fh;
    fh.hid = HEADER_MAGIC;
    fh.block_size = ics_get_brk() - *heap - 16;
    fh.requested_size = 0;
    ics_header* fh_ptr = *heap + 8;
    *fh_ptr = fh;

    ics_footer ff;
    ff.fid = FOOTER_MAGIC;
    ff.block_size = ics_get_brk() - *heap - 16;
    ff.requested_size = 0;
    ics_footer* ff_ptr = ics_get_brk() - 16;
    *ff_ptr = ff;


    freelist_head = (ics_free_header*)fh_ptr;
    freelist_head->header = *fh_ptr;
    freelist_head->next = NULL;
    freelist_head->prev = NULL;
    freelist_next = freelist_head;
    printf("free block starts at : %p\n", fh_ptr);
    printf("free block ends at : %p\n", ff_ptr);
    printf("block size is : %d\n", ff_ptr->block_size);
    printf("address of freelist_head %p\n", freelist_head);
}

int moreHeap(void** heap)
{
    printf("start finding block and allocate pages\n");
    ics_footer* current_footer = ics_get_brk() - 16;
    printf("current_footer %p\n", current_footer);
    ics_free_header* new_fh = freelist_head;
    ics_free_header* current_header = new_fh;
    printf("current footer bitfield %d\n", current_footer->block_size);
    ((ics_header*)(ics_get_brk() - sizeof(ics_footer)))->block_size = 0;
    ics_header* current_epilogue = ((ics_header*)(ics_get_brk() - sizeof(ics_footer))); //reset epilogue;
    void* new_heap = ics_inc_brk();
    if (new_heap == (void*) -1 )
    {
        printf("requested too many pages\n");
        printf("brk at %p\n", ics_get_brk());
        return -1;
    }
    create_epilogue();
    printf("new epilogue address at %p\n", ics_get_brk()-8);
    printf("new fh is at %p\n", new_fh);
    //printf("new fh_next is at %p\n", new_fh->next);

    // get last free block
    if (new_fh != NULL)
    {
        while (new_fh != NULL)
        {
            current_header = new_fh;
            new_fh = new_fh->next;
        }
        current_footer = (void*)current_header+((ics_header)current_header->header).block_size-8;
    }
    printf("%p\n", current_footer);
    printf("free list is %p\n", freelist_head);
    printf("current header is %p\n", current_header);
    
    printf("anything\n");
    printf("current_footer at %p\n",current_footer);
    printf("new heap starts at %p\n", new_heap);
    printf("current_footer block size %d\n", (current_footer)->block_size);
    if (freelist_head != NULL && (int)((void*)new_heap-(void*)current_footer) == 16)
    {
        //two block are adjacent
        //header remain the same but add new size;
        printf("header size before change: %d\n", current_header->header.block_size);
        current_header->header.block_size += 4096;
        printf("header size after change: %d\n", current_header->header.block_size);

        //old footer ignored
        ics_footer footer;
        footer.fid = FOOTER_MAGIC;
        footer.block_size = current_header->header.block_size;
        footer.requested_size = 0;
        ics_footer* footer_ptr = ics_get_brk() - 16; 
        *footer_ptr = footer;

        printf("address of footer %p\n", footer_ptr);
    }
    else
    {
        //modify epilogue from last one;
        current_epilogue->block_size = 4096;

        ics_footer footer;
        footer.fid = FOOTER_MAGIC;
        footer.block_size = 4096;
        footer.requested_size = 0;
        ics_footer* footer_ptr = ics_get_brk() - 16; 
        *footer_ptr = footer;


        ics_free_header* new_fh;
        new_fh = (ics_free_header*) current_epilogue;
        new_fh->header = *current_epilogue;
        new_fh->prev = current_header;
        new_fh->next = NULL;
        if (freelist_head == NULL)
        {
            //no freelist_head make new block header
            
            freelist_head = new_fh;
            freelist_next = new_fh;
        }
        else
        {
            current_header->next = new_fh;
        }
        
        printf("freehead is at %p\n", freelist_head);
        printf("freehead next is at %p\n", freelist_head->next);
    }
    if (ics_get_brk() - (void*)heap == 20480)
    {
        return -1;
    }
    return 0;
}



void* search(size_t size, size_t payload)
{
    printf("start searching\n");
    ics_free_header* temp = freelist_next;
    if (freelist_head == NULL)
    {
        return NULL;
    }
    while (1)
    {
        size_t useable_bytes = ((ics_header)(freelist_next->header)).block_size - 16;
        printf("useable_bytes is %ld\n", useable_bytes);
        if (useable_bytes >= payload)
        {
            printf("block found\n");
            ics_free_header* returnAddr = NULL;
            if (useable_bytes - payload >= 32)
            {
                ics_header* old_header = (ics_header*)freelist_next;
                old_header->block_size = payload + 17;
                old_header->requested_size = size;

                ics_footer footer;
                footer.fid = FOOTER_MAGIC;
                footer.block_size = payload+17;
                footer.requested_size = size;
                ics_footer* footerPtr = (void*) old_header + payload + 8;
                *footerPtr = footer;

                ics_header new_fh;
                new_fh.hid = HEADER_MAGIC;
                new_fh.block_size = useable_bytes - payload;
                new_fh.requested_size = 0;
                ics_header* headerPtr = (void*) footerPtr + 8;
                *headerPtr = new_fh;

                ics_footer* old_footer = (void*) headerPtr + useable_bytes - payload - 16+8;
                old_footer->block_size = useable_bytes-payload;

                //put them back into linked list, no readjusting.
                ics_free_header* new_free_block = (ics_free_header*) headerPtr;
                new_free_block->header = *headerPtr;
                new_free_block->prev = freelist_next->prev;
                new_free_block->next = freelist_next->next;
                if (freelist_next->next != NULL)
                {
                    freelist_next->next->prev = new_free_block;
                }
                if (freelist_next->prev != NULL)
                {
                    freelist_next->prev->next = new_free_block;
                }
                freelist_next->prev = NULL;
                freelist_next->next = NULL;
                
                returnAddr = (void*)old_header;
                freelist_next = new_free_block;
                printf("head size is now %d\n",((ics_header)(freelist_head->header)).block_size);
                if (((ics_header)(freelist_head->header)).block_size % 16 != 0)
                {
                    printf("we change the head of freelist");
                    freelist_head = new_free_block;
                }
                printf("return address is %p\n", returnAddr->next);
                return returnAddr;
            }
            else
            {
                printf("DONT SPLIT\n");
                printf("block_size is %d\n", ((ics_header)(freelist_next->header)).block_size);
                if (freelist_next->next != NULL)
                {
                    freelist_next->next->prev = freelist_next->prev;
                }
                if (freelist_next->prev != NULL)
                {
                    freelist_next->prev->next = freelist_next->next;
                }

                returnAddr = freelist_next;

                
                returnAddr->header.block_size += 1;
                returnAddr->header.requested_size = size;
                printf("new_header-> %d\n", returnAddr->header.block_size);
                
                ics_footer* old_footer = (void*) returnAddr+returnAddr->header.block_size-1 - 8;
                old_footer->block_size +=1;
                old_footer->requested_size = size;
                if (freelist_next->next == NULL && freelist_next->prev == NULL)
                {
                    freelist_next = NULL;
                    freelist_head = NULL;
                }
                else if (freelist_next->next == NULL && freelist_next->prev != NULL)
                {
                    freelist_next = freelist_head;
                }
                else if (freelist_next->next != NULL && freelist_next->prev == NULL)
                {
                    freelist_next = freelist_next->next;
                    freelist_head = freelist_next;
                }
                else if (freelist_next->next != NULL && freelist_next->prev != NULL)
                {
                    freelist_next = freelist_next->next;
                }
               
                return returnAddr;
            }
        }
        freelist_next = freelist_next->next;
        if (freelist_next == NULL)
        {
            freelist_next = freelist_head;
        }
        if (freelist_next == temp)
        {
            return NULL;
        }
    }
 
    return NULL;
}

int validPtr(void* ptr, void* heapSpace)
{
    printf("heap ends at %p\n", ics_get_brk());
    if (heapSpace > ptr || ptr > ics_get_brk())
    {
        printf("ptr not on the heap\n");
        return -1;
    }
    ics_header* curr_header = (void*) ptr - 8;
    ics_footer* curr_footer = (void*) ptr - 8 + curr_header->block_size - 9;
    if (curr_header->block_size % 16 != 1 || curr_footer->block_size % 16 != 1 || curr_header->block_size != curr_footer->block_size)
    {
        printf("size is incorrect\n");
        return -1;
    }

    if (curr_header->hid != HEADER_MAGIC || curr_footer->fid != FOOTER_MAGIC)
    {
        printf("hid and fid is incorrect\n");
        return -1;
    }
    if (curr_header->requested_size != curr_footer->requested_size)
    {
        return -1;
    }
    printf("ptr is valid\n");
    return 0;
}

void setField(void* ptr,void* heapSpace)
{
    printf("free pointer at address %p\n",ptr);
    printf("heap space is %p\n", heapSpace);
    if (ptr - 16 <= heapSpace)
    {
        printf("it is in the front of the heap, no colseacing\n");
        ics_header* curr_header = (void*) ptr - 8;
        ics_footer* curr_footer = (void*) ptr - 8 + curr_header->block_size - 9;

        //-1 bitfield
        curr_header->block_size -= 1;
        curr_footer->block_size -= 1;

        curr_header->requested_size = 0;
        curr_footer->requested_size = 0;

        //insert into free_list
        ics_free_header* temp = freelist_next;
        printf("freelist_next at %p\n", temp);
        if (temp == NULL)
        {
            printf("current block is the only block thats free\n");
            freelist_head = (ics_free_header*)curr_header;
            freelist_head->header = *curr_header;
            freelist_head->next = NULL;
            freelist_head->prev = NULL;
            freelist_next = freelist_head;
            printf("address of freelist_head %p\n", freelist_head);
        }
        else
        {
            if (temp->next == NULL && temp->prev == NULL)
            {
                printf("freelist_next has one block]\n");
                printf("Don't link, cuz lower address coleascing only\n");
                printf("freelist_next at %p\n", temp);
                freelist_head = (ics_free_header*)curr_header;
                freelist_head->header = *curr_header;
                freelist_head->next = temp;
                freelist_head->next->prev = freelist_head;
                freelist_head->prev = NULL;
                printf("freelist next->next should be at 028 address: %p\n", freelist_head->next);
                
            }
            else if (temp->next != NULL && temp->prev != NULL)
            {
                printf("no null case\n");
                while (temp != NULL)
                {
                    if (temp->prev == NULL)
                    {
                        break;
                    }
                    temp = temp->prev;
                }
                freelist_head = (ics_free_header*)curr_header;
                freelist_head->header = *curr_header;
                freelist_head->prev = NULL;
                freelist_head->next = temp;
                freelist_head->next->prev = freelist_head;       
            }
            else if (temp->next == NULL && temp->prev != NULL)
            {
    
                printf("temp is %p\n", temp);
                while (temp->prev != NULL)
                {
                    printf("temp is %p\n", temp);
                    temp = temp->prev;
                }
                freelist_head = (ics_free_header*)curr_header;
                freelist_head->header = *curr_header;
                freelist_head->next = temp;
                freelist_head->next->prev = freelist_head;
                freelist_head->prev = NULL;
            }
        }
    }
    else
    {
        printf("might need coleascing\n");
        printf("free target at %p\n", ptr);
        ics_footer* previous_block = (void*)ptr - 16;//header + footer;
        printf("block bitfield is %d\n", previous_block->block_size);
        if (previous_block->block_size % 16 != 0)
        {
            printf("we dont need to perform coleascing\n");
            ics_header* curr_header = (void*) ptr - 8;
            ics_footer* curr_footer = (void*) ptr - 8 + curr_header->block_size - 9;
            printf("block size is, expected 49 : %d\n", curr_header->block_size);
            curr_header->block_size -= 1;
            curr_footer->block_size -= 1;

            curr_header->requested_size = 0;
            curr_footer->requested_size = 0;

            ics_free_header* temp = freelist_next;
            if (temp == NULL)
            {
                printf("current block is the only block thats free\n");
                freelist_head = (ics_free_header*)curr_header;
                freelist_head->header = *curr_header;
                freelist_head->next = NULL;
                freelist_head->prev = NULL;
                freelist_next = freelist_head;
            }
            else
            {
                printf("temp is at %p\n", temp);
                printf("temp->next : %p, temp->prev %p\n", temp->next, temp->prev);
                if (temp->next == NULL && temp->prev == NULL)
                {
                    if ((void*) curr_header < (void*) temp)
                    {
                        printf("All null\n");
                        printf("new block is the head\n");
                        freelist_head = (ics_free_header*)curr_header;
                        freelist_head->header = *curr_header;
                        freelist_head->next = temp;
                        printf("temp address is: %p\n", temp);
                        printf("the new block address is at %p\n", curr_header);
                        freelist_head->next->prev = freelist_head;
                        freelist_head->prev = NULL;
                        printf("head->next->prev is %p\n",freelist_head->next->prev);
                    }
                    else
                    {
                        printf("insert after head\n");
                        freelist_head = temp;
                        freelist_head->header = temp->header;
                        freelist_head->next = (ics_free_header*)curr_header;
                        freelist_head->next->header = *curr_header;
                        freelist_head->next->prev = freelist_head;           
                    }           
                }
                else if (temp->next != NULL && temp->prev == NULL)
                {
                    printf("starting from the begining of list\n");
                    if ((void*) curr_header < (void*) temp)
                    {
                        printf("place at beginning\n");
                        freelist_head = (ics_free_header*)curr_header;
                        freelist_head->header = *curr_header;
                        freelist_head->next = temp;
                        freelist_head->next->prev = freelist_head;
                        freelist_head->prev = NULL;
                    }
                    else
                    {
                        while (temp != NULL)
                        {
                            printf("head is %p\n", temp);
                            printf("block at %p\n", curr_header);
                            if (((void*)temp->next > (void*)curr_header || temp->next == NULL )&& (void*)curr_header > (void*)temp)
                            {
                                printf("block found, it is address %p\n", temp);
                                break;
                            }
                            temp = temp->next;
                        }
                        freelist_head = (ics_free_header*)curr_header;
                        freelist_head->header = *curr_header;
                        freelist_head->prev = temp;
                    
                        printf("freelist_next at %p\n", freelist_next);
                        if (temp->next != NULL)
                        {
                            freelist_head->next = temp->next;
                            freelist_head->next->prev = freelist_head;
                        } 
                        else
                        {
                            freelist_head->next = NULL;
                        }
                        freelist_head->prev->next = freelist_head;

                        while (freelist_head != NULL)
                        {
                            if (freelist_head->prev == NULL)
                            {
                                break;
                            }
                            freelist_head = freelist_head->prev;    
                        }
                    }    
                }
                else if (temp->next == NULL && temp->prev != NULL)
                {
                    printf("header at %p, temp at %p\n", curr_header, temp);
                    if ((void*) curr_header < (void*)temp)
                    {
                        while (temp != NULL)
                        {
                            printf("prev is %p\n", temp);
                            if ((void*)temp->prev < (void*)curr_header && (void*)curr_header < (void*)temp)
                            {
                                printf("block found, it is address %p\n", temp);
                                break;
                            }
                            temp = temp->prev;
                        }
                        printf("final prev is %p\n", temp);
                        freelist_head = (ics_free_header*)curr_header;
                        freelist_head->header = *curr_header;
                        freelist_head->next = temp;

                        if (temp->prev != NULL)
                        {
                            freelist_head->prev = temp->prev;
                            freelist_head->prev->next = freelist_head;
                        }
                        else
                        {
                            freelist_head->prev = NULL;
                        }
                        freelist_head->next->prev = freelist_head;
                        
                        while (freelist_head != NULL)
                        {
                            if (freelist_head->prev == NULL)
                            {
                                break;
                            }
                            freelist_head = freelist_head->prev;
                        }
                    }
                    else
                    {
                        while (temp != NULL)
                        {
                            printf("head is %p\n", temp);
                            printf("block at %p\n", curr_header);
                            if (((void*)temp->next > (void*)curr_header || temp->next == NULL )&& (void*)curr_header > (void*)temp)
                            {
                                printf("block found, it is address %p\n", temp);
                                break;
                            }
                            temp = temp->next;
                        }
                        freelist_head = (ics_free_header*)curr_header;
                        freelist_head->header = *curr_header;
                        freelist_head->prev = temp;
                        
                        if (temp->next != NULL)
                        {
                            freelist_head->next = temp->next;
                            freelist_head->next->prev = freelist_head;
                        } 
                        else
                        {
                            freelist_head->next = NULL;
                        }
                        freelist_head->prev->next = freelist_head;

                        while (freelist_head != NULL)
                        {
                            if (freelist_head->prev == NULL)
                            {
                                break;
                            }
                            freelist_head = freelist_head->prev;
                            
                        }
                    }       
                }
                else if (temp->next != NULL && temp->prev != NULL)
                {
                    printf("we know that if current header > temp, we go to next, other wise we go prev\n");
                    printf("current block at %p, and next at %p\n", curr_header, temp);
                    if ((void*)curr_header > (void*)temp)
                    {
                        printf("header is after\n");
                        while (temp != NULL)
                        {
                            printf("head is %p\n", temp);
                            printf("block at %p\n", curr_header);
                            if (((void*)temp->next > (void*)curr_header || temp->next == NULL )&& (void*)curr_header > (void*)temp)
                            {
                                printf("block found, it is address %p\n", temp);
                                break;
                            }
                            temp = temp->next;
                        }
                        freelist_head = (ics_free_header*)curr_header;
                        freelist_head->header = *curr_header;
                        freelist_head->prev = temp;
                        
                        if (temp->next != NULL)
                        {
                            freelist_head->next = temp->next;
                            freelist_head->next->prev = freelist_head;
                        } 
                        else
                        {
                            freelist_head->next = NULL;
                        }
                        freelist_head->prev->next = freelist_head;
                        
                        while (freelist_head != NULL)
                        {
                            if (freelist_head->prev == NULL)
                            {
                                break;
                            }
                            freelist_head = freelist_head->prev;
                        
                        }
                    }
                    else
                    {
                        printf("header is before\n");
                        while (temp != NULL)
                        {
                            printf("prev is %p\n", temp);
                            if ((void*)temp->prev < (void*)curr_header && (void*)curr_header < (void*)temp)
                            {
                                printf("block found, it is address %p\n", temp);
                                break;
                            }
                            temp = temp->prev;
                        }
                        freelist_head = (ics_free_header*)curr_header;
                        freelist_head->header = *curr_header;
                        freelist_head->next = temp;
                        if (temp->prev != NULL)
                        {
                            freelist_head->prev = temp->prev;
                            freelist_head->prev->next = freelist_head;
                        }
                        else
                        {
                            freelist_head->prev = NULL;
                        }
                        freelist_head->next->prev = freelist_head;
                        
                        while (freelist_head != NULL)
                        {
                            if (freelist_head->prev == NULL)
                            {
                                break;
                            }
                            freelist_head = freelist_head->prev;
                    
                        }
                    }  
                }
            }
        }
        else
        {
            printf("combine with previous free block needed to be performed\n");
            ics_header* curr_header = (void*) ptr - 8;
            ics_footer* curr_footer = (void*) ptr - 8 + curr_header->block_size - 9;
            printf("block size is, expected 49 : %d\n", curr_header->block_size);
            
            ics_footer* previous_footer = (void*)curr_header - 8;
            ics_footer* previous_header = (void*)previous_footer - previous_footer->block_size + 8;
            printf("previous block size is %d\n", previous_footer->block_size);
            printf("header at %p\n", previous_header);
            
            curr_footer->block_size -= 1;
            previous_header->block_size += curr_footer->block_size;
            printf("now new size is %d\n", previous_header->block_size);
            curr_footer->block_size = previous_header->block_size;
            curr_footer->requested_size = 0;
        }
    }
}

