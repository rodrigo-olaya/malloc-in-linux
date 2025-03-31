/*
 * mm.c
 *
 * Name: Rodrigo Olaya Moreno
 *
 * The following code is a dynamic memory simulation. It is capable of simulating malloc, free and realloc.
 * It was optimized using split-allocate, coalescing and segregated linked lists.
 * 
 * The initial design:
 * My initial design was capable of allocating, freeing and reallocationg. 
 * Nevertheless, it was extremely slow because I had a loop in malloc that iterated through
 * the whole heap. The meain idea was to iterate through the whole heap and try to find a free block. It epilogue was
 * reached, the heap was extended using mm_sbrk. That is, it used first-fit allocation.
 * The initial design did have very good memory utilization because it used split and allocate, which will be exaplained in detail below.
 * 
 * Header/footer design:
 * 
 * -------------------------------------------------------------------------
 * | size of payload + header + footer (7 bytes) | allocated/not allocated |
 * -------------------------------------------------------------------------
 * 
 * Block design;
 * The main takeaway here is that the payload has to be 16 bytes to be able to fit a previous and next pointers for the explicit free list.
 * More on this later.
 * 
 * --------------------------------------------------------------------
 * | Header (8 bytes) | Payload (minimum 16 bytes) | Footer (8 bytes) |
 * --------------------------------------------------------------------
 * 
 * Split-allocate:
 * When trying to allocate/reallocate a block of size n, we might find a block that can fit it but has extra space. 
 * This causes memory fragmentation, so I split the block into two parts. One was just enough, ie. aligned using provided function, to fit the block.
 * The other was passed into the free function to be used later on and not waste space. 
 * 
 * Coalescing:
 * When 2 or three adjacent blocks are free, it joins them together to m,ake a bigger free block. The way I implemented this was the following:
 * 1. If left was free, coalesce with it.
 * 2. After 1, if right is also free, coalesce with it.
 * 3. If left is not freee but right is free, coalesce only with right. 
 * This allowed me to eliminate the case where both are free and reuse the code for coalescing left and rightm separately. 
 * 
 * Explicit lists:
 * A single doubly linked list was used to iterate through the free blocks. The reasor for it is to avoid iterating the entire heap.
 * The way it worked is as follows:
 * The list starts empty, whenever a block is freed, it is added to the free list using prev and next nodes as shown here:
 * 
 * ---------------------------------------------------------------------------------
 * | Header (8 bytes) | Previous | Next| Payload (can be empty) | Footer (8 bytes) |
 * ---------------------------------------------------------------------------------
 * 
 * The previous and next nodes are used to iterate through the free list until fitting blick is found. We start iterating from head->next.
 * It the head is found, then we need to extend the heap.
 * 
 * Segregated lists:
 * This essentially divides free blocks into categories according to size. The reasoning for this is as follows. 
 * If we have many small free blocks, say 16 in size, but only one that is 4096 in size. It we wabnt to allocate 4096, we have
 * to iterate through all 16 sized blocks, which takes time. To avoid this, the can search directly in a list 
 * specifically made for block of size greater than 4096.
 * 
 * Key aspects:
 * Malloc and realloc use free. This is a big part of the design because it allowed for me to reuse a lot of code and make the debugging less tedious.
 * Realloc - It is probably the most complex so it is worth explaining. If the original block is the same as input size, it remains the same. If the 
 * original block is not enough but the next block and original combined are enough, it uses the block. Whenever a block(or combination of blocks) 
 * is too large, it split-allocates. If none of the already mentioned techniques work, we copy the data already stored, make new space using mm_sbrk 
 * and copy the dta into it.
 *
 * Other notes: The code was all created and tested using ubuntu focal, as instructed by a TA. Please reach out if you needd more info on this.
 * The heap checker is only being called in one place, it is the one I thought to be more optimal to show consistency after a few operations. 
 * In real debugging operations, it was used in other places.
 * Also, on my machine this passes 64/100 in final submision.
 *
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
 //#define DEBUG

#ifdef DEBUG
// When debugging is enabled, the underlying functions get called
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
// When debugging is disabled, no code gets generated
#define dbg_printf(...)
#define dbg_assert(...)
#endif // DEBUG

// do not change the following!
#ifdef DRIVER
// create aliases for driver tests
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mm_memset
#define memcpy mm_memcpy
#endif // DRIVER

#define ALIGNMENT 16

// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

static bool aligned(const void* p);

// struct for Doubly Linked List Node
typedef struct dll_node{
    struct dll_node* prev;
    struct dll_node* next;
} dll_node_t;

dll_node_t* seg_list[15];

void* first;    // pointer to the initial heap extension

/*
 * mm_init: returns false on error, true on success.
 */
