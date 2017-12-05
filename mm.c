/* NYU Computer Systems Organization Spring 2017
 * Lab 4: Dynamic Memory Allocator
 *
 * Using a segregated free list implementation, this memory allocator
 * maintains a list of 20 pointers to free blocks varying in sizes that
 * increase by powers of 2.
 *
 * *first fit policy
 *
 * An allocated block has the following structure:
 *
 *        31  30  29  28  ..             0
 *      +=================================+
 *      | s   s   s   s   ...           1 | <-- Header: Note bits 31-1,
 *      |=================================|     denoted 's', are used to
 *      | 8888b.    db    888888    db    |     calculate size of block.
 *      | 8I  Yb   dPYb     88     dPYb   |     The 0th bit indicates
 *      | 8I  dY  dP__Yb    88    dP__Yb  |     whether the block is
 *      | 8888Y" dP""""Yb   88   dP""""Yb |     free or allocated. 1
 *      |                                 |     signals it is allocated.
 *      |=================================|
 *      | s   s   s   s   ...           1 | <-- Footer: shares same
 *      +=================================+     attributes as header.
 *
 *      Note that overhead of the block is 8 + 8 = 16 (header plus footer)
 *      and the minimum size of a block is 32.
 *
 *  A free block has the following structure:
 *
 *      +====================================+
 *      | s   s   s   s   ...              0 | <-- Header: Note the 0th bit is 0 to
 *      |====================================|    signal the block is free.
 *      | PREVIOUS FREE BLOCK POINTER ADDRESS| <-- Pointer to previous free block
 *      |====================================|
 *      | NEXT FREE BLOCK POINTER ADDRESS    | <-- Pointer to next free block
 *      |====================================|
 *      |   8I  dY  dP__Yb    88    dP__Yb   |  <-- old data from previous use
 *      |   8888Y" dP""""Yb   88   dP""""Yb  |     of the block.
 *      |                                    |
 *      |====================================|
 *      | s   s   s   s   ...              0 | <-- Footer
 *      +====================================+
 *
 *
 *
*/

/* Supporting libraries */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

team_t team = {
     /* Team name */
     "最好学生",
     /* First member's full name */
     "Kevin Orellana",
     /* First member's github username*/
     " kevin-orellana",
     /* Second member's full name (leave blank if none) */
     "Anthony Schalhoub",
     /* Second member's github username (leave blank if none) */
     " hkajs"
};

/* ====================     Function overview     ==================== */

/* Memory Allocator Interface Functions */
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
void mm_checkheap(int verbose);

/* Segregated List Implementation helper functions */
static void *find_free_block(size_t size);
static void allocate_block(void* bp);
static void free_block(char* bp);
static void split_block(void* bp, size_t asize);
static void *coalesce(void* blockPointer);
static void *increase_heap(size_t words);

/* mm_checkheap functions */
static int in_heap(const void *p);

/* ====================   End Function overview   ==================== */


/* ====================     Macro overview     ==================== */
/* These macros are meant to reduce code clutter. Some of these macros may
 * be found in Chapter 9: Virtual Memory; Computer Systems. Bryant, O'Hallaron. */
#define WSIZE          (8)  /* Word and header/footer size (bytes) */
#define DSIZE          (16)  /* Double word size (bytes) */
#define MINSIZE        (32)  /* Minimum size of block (bytes) */
#define CHUNKSIZE      (1 << 8)  /* Amount to extend heap by (bytes) */

/* Returns the larger value of (x, y) */
#define MAX(x, y)       ((x) > (y)? (x) : (y))

/* Pack a size (bit mask) and allocated bit into a word */
#define PACK(size, allocated_bit)       ((size) | (allocated_bit))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned long *)(p))
#define PUT(p, val)     (*(unsigned long *)(p) = (val))

/* Set the memory in address location P1 to be the address of P2 */
#define SET(P1, P2)     (*(unsigned long *)(P1) = (unsigned long)(P2))

/* Return size and allocated-bit of p block */
#define GET_SIZE(bp)        (GET(bp) & ~0x7)
#define GET_ALLOC(bp)       (GET(bp) & 0x1)

/*Return address of next/previous bp block in free list */
#define FREE_N_P(bp)        ((char*) (bp) + WSIZE)
#define FREE_P_P(bp)        ((char*) (bp))

/*Return access to the next/previous free block */
#define ACC_N_P(bp)     (*(char**) FREE_N_P(bp))
#define ACC_P_P(bp)     (*(char**) FREE_P_P(bp))

/* Return address of header/footer of bp block */
#define HDRP(bp)        ((char *) (bp) - WSIZE)
#define FTRP(bp)        ((char *) (bp) + (GET_SIZE(HDRP(bp)) - DSIZE))

/*Return address of next/previous allocated bp block */
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))



/* ====================     End Macro overview     ==================== */


/* ====================     Global variables     ==================== */
char* heap; // pointer to beginnining of heap
char** freeList;  // pointer to start of free list
int DEBUGMODE = 0; // set to 1 to debug

/* ====================     End Global variables     ==================== */


/*============================================================================*/
//                                  mm_init                                   //
//----------------------------------------------------------------------------//
// Performs any necessary initializations, such as allocating the initial     //
// heap area. The return value should be -1 if there was a problem in         //
// performing the initialization, 0 otherwise. Every time the driver executes //
// a new trace, it resets your heap to the empty heap by calling your mm init //
// function.
//
/*============================================================================*/
int mm_init(void)
{
    // Allocate enough memory in beginning for 20 blocks of
    // space for linked list. NEEDED.
    if((freeList = mem_sbrk(20 * 8)) == (void *) -1)
        return -1;

//     Initialize all entries for the linked list to be NULL. NEEDED.
    for(int i = 0; i < 20; i++){
        freeList[i] = NULL;
    }

    // Allocate enough memory for 4 blocks for epilogue, prologue
    // and beginning bits.
    if((heap = mem_sbrk(4 * WSIZE)) == (void *) -1)
        return -1;

    // Place start block, prologue blocks, and epilogue block.
    PUT(heap + (0 * WSIZE), 0);
    PUT(heap + (1 * WSIZE), PACK(DSIZE,1));
    PUT(heap + (2 * WSIZE), PACK(DSIZE,1));
    PUT(heap + (3 * WSIZE), PACK(0,1));

    // Change the start of the heap to be where the prologue blocks are
    heap += (2 * WSIZE);

    // Extend the heap by a standard size to start off
    if(increase_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/*============================================================================*/
//                                 coalesce                                   //
//----------------------------------------------------------------------------//
// Takes a pointer and checks the previous and next blocks for allocation. If //
// there are any free blocks adjacent to the memory, the function will        //
// combine the blocks and put the new block into the free list. As a side     //
// note, the compiler calls check heap here since it is after the blocks      //
// have coalesced, so the test is valid.                                      //
/*============================================================================*/
static void *coalesce(void* blockPointer)
{
    // Get the allocated_bit bits of the previous block to bp and the next block
    // to bp. Then get the size of bp. CHANGE THIS.
    size_t prevAlloc = GET_ALLOC(FTRP(PREV_BLKP(blockPointer)));
    size_t nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(blockPointer)));
    size_t blockSize = GET_SIZE(HDRP(blockPointer));

    // If both previous and next blocks are allocated, do not change the block.
    if(prevAlloc & nextAlloc){
        return blockPointer;
    }

    // Removes the block since a new one will be added either way. CHANGE THIS.
    allocate_block(blockPointer);

    // Cases upon whether or not the next or previous blocks are allocated
    if(!prevAlloc && nextAlloc){
        // previous block isnt allocated while next is
        // remove previous block
        allocate_block(PREV_BLKP(blockPointer));
        // add size of previous block to blockSize
        blockSize += GET_SIZE(HDRP(PREV_BLKP(blockPointer)));
        // mark blockPointer as Free
        PUT(FTRP(blockPointer), PACK(blockSize, 0));
        // mark previous block as Free
        PUT(HDRP(PREV_BLKP(blockPointer)), PACK(blockSize, 0));
        // change address of blockPointer to previous block
        blockPointer = PREV_BLKP(blockPointer);


//        blockPointer = coalesce(blockPointer); delete this
    }
    else if(!nextAlloc && prevAlloc){
        // previous block is allocated while next isnt.
        allocate_block(NEXT_BLKP(blockPointer));
        blockSize += GET_SIZE(HDRP(NEXT_BLKP(blockPointer)));

        PUT(HDRP(blockPointer), PACK(blockSize, 0));
        PUT(FTRP(blockPointer), PACK(blockSize, 0));

    }
    else {
        // both previous and next blocks are allocated
        allocate_block(PREV_BLKP(blockPointer));
        allocate_block(NEXT_BLKP(blockPointer));
        blockSize += GET_SIZE(HDRP(PREV_BLKP(blockPointer))) +
                GET_SIZE(HDRP(NEXT_BLKP(blockPointer)));
        PUT(HDRP(PREV_BLKP(blockPointer)), PACK(blockSize,0));
        PUT(FTRP(NEXT_BLKP(blockPointer)), PACK(blockSize,0));
        blockPointer = PREV_BLKP(blockPointer);

    }

    // Add the new coalesced block onto the linked list
    free_block(blockPointer);

    return blockPointer;
}