bool mm_init(void)
{
    // IMPLEMENT THIS

    size_t* pro_head;
    size_t* pro_foot;
    size_t* epi; 

    // make initial space and assign initial pointer
    if ((first = mm_sbrk(32)) == (void*)-1){
        return false;
    }

    pro_head = first + 8;   // initialize pointer for prologue
    pro_foot = first + 16;
    epi = first + 24;    //init pointer for epilogue

    *pro_head = 0x11;    // initialize values for proloque
    *pro_foot = 0x11;
    *epi = 0x1;    //initialize values for epilogue

    // initialize and set heads of segregated free lists
    struct dll_node* dll_head16 = NULL; // 0
    struct dll_node* dll_head32 = NULL; // 1
    struct dll_node* dll_head48 = NULL; // 2
    struct dll_node* dll_head64 = NULL; // 3
    struct dll_node* dll_head80 = NULL; // 4
    struct dll_node* dll_head96 = NULL; // 5
    struct dll_node* dll_head112 = NULL; // 6
    struct dll_node* dll_head128 = NULL; // 7
    struct dll_node* dll_head144 = NULL; // 8
    struct dll_node* dll_head160 = NULL; // 9
    struct dll_node* dll_head256 = NULL; // 10
    struct dll_node* dll_head512  = NULL; // 11
    struct dll_node* dll_head1024 = NULL; // 12
    struct dll_node* dll_head2048 = NULL; // 13
    struct dll_node* dll_head4096 = NULL; // 14

    seg_list[0] = dll_head16; // 0
    seg_list[1] = dll_head32; // 1
    seg_list[2] = dll_head48; // 2
    seg_list[3] = dll_head64; // 3
    seg_list[4] = dll_head80; // 4
    seg_list[5] = dll_head96; // 5
    seg_list[6] = dll_head112; // 6
    seg_list[7] = dll_head128; // 7
    seg_list[8] = dll_head144; // 8
    seg_list[9] = dll_head160; // 9
    seg_list[10] = dll_head256; // 10
    seg_list[11]= dll_head512; // 11
    seg_list[12] = dll_head1024; // 12
    seg_list[13] = dll_head2048; // 13
    seg_list[14] = dll_head4096; // 14

    return true;
}

// takes in the size and outputs the index of the corresponding seg list
int find_list(size_t size){
    if (size == 16){ 
        return 0;
    } else if (size == 32){
        return 1;
    } else if (size == 48){
        return 2;
    } else if (size == 64){
        return 3;
    } else if (size == 80){
        return 4;
    } else if (size == 96){
        return 5;
    } else if (size == 112){
        return 6;
    } else if (size == 128){
        return 7;
    } else if (size == 144){
        return 8;
    } else if (size == 160){
        return 9;
    } else if (size <= 256){
        return 10;
    } else if (size <= 512){
        return 11;
    } else if (size <= 1024){
        return 12;
    }else if (size <= 4096){
        return 13;
    } else{
        return 14;
    }

}

// takes a head and outputs block size, ie. payload size + header + footer.
size_t get_size(size_t* curr){
    return (*curr & 0xfffffffffffffff0);
}

// takes total block size and sets allocated to 1
size_t set_alloc(size_t size){
    return 0x1 | size;
}

// deletes a DLL node when size is exactly block size - returns nothing
void delete_node(size_t* curr, size_t size){
    dll_node_t* body = (dll_node_t*)curr;

    int list_num = find_list(size);

    if (seg_list[list_num] == body && body->next == body){  // case where node is head and it is the only node in list
        seg_list[list_num] = NULL;  // list is now empy
    }

    else if(seg_list[list_num] == body && body->next != body){  // case where node is head but it is not the only node
        seg_list[list_num] = body->prev;    // make next node head
    }

    body->prev->next = body->next;
    body->next->prev = body->prev;
}