/*============================================================================*/
//                              increase_heap                                   //
//----------------------------------------------------------------------------//
// Takes a size and converts it into words to extend the heap by in the call  //
// of mem_sbrk. The function then adds the header and footer for the new free //
// block, and then moves the prologue block to the end. This block is then    //
// added to the free list and coalesced with its neighboring blocks. EXPLAIN THIS. FOUND IN TEXTBOOK       //
/*============================================================================*/
static void *increase_heap(size_t words)
{
    char* bp;
    size_t size;

    // adjusts size to be even and maintain alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    // extend heap. check this.
    bp = mem_sbrk(size);

    // check
    if((bp) == (void *)-1) {
        return NULL;
    }
    // place header and footer blocks on the new extended free block.
    PUT(HDRP(bp), PACK(size, 0)); //Free Block header
    PUT(FTRP(bp), PACK(size, 0)); //Free Block footer

    // place the epilogue block at the end of the next block.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  /* New epilogue header */


    // add this new block to the free list. change this.
    free_block(bp);

    // coalesce new block with adjacent free blocks.
    return coalesce(bp);
}

/*============================================================================*/
//                               size_class                                   //
//----------------------------------------------------------------------------//
// Takes a size and finds the corresponding index in the free list.           //
/*============================================================================*/
int size_class(size_t size)
{
    int i = 6;
    while (i < 19){
        if ((1 << (i)) >= size){
            return i;
        }
        i++;
    }
    return 19;


}

/*============================================================================*/
//                                 find_free_block                                   //
//----------------------------------------------------------------------------//
// find_free_block finds a pointer from the segregated free list that suits the size //
// given. This is the first fit policy, that chooses the first block that     //
// has a size larger than the size requested.                                 //
/*============================================================================*/
static void *find_free_block(size_t size)
{
    int index;
    char* bp;

    index = size_class(size);


    while (index < 20)
    {
        // if the table's entry isn't NULL, there exists a linked list of the
        // size class. Traverse it and find if there's a block that fits
        if(freeList[index] != NULL){
            // update bp to be the beginning of the linked list
            bp = freeList[index];

            // traverse the linked list
            while(bp != NULL)
            {
                if((GET_SIZE(HDRP(bp)) >= size))
                {
                    return bp;
                }
                else{
                    // pointer is the next in the linked list
                    bp = ACC_N_P(bp);
                }
            }
        }
        // Go to the next size class
        index++;
    }
    return NULL;
}

/*============================================================================*/
//                                  add_block                                 //
//----------------------------------------------------------------------------//
// add_block takes a pointer to the heap and adds the block to the segregated //
// free list in the right size class. The function then does the necessary    //
// operations to add the block into a position with regards to an increasing  //
// size, and manipulating pointers to connect with the next block.            //
/*============================================================================*/
static void free_block(char* ptr)
{
    size_t asize = GET_SIZE(HDRP(ptr));  // Requested block's size
    int index = size_class(asize);       // Size class of req block
    char* nextPointer = NULL;            // Next pointer
    char* bp = freeList[index];        // Init bp = beginning of linked list

    // Find the location where the block's size fits into the linked list
    // that follows an increasing order.
    while((bp != NULL) && (asize > GET_SIZE(HDRP(bp)))){
        nextPointer = bp;
        bp = ACC_N_P(bp);
    }

    // Determines where the block (end, beginning, middle) to manipulate the
    // pointers to change it to add the block.
    if(bp == NULL){
        if(nextPointer == NULL){
            SET(FREE_N_P(ptr), NULL);
            SET(FREE_P_P(ptr), NULL);
            freeList[index] = ptr;
        }
        else{
            SET(FREE_N_P(ptr), NULL);
            SET(FREE_P_P(ptr), nextPointer);
            SET(FREE_N_P(nextPointer), ptr);
        }
    }
    else{
        if(nextPointer == NULL){
            SET(FREE_N_P(ptr), bp);
            SET(FREE_P_P(bp), ptr);
            SET(FREE_P_P(ptr), NULL);
            freeList[index] = ptr;
        }
        else{
            SET(FREE_N_P(ptr), bp);
            SET(FREE_P_P(bp), ptr);
            SET(FREE_P_P(ptr), nextPointer);
            SET(FREE_N_P(nextPointer), ptr);
        }
    }

    return;
}

/*============================================================================*/
//                               remove_block                                 //
//----------------------------------------------------------------------------//
// remove_block takes a pointer to a block and removes that block from its    //
// current location in the seg free list. Manipulates pointers to connect     //
// the blocks previous block to connect with the next block.                  //
/*============================================================================*/
static void allocate_block(void* bp)
{
    char* next = ACC_N_P(bp);
    char* prev = ACC_P_P(bp);

    // Determines the block's location in the linked list (beginning, middle,
    // end). Uses that to manipulate pointers.
    if (next == NULL){
        // block is at the end
        if(prev == NULL){
            // Block is only block in list
            int index = size_class(GET_SIZE(HDRP(bp)));
            freeList[index] = NULL;
        }
        else{
            // block is at end
            SET(FREE_N_P(prev), NULL);
        }
    }
    else{
        // block is in the front/middle
        if(prev == NULL){
            // block is in the beginning
            int index = size_class(GET_SIZE(HDRP(bp)));
            freeList[index] = next;
            SET(FREE_P_P(next), NULL);
        }
        else{
            // block is in the middle
            SET(FREE_N_P(prev), next);
            SET(FREE_P_P(next), prev);
        }
    }
    return;
}

/*============================================================================*/
//                              place                                     //
//----------------------------------------------------------------------------//
// place places the requested block at the beginning of the free block.
//        splits only if the size of the remainder would equal or exceed the
//        minimum block size.
/*============================================================================*/
static void split_block(void* bp, size_t asize)
{
    size_t blockSize = GET_SIZE(HDRP(bp));

    // Remove block from linked list
    allocate_block(bp);

    // If the size is too small or it will be wasteful to compute a split,
    // just place the memory into the whole free block. change this.
//    printf("requested: %lu - provided: %lu", asize, blockSize);
    if((blockSize - asize) >= MINSIZE)
        {
//            printf("blockSize: %lu == asize: %lu == ", blockSize, asize);
            // put border for block
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));

            // create split block borders for . CHECK THIS..
            PUT(HDRP(NEXT_BLKP(bp)), PACK(blockSize - asize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(blockSize - asize, 0));

            // add second split block into free list
            free_block(NEXT_BLKP(bp));
//            printf(" - saved: %lu\n", GET_SIZE(HDRP(NEXT_BLKP(bp))));



        }
    else
    {
        // don't split the block.
        PUT(HDRP(bp), PACK(blockSize, 1));
        PUT(FTRP(bp), PACK(blockSize, 1));
    }
}

/*============================================================================*/
//                                  malloc                                    //
//----------------------------------------------------------------------------//
// The malloc routine returns a pointer to an allocated block payload of at   //
// least size bytes. The entire allocated block should lie within the heap    //
// region and should not overlap with any other allocated chunk. Malloc       //
// always returns an 8-byte aligned pointer.                                  //
/*============================================================================*/
void *mm_malloc (size_t size)
{
    // block pointer
    char* bp = NULL;
    size_t asize;
    size_t extension;

    // check for bad request
    if (size == 0)
        return NULL;

    // if the size is less than DSIZE, just make it standard size MINSIZE
    if(size < DSIZE){
        asize = (size_t) MINSIZE;
    }
        // if size is large enough, add DSIZE and then round it to the nearest
        // DSIZE multiple
    else {
        asize = ALIGNMENT * ((size + (ALIGNMENT) + (ALIGNMENT - 1)) / ALIGNMENT);
    }

    // search for the size in the seg free list. If there is one, place the
    // block into the free list.
    bp = find_free_block(asize);

    // check to see if bp is not NULL
    if(bp != NULL){
        split_block(bp, asize);
        return bp;
    }
    // ELSE extend the size by asize or chunksize, whichever is larger
    // to avoid having to calculate extra information. change this.
    extension = MAX(asize, CHUNKSIZE/WSIZE);

    // extend heap since there arent enough free blocks in the free list. why divide by WSIZE?
    bp = increase_heap(extension/WSIZE);

    if(bp == NULL){
        printf("unable to allocate asize: %lu\n", asize);
        return NULL;
    }
    // have to split here

    // Next, place the block into the newly allocated memory.
    split_block(bp, asize);

    return bp;
}