// extends the heap by size amount - returns a pointer to payload
void* extend_heap(size_t size){
    size_t* new;
    if ((new = mm_sbrk(align(size + 16))) != (void*)-1){    // creates new space with for size + header + footer

        size_t* epi = new + (size/(sizeof(size_t)) + 1);
        *epi = 0x1;

        size_t* new1 = new - (1);
        *new1 = set_alloc(size+16);

        size_t* new_last = new + (size/sizeof(size_t));
        *new_last = set_alloc(size+16); 

        return new;
    }
    else{
        return NULL;
    }
}

// attempt to allocate size in current node (malloc). Returns payload address.
void* insert(size_t* curr, size_t size){
    
    size_t b_size = align(*curr & 0xfffffffffffffff0); //get size of current block

    if (b_size == size + 16){    // case where size fits exactly in current block (16 bytes)

        *curr = set_alloc(size+16);

        size_t* new_foot = curr + (((size)/sizeof(size_t))+1);
        *new_foot = *curr;

        delete_node(curr+1, b_size-16);

        return curr+1;
    }
    else if (b_size == size + 32){    // case where size fits exactly in current block (32 bytes)

        *curr = set_alloc(size+32);

        size_t* new_foot = curr + (((size+16)/sizeof(size_t))+1);
        *new_foot = *curr;

        delete_node(curr+1, b_size-16);

        return curr+1;
    }
    else if (b_size >= size+48){    // case where block size is large enough for size

        *curr = set_alloc(size+16); 

        size_t* new_foot = curr + ((size)/8 + 1);   // split at corresponding size
        *new_foot = *curr; 

        size_t* new_head = (new_foot + 1);  // new head to be freed
        *new_head = b_size - size - 16;

        new_foot = curr + ((b_size)/8 -1);
        *new_foot = *new_head;

        delete_node(curr+1, b_size-16); // deletes the DLL node that has been allocated
        free(new_head+1);   // frees the remaining space after split-allocate

        return curr + 1;
    }
    return NULL;
}

// coalesce adjacent free blocks. Returns pointer to new header. Takes in pointers to the current and next headers and sizes.
size_t* coal(size_t* curr, size_t* next, size_t* size_curr, size_t* size_next){
    size_t comb_size = (*size_next + *size_curr);
    *curr = comb_size & 0xfffffffffffffff0;
    size_t* next_foot = (next + (*size_next-8)/8);
    *next_foot = (comb_size & 0xfffffffffffffff0);

    return curr;
}

// add a new node to beginning of DLL - Takes in the pointer and size we want to store in the free list, returns nothing.
void dll_add_free(size_t* curr, size_t size){

    int list_num = find_list(size);
    
    if (seg_list[list_num] == NULL){    // initialize explicit free list (DLL)
        struct dll_node* new1 = (dll_node_t*)(curr+1);
        new1->next = new1;
        new1->prev = new1;

        seg_list[list_num] = new1;
    }
    else{   // add to beggining if already initialized
        struct dll_node* new_head = (dll_node_t*)(curr+1);
        struct dll_node* last = seg_list[list_num]->prev;

        new_head->next = seg_list[list_num];
        new_head->prev = last;

        last->next = new_head;

        seg_list[list_num]->prev = new_head;
        seg_list[list_num] = new_head;

    }
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    // IMPLEMENT THIS

    size_t* curr = NULL;
    size = align(size);

    int list_num = find_list(size); // find corresponding list index

    // iterate through the segregated lists
    while (list_num < 15){
        if (seg_list[list_num] != NULL){
            curr = (size_t*)(seg_list[list_num]) - 1;

            size_t* insertion = insert(curr, size); // attempt to insert at head of current list number
            if (insertion != NULL){
                assert(mm_checkheap(__LINE__)==true);
                return insertion;
            }
            
            if (seg_list[list_num]->next != seg_list[list_num]){    
                struct dll_node* curr_node = seg_list[list_num]->next;
                curr = (size_t*)curr_node - 1;

                while (curr_node != seg_list[list_num]){    // iterate through the current seg list (starting from head->next)

                    insertion = insert(curr, size);
                    if (insertion != NULL){
                        assert(mm_checkheap(__LINE__)==true);   //call to check heap consistency
                        return insertion;
                    }

                    curr_node = curr_node->next;
                    curr = (size_t*)curr_node - 1;
                    }
            }
        }
        list_num = list_num + 1;
    }

    size_t* new = extend_heap(size);

    return new;
}

/*
 * free
 */
void free(void* ptr)
{

    // IMPLEMENT THIS
    size_t* curr = ptr;
    size_t* head = curr-1;
            
    size_t b_size = get_size(head); 
    
    *head = b_size; // sets to free
    
    size_t* foot = curr + ((b_size-16)/8);   // sets footer according to head
    *foot = b_size; 

    size_t b_size_left = get_size(head-1);
    size_t* left = curr - b_size_left/8 -1;
    curr = head;
    size_t* right = foot +1; 

    size_t alloc_left = *left & 0xf;    // get allocations
    size_t alloc_curr = *curr & 0xf;
    size_t alloc_right = *right & 0xf;

    size_t b_size_curr = get_size(curr);    // get sizes
    size_t b_size_right = get_size(right);

    if (alloc_left == 0 && alloc_curr == 0){    // case where we need to coalesce with already-free left block
        delete_node(left+1, b_size_left-16);
        curr = coal(left, curr, &b_size_left, &b_size_curr);
        b_size_curr = get_size(curr);
        alloc_curr = *curr & 0xf;
        
        if (alloc_curr == 0 && alloc_right == 0){   // case where we need to coalesce with already-free right block (left was also free)
            curr = coal(curr, right, &b_size_curr, &b_size_right);
            b_size_curr = get_size(curr);
            delete_node(right+1, b_size_right-16);
            dll_add_free(curr, b_size_curr-16);
            return;
        }
        else{
            b_size_curr = get_size(curr);
            dll_add_free(curr, b_size_curr-16);
            return;
        }
    }
    if (alloc_curr == 0 && alloc_right == 0){   // case where we need to coalesce with already-free right block (left was not free)
        b_size_curr = get_size(curr);
        curr = coal(curr, right, &b_size_curr, &b_size_right);
        b_size_curr = get_size(curr);
        delete_node(right+1,b_size_right-16);
        dll_add_free(curr, b_size_curr-16);
        return;
        }

    dll_add_free(curr, b_size_curr-16); // case for no coalescing
    return;
}


/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    // IMPLEMENT THIS

    void* retval;
    if (oldptr == NULL){    
        retval = malloc(align(size));
        return retval;
    }
    if (size == 0){
        free(oldptr);
        return oldptr;
        }
    else{
        size = align(size);

        size_t* og_head = (size_t*)oldptr -1;   // get header and footer sizes, allocations
        size_t og_size = get_size(og_head);
        size_t* og_foot = og_head + (og_size-16)/8 + 1;

        size_t* og_next = og_foot + 1;
        size_t og_next_size = get_size(og_next);
        size_t* og_next_foot = og_next + (og_next_size-16)/8 + 1;
        size_t og_next_alloc = *og_next & 0xf;

        if (size+16 == og_size){    // case where size fits exactly in previously allocated block (16 bytes)

            *og_head = *og_head | 0x1;   // set head to allocated
            *og_foot = *og_head;    // match header and footer
            
            return og_head + 1;
        }

        if (size+32 == og_size){    // case where size fits exactly in previously allocated block (16 bytes)
            
            *og_head = *og_head | 0x1;   // set head to allocated
            *og_foot = *og_head;    // match header and footer
            
            return og_head + 1;
        }
        else if(size+48 <= og_size){    // case where previously allocated block is enough for size

            *og_head = (size+16) | 0x0000000000000001;
            size_t* new_foot = og_head + size/8 + 1;
            *new_foot = *og_head;   // split the space 
            size_t* new_head = new_foot + 1;
            *new_head = (og_size - 16 - size);
            *og_foot = *new_head;

            free(new_head+1);

            return og_head +1;
        }
        else{   
            if (og_size + og_next_size == size + 16 && og_next_alloc == 0){     // case where size fits perfect in curr + next

                delete_node(og_next+1, og_next_size-16);

                *og_head = (size+16) | 0x0000000000000001;
                *og_next_foot = *og_head;
                
                return og_head + 1;
            }
            if (og_size + og_next_size == size + 32 && og_next_alloc == 0){     // case where size fits perfect in curr + next

                delete_node(og_next+1, og_next_size-16);

                *og_head = (size+32) | 0x0000000000000001;
                *og_next_foot = *og_head;
                
                return og_head + 1;
            }
            if (og_size + og_next_size - 16 > size && og_next_alloc == 0){      // case where curr + next is enough for size
                
                *og_head = (size+16) | 0x0000000000000001;  
                size_t* new_foot = og_head + size/8 + 1;

                delete_node(og_next+1, og_next_size - 16);
                *new_foot = *og_head;
                size_t* new_head = new_foot + 1;
                
                *new_head = (og_size + og_next_size - 16 - size);
                *og_next_foot = *new_head;

                free(new_head + 1);

                return og_head + 1;
            }
            else{   // create new space and move all existing data to this new space, then free previously allocated block

                size_t* retval = extend_heap(size);

                memcpy(retval, og_head + 1, og_size-16);
                
                free(oldptr);

                return retval;
                
            }
        }
    }
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mm_heap_hi() && p >= mm_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 * You call the function via mm_checkheap(__LINE__)
 * The line number can be used to print the line number of the calling
 * function where there was an invalid heap.
 */