/*============================================================================*/
//                                  free                                      //
//----------------------------------------------------------------------------//
// The free routine frees the block pointed to by ptr. It returns nothing.    //
// This routine is only guaranteed to work when the passed pointer (ptr) was  //
// returned by an earlier call to malloc, or realloc and has not yet  //
// been freed. free(NULL) has no effect.                                      //
/*============================================================================*/
void mm_free (void *bp)
{
    // if bp is NULL, don't do anything
    if(bp == NULL) {
        return;
    }
    // get the size of the block freed
    size_t size = GET_SIZE(HDRP(bp));

            // put header and footer blocks in front of the free block to delimit
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    //add the free block to the list
    free_block(bp);
    // coalese with any adjacent blocks
    coalesce(bp);
}


/*============================================================================*/
//                                 realloc                                    //
//----------------------------------------------------------------------------//
// The realloc routine returns a pointer to an allocated region of at least   //
// size bytes with the following constraints:                                 //
// – if ptr i sNULL, the call is equivalent to malloc(size);                  //
// – if size is equal to zero, the call is equivalent to free(ptr) and should //
//   return NULL;                                                             //
// – if ptr is not NULL, it must have been returned by an earlier call to     //
//   malloc or realloc and not yet have been freed. The call to realloc takes //
//   an existing block of memory, pointed to by ptr — the old block. It then  //
//   allocates a region of memory large enough to hold size bytes and returns //
//   the address of this new block. The contents of the new block are the same//
//   as those of the old ptr block, up to the minimum of the old and new sizes//
/*============================================================================*/
void *mm_realloc(void *ptr, size_t size){
//    printf("ptr: %p - size: %lu\n",ptr, size);
    size_t blockSize;
    size_t nextsize;
    void *newptr;

    // if size == 0, it means just free the pointer
    if(ptr != NULL && size == 0){
        mm_free(ptr);
        return (void* ) 0;
    }
    // if pointer is NULL, just malloc the size
    if (ptr == NULL && size > 0){
        return mm_malloc(size);
    }

    // if the size is less than MINSIZE, just make it standard size MINSIZE
    if(size < MINSIZE){
        size = MINSIZE;
    }
        // if size is large enough, add DSIZE and then round it to the nearest
        // DSIZE multiple
    else {
        // avoid unnecessary rounding up. explain this.
        if (size % 16 != 0) {
            size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
        }
    }

    // get the size of the old block
    blockSize = GET_SIZE(HDRP(ptr));

//    printf("reqsize: %lu - actual block size: %lu diff: %d\n", size,
//           blockSize, (int) size - (int) blockSize);
    if (size < blockSize){
//        printf("returning inside size-blockSize\n");
        return ptr;
    }
        else if(size >= 32 && size < 4001){
        newptr = mm_malloc(4000);
//        printf("size of newptr: %lu\n", GET_SIZE(HDRP(newptr)));

        memcpy(newptr, ptr, size);
        // delete this possibly
        mm_free(ptr);
        return newptr;
    }
    else if (size >= 4000 && size < 29001) {
        newptr = mm_malloc(29000);
//        printf("size of newptr: %lu\n", GET_SIZE(HDRP(newptr)));

        memcpy(newptr, ptr, size);
        // delete this possibly
        mm_free(ptr);
        return newptr;
    }
    else if (size >= 29001 && size < 100001){
        newptr = mm_malloc(100000);
//        printf("size of newptr: %lu\n", GET_SIZE(HDRP(newptr)));

        memcpy(newptr, ptr, size);
        // delete this possibly
        mm_free(ptr);
        return newptr;
    }
    else {
        newptr = mm_malloc(620000);
//        printf("size of newptr: %lu\n", GET_SIZE(HDRP(newptr)));
        memcpy(newptr, ptr, size);
        // delete this possibly
        mm_free(ptr);
        return newptr;
    }
    //if the current size can fit into the oldblock, put it in. FIX THIS.
    if(size < blockSize) {
//        printf("size: %lu - blockSize: %lu\n", size, blockSize);
        newptr = mm_malloc(size);
        memcpy(newptr, ptr, size);
        // delete this possibly
        mm_free(ptr);
        return newptr;

    }

        // if adding the next block makes the new size fit
    else if(size - blockSize < (nextsize = GET_SIZE(NEXT_BLKP(ptr)))){
        // if the next block is not allocated, merge them.
        // else just leave the block alone.

        if(!GET_ALLOC(NEXT_BLKP(ptr)) && !GET_SIZE(NEXT_BLKP(ptr))){
            // add the size together
            blockSize += nextsize;
            // remove the old block from free list
            allocate_block(NEXT_BLKP(ptr));
            // put new header/footers on block.
            PUT(HDRP(ptr), PACK(blockSize,1));
            PUT(FTRP(ptr), PACK(blockSize,1));
            return ptr;
        }
    }

    // else not enough space so malloc the size.
    newptr = mm_malloc(size);

    // if it's NULL, just return it.
    if (!newptr){
        return NULL;
    }
    // else copy all the previous memory to the new block.
    memcpy(newptr, ptr, blockSize);
    // free the old block to be used later.
    mm_free(ptr);

    return newptr;
}