bool mm_checkheap(int line_number)
{
#ifdef DEBUG
    // Write code to check heap invariants here
    // IMPLEMENT THIS
    
    size_t* curr = first + 8;
    size_t* next = curr + 2;
    
    // Iterate through the entire heap: Invariants #1 - #4
    while (*next != 0x1){

        size_t b_size_curr = get_size(curr);
        size_t b_size_next = get_size(next);
        size_t alloc_curr = *curr & 0x0000000000000000f;
        size_t alloc_next = *next & 0x0000000000000000f;

        size_t* foot = curr + (b_size_curr-8)/8;

        if (alloc_curr == 0x0){

            // INVARIANT #1: Is Header equal to header? NOTE: only check when free considering footer optimization
            if (*curr != *foot){
                return false;
            }
            

            int listnum = find_list(b_size_curr-16);
            dll_node_t* free_node = (dll_node_t*)(curr +1);
            if (seg_list[listnum] == 0x0){
                return false;
            }
            struct dll_node* curr_node = seg_list[listnum]->next;
            curr = (size_t*)curr_node - 1;

            while (curr_node != seg_list[listnum]){
                if (curr_node == free_node){
                    break;
                }
                // INVARIANT #2: Are all free blocks in the segregated free lists?
                if (curr_node->next == seg_list[listnum] && seg_list[listnum] != free_node){
                    return false;
                }
                curr_node = curr_node->next;
                curr = (size_t*)curr_node - 1;
            }
        }

        // INVARIANT #3: Is header garbage? ie. there is/are overlapping data/blocks
        if (alloc_curr != 0x0 && alloc_curr != 0x1 && alloc_curr != 0x10 && alloc_curr != 0x11){
            return false;
        }

        // INVARIANT #4: Are there two contiguous free blocks that have not been coalesced?
        if (alloc_curr == 0 && alloc_next == 0){
            return false;
        }

        //INVARIANT #5: Are all headers actually in the heap?
        if (!in_heap(curr)){
            return false;
        }

        curr = next;
        next = curr + b_size_next/8;

    }
    int list_num = 0;

    // iterate through the segregated lists. Invariantes #6 - #7
    while (list_num < 15){
        if (seg_list[list_num] != NULL){
            curr = (size_t*)(seg_list[list_num]) - 1;
            
            if (seg_list[list_num]->next != seg_list[list_num]){
                struct dll_node* curr_node = seg_list[list_num]->next;
                curr = (size_t*)curr_node - 1;

                while (curr_node != seg_list[list_num]){

                    size_t alloc = *curr & 0x000000000000000f;

                    // INVARIANT #6: Do all pointers in the segregated free lists point to an actual free block?
                    if (alloc != 0){
                        return false;
                    }

                    // INVARIANT #7: Do next and prev pointers point to each other?
                    if (curr_node != curr_node->next->prev){
                        return false;
                    }

                    curr_node = curr_node->next;
                    curr = (size_t*)curr_node - 1;
                    }
            }
        }
        list_num = list_num + 1;
    }
    
    
#endif // DEBUG
    return true;
}