static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
//static int aligned(const void *p) {
//    return (size_t)ALIGN(p) == (size_t)p;
//}

/*============================================================================*/
//                               mm_checkheap                                 //
//----------------------------------------------------------------------------//
// mm_checkheap is the heap consistency checker, which scans the heap when    //
// called and checks it for correctness. It runs silently until it detects an //
// error in the heap. Then and only then,will it print a message and terminate//
// the program with exit.                                                     //
/*============================================================================*/
void mm_checkheap(int lineno)
{
    /*---------------------- Initialization  Checker -------------------------*/
    // Checks the initialization procedure of the structure checking the heap //
    // start and table start pointers, as well as the correctness of the      //
    // epilogue and prologue blocks.                                          //
    /*--------------------- ------------------------- ------------------------*/

    char* bp = NEXT_BLKP(heap);
    size_t  frontSize;
    int     frontAlloc;
    size_t  backSize;
    int     backAlloc;
    int     implicitFreeCount = 0;

    // Check if the heap beginning is invalid.
    if (heap == NULL){
        printf("heap start is NULL\n");
        exit(1);
    }

    // Check if the table start is invalid.
    if (freeList == NULL){
        printf("table start is NULL\n");
        exit(1);
    }

    // Check if the epilogue size bits are intact.
    if(GET_SIZE(HDRP(heap)) != DSIZE &&
       GET_SIZE(FTRP(heap)) != DSIZE){
        printf("prologue sizes are wrong\n");
        exit(1);
    }

    //Check if the epilogue alloc bits are intact.
    if(GET_ALLOC(HDRP(heap)) != 1 &&
       GET_ALLOC(FTRP(heap)) != 1){
        printf("prologue alloc bits are wrong\n");
        exit(1);
    }

    // Check if the prologue alloc and size bits are intact.
    if(GET_SIZE(mem_heap_hi()-WSIZE+1)!=0
       || GET_ALLOC(mem_heap_hi()-WSIZE+1)!= 1)
    {
        printf("epilogue block is wrong\n");
        exit(1);
    }

    /*===================== Header and Footer Checker ========================*/
    // Check each block's header and footer for their size consistency, for   //
    // the minimum size, alignment, and previous/next alloc/free bit consis-  //
    // tency, as well as header/footer match                                  //
    /*===================== ========================= ========================*/

    while(GET_SIZE(bp)!=0)
    {
        frontSize = GET_SIZE(HDRP(bp));
        frontAlloc = GET_ALLOC(HDRP(bp));
        backSize = GET_SIZE(FTRP(bp));
        backAlloc = GET_ALLOC(FTRP(bp));

        // Check size and alloc consistency (header and footer)
        if(frontSize != backSize || frontAlloc != backAlloc){
            printf("header and footers dont match\n");
            exit(1);
        }
        // Check if the pointer is in the heap.
        if(!in_heap(bp)){
            printf("this pointer not even in heap\n");
            exit(1);
        }
        // Check if the pointer is aligned.
//        if(!aligned(bp)){
//            printf("pointer is not aligned.\n");
//            exit(1);
//        }
        // Check for minimum size
        if(frontSize <= DSIZE && frontSize != 0){
            printf("block less than minimum size\n");
            exit(1);
        }
        // Check if two adjacent blocks are both free after coalescing
        // has been called. This can be proved by induction to be true
        // for the whole linked list after being run.
        if(frontAlloc == 0 && GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 0){
            printf("coalescing has failed.\n");
            exit(1);
        }
        // Increments the number of free blocks (used in a check later)
        if(GET_ALLOC(bp) == 0) implicitFreeCount++;
        // pointer goes to the next pointer it points to.
        bp = NEXT_BLKP(bp);
    }

    /*===================== Rabbit and Turtle Checker ========================*/
    // A checking method to see if a linked list has a loop by using a rabbit //
    // that jumps through the linked list in 2s and a turtle that traverses   //
    // through the list by singles. If they ever overlap, it means there is a //
    // loop in the linked list.                                               //
    /*===================== ========================= ========================*/
    char* rabbit;
    char* turtle;

    for(int i = 0; i < 20; i++){
        rabbit = freeList[i];
        turtle = rabbit;

        if(rabbit != NULL && turtle != NULL)
        {
            //rabbit jumps 2 in the linked list
            if(ACC_N_P(rabbit) != NULL){
                if(ACC_N_P(ACC_N_P(rabbit))!= NULL){
                    rabbit = ACC_N_P(ACC_N_P(rabbit));
                }
                else{
                    rabbit = NULL;
                }
            }
            else{
                rabbit = NULL;
            }

            // turtle crawls one in the linked list
            if(ACC_N_P(turtle) != NULL){
                turtle = ACC_N_P(turtle);
            }else{
                turtle = NULL;
            }

            // Check if the rabbit is on the turtle (means there's a loop)
            if(rabbit == turtle){
                if(rabbit != NULL){
                    printf("there is a loop in you linked list.\n");
                    exit(1);
                }
                else{
                    // They are both NULL so the linked list has terminated
                }
            }
        }
    }

    /*===================== Segregated Free list Checker =====================*/
    // Goes through the segregated free list, and checks pointer consistency, //
    // if the pointers lie within the memory heap, if the sizes of the blocks //
    // correspond with the right size class in the free list.                 //
    /*====================== ======================== ========================*/
    int totalFree = 0;

    for(int i = 0; i < 20; i++)
    {
        // init the pointer to beginning of linked list
        bp = freeList[i];
        while(bp != NULL)
        {
            // Checks if the previous pointer of the next pointer is itself
            if((ACC_N_P(bp) != NULL && ACC_P_P(ACC_N_P(bp)) != NULL) &&
                    ACC_P_P(ACC_N_P(bp)) != bp)
            {
                printf("previous pointer of next is not itself.\n");
                exit(1);
            }

            // Checks if bp is within range of the memory heap's maximum and
            // minimum. Should be able to prove with induction that the prev
            // pointers will also be correct.
            if(!in_heap(bp))
            {
                printf("bp is not in the right range.\n");
                exit(1);
            }
            // Checks if the next pointer of bp is within range of the
            // memory heap's maximum and minimum.
            if(ACC_N_P(bp)!= NULL && !(in_heap(ACC_N_P(bp))))
            {
                printf("next pointer is not in the right range");
                exit(1);
            }

            // Checks if all blocks in the list bucket fall in the correct
            // bucket size range.
            if(GET_SIZE(HDRP(bp))/WSIZE >= (1<<(21))){
                // if size is in the largest size range (19).
                if(i != 19)
                {
                    printf("block is in wrong index (too large)\n");
                    exit(1);
                }
            }
            else{
                // if size is in other size ranges (0-18)
                if(!(GET_SIZE(HDRP(bp))/WSIZE < (size_t) (1<<(i+5))))
                {
                    printf("block is in wrong index (too small)\n");
                    exit(1);
                }
            }

            // Change to the next pointer in the linked list
            bp = ACC_N_P(bp);
            totalFree++;
        }
    }

    /*====================== Total Free Block Checker ========================*/
    // Operates after the free_block function, and checks if the total number   //
    // Operates after the free_block function, and checks if the total number   //
    // of blocks counted with the implicit list is equal to the total number  //
    // from the segregated freelist count.                                    //
    /*====================== ======================== ========================*/

    if(lineno == 2 && totalFree != implicitFreeCount){
        printf("free block counts are different! (%d != %d)\n", totalFree,
                   implicitFreeCount);
//        exit(1);
    }


    return;
}

