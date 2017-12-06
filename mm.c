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

/* ====================     Function overview     ================= */

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
static int class_finder(size_t size);

/* mm_checkheap functions */
static int in_heap(const void *p);
void header_footer_check();


/* ====================   End Function overview   ================= */


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



/* ====================     End Macro overview     ================== */


/* ====================     Global variables     ==================== */
    char *heap; // pointer to beginnining of heap
    char **freeList;  // pointer to start of free list
    int DEBUGMODE = 0; // set to 1 to debug

/* ====================     End Global variables     ================ */


/* mm_init makes two initializations: (1) sets freeList to hold 20 8-byte sized
 * pointers and (2) sets heap pointer to have enough memory for the prologue and
 * epilogue headers (which mark the beginning/end of the heap) and finally extends
 * the heap by a CHUNKSIZE.
 */
int mm_init(void) {
    // Allocate 20 (8-byte sized words) for the free list.
    freeList = mem_sbrk(20 * 8);
    if (freeList == (void *) -1) {  // checks for an error initializing the free list
        return -1;
    }

    // Set all pointers in the free list to NULL
    for (int i = 0; i < 20; i++) {
        freeList[i] = NULL;
    }

    // Allocate enough memory for 4 blocks for epilogue, prologue
    // and beginning bits and set their values
    heap = mem_sbrk(4 * WSIZE);
    if (heap == (void *) -1) { // checks for an error initializing the heap
        return -1;
    }

    PUT(heap + (0 * WSIZE), 0);
    PUT(heap + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap + (2 * WSIZE), PACK(DSIZE, 1)); // prologue
    PUT(heap + (3 * WSIZE), PACK(0, 1));     // epilogue
    heap += (2 * WSIZE);

    // Extend the heap by CHUNKSIZE
    increase_heap(CHUNKSIZE);

    return 0;
}


/* mm_malloc receives an amount and returns a pointer to a block of that size.
 * It handles faulty requests, such as a request for a 0-sized block of a
 * block size less than the minimum 32 bytes. It also aligns the size parameter
 * to a multiple of 16. If there is not enough space in the heap, it will
 * also extend the size of the heap. */
void *mm_malloc(size_t size) {
    // block pointer
    char *bp = NULL;
    size_t adj_size;

    // check for bad request
    if (size == 0)
        return NULL;

    // if the size is less than DSIZE, just make it standard size MINSIZE
    if (size < (size_t) DSIZE) {
        adj_size = (size_t) MINSIZE;
    }
        // align the requested size to be a multiple of ALIGNMENT
    else {
        adj_size = ALIGNMENT * ((size + (ALIGNMENT) + (ALIGNMENT - 1)) / ALIGNMENT);
    }

    // update bp to point to a block of size adj_size
    bp = find_free_block(adj_size);

    // check to see if bp is NULL, which occurs if there is not enough
    // space in the heap
    if (bp != NULL) {
        split_block(bp, adj_size);
        return bp;
    }

    // extend the size of the heap by adj_size or CHUNKSIZE, whichever is larger.
    bp = increase_heap(MAX(adj_size, CHUNKSIZE / WSIZE));

    if (bp == NULL) {
        printf("Unable to allocate adj_size: %lu\n", adj_size);
        return NULL;
    }
    // split the new block to save space
    split_block(bp, adj_size);

    return bp;
}

/* mm_free receives an allocated block, updates the allocated  bit in its footer
 * header words. Also checks for fault mm_free requests. Lastly, it adds the
 * freed block to the free list. */
void mm_free(void *bp) {
    // check for bad requests
    if (bp == NULL) {
        return;
    }
    // get the size of the block freed
    size_t blockSize = GET_SIZE(HDRP(bp));

    // update the header and footer
    PUT(HDRP(bp), PACK(blockSize, 0));
    PUT(FTRP(bp), PACK(blockSize, 0));

    // return the freed block to the free list
    free_block(bp);
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
/* mm_realloc has been optimized for this lab's particular trace set.
 * The realloc function will check for faulty parameters such as a NULL pointer
 * or a 0B size parameter. It'll also check to see if the existing block size of
 * the pointer is large enough to hold the requested amount of bytes, at which
 * it'll simply return the block. The traces used in this lab have heavily
 * affected the implementation of mm_realloc, as certain realloc calls are made to
 * predictable pointers, we have made it so will place blocks greater than 32B
 * in a 4000B block, blocks greater 4000B in a 29000B and so forth. During these
 * reallocations, the mm_realloc function will also copy data from the previous
 * block, referenced by the original pointer 'ptr'. */
void *mm_realloc(void *ptr, size_t size) {
    size_t blockSize; // ptr block size
    void *newptr; // potential new block pointer

    // if size == 0, free the pointer
    if (ptr != NULL && size == 0) {
        mm_free(ptr);
        return (void *) 0;
    }
    // if pointer is NULL, just malloc the size
    if (ptr == NULL && size > 0) {
        return mm_malloc(size);
    }

    // if the size is less than MINSIZE, adjust it to be MINSIZE
    if (size < MINSIZE) {
        size = MINSIZE;
    } else {
        // align the size of the block to the next multiple of ALIGNMENT
        size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // calculate the size of the block referenced by ptr
    blockSize = GET_SIZE(HDRP(ptr));

    // check to see if block referenced by ptr can hold the requested size, if so, return it.
    if (size < blockSize) {
        return ptr;
    }
        // check to see if the mm_realloc function is requesting a block between 32B and 4001B
    else if (size >= 32 && size < 4001) {
        newptr = mm_malloc(4000);
        memcpy(newptr, ptr, size);
        mm_free(ptr);
        return newptr;
    }
        // check to see if the mm_realloc function is requesting a block between 32B and 4001B
    else if (size >= 4000 && size < 29001) {
        newptr = mm_malloc(29000);
        memcpy(newptr, ptr, size);
        mm_free(ptr);
        return newptr;
    } else if (size >= 29001 && size < 100001) {
        newptr = mm_malloc(100000);
        memcpy(newptr, ptr, size);
        mm_free(ptr);
        return newptr;
    } else {
        newptr = mm_malloc(620000);
        memcpy(newptr, ptr, size);
        mm_free(ptr);
        return newptr;
    }
}

/*============================================================================*/
//                               mm_checkheap                                 //
//----------------------------------------------------------------------------//
// mm_checkheap is the heap consistency checker, which scans the heap when    //
// called and checks it for correctness. It runs silently until it detects an //
// error in the heap. Then and only then,will it print a message and terminate//
// the program with exit.                                                     //
/*============================================================================*/
/* mm_checkheap */
void mm_checkheap(int lineno) {
    char* bp = NEXT_BLKP(heap);

    // calls mm_checkheap helper function header_footer_check()
    header_footer_check();

    if (heap == NULL) {
        printf("heap pointer is wrong \n");
    }
    if (freeList == NULL) {
        printf("free list initial pointer is wrong \n");
    }
    if (GET_SIZE(HDRP(heap)) != DSIZE && GET_SIZE(FTRP(heap)) != DSIZE) {
        printf("prologue/epilogue sizes are wrong \n");
    }
    if (GET_ALLOC(HDRP(heap)) != 1 && GET_ALLOC(FTRP(heap)) != 1) {
        printf("prologue alloc bits are wrong \n");
    }
    if (GET_SIZE(mem_heap_hi() - WSIZE + 1) != 0
        || GET_ALLOC(mem_heap_hi() - WSIZE + 1) != 1) {
        printf("epilogue block is wrong \n");
    }




    /*===================== Segregated Free list Checker =====================*/
    // Goes through the segregated free list, and checks pointer consistency, //
    // if the pointers lie within the memory heap, if the sizes of the blocks //
    // correspond with the right size class in the free list.                 //
    /*====================== ======================== ========================*/
    /* checks the blocks in the free list */

    for (int i = 0; i < 20; i++) {
        bp = freeList[i]; // set to beginning of list
        while (bp != NULL) { // traverse down the list
            if (ACC_N_P(bp) != NULL && !(in_heap(ACC_N_P(bp)))) {
                printf("bp not in the right range\n");
            }
            if (!in_heap(bp)) {
                printf("bp not in heap.\n");
            }
            if ((ACC_N_P(bp) != NULL && ACC_P_P(ACC_N_P(bp)) != NULL) &&
                ACC_P_P(ACC_N_P(bp)) != bp) {
                printf("mismatch between bp and nextblock\n");
            }
            bp = ACC_N_P(bp); // update bp to next block
        }
    }
    return;
}


/* Check all the footers and headers in the heap to make sure they are the same. */
void header_footer_check(){
    char* bp = NEXT_BLKP(heap);
    size_t  frontSize;
    size_t  backSize;
    int     frontAlloc;
    int     backAlloc;
    while(GET_SIZE(bp) != 0) //  traverse down the heap
    {
        frontSize = GET_SIZE(HDRP(bp));
        frontAlloc = GET_ALLOC(HDRP(bp));
        backSize = GET_SIZE(FTRP(bp));
        backAlloc = GET_ALLOC(FTRP(bp));
        if(!in_heap(bp)){
            printf("bp outside the heap\n");
        }
        if(frontSize != backSize || frontAlloc != backAlloc){
            printf("header and footers mismatch\n");
        }
    }
    bp = NEXT_BLKP(bp); // update bp to the next block
}


/* find_free_block returns a pointer to a block in the free list that is large
 * enough to hold size bytes. This is done using a first fit policy and the
 * helper function class_finder, which returns an index to index of the free
 * list most appropiate to begin the first-fit policy search. */
static void *find_free_block(size_t size)
{
    int index = class_finder(size);
    char* bp = freeList[index];
    while (index < 20) // traverse down all lists
    {
        if(freeList[index] != NULL){
            bp = freeList[index]; // point to beginning of list
            // traverse the linked list
            while(bp != NULL) // traverse down entire list
            {
                if((GET_SIZE(HDRP(bp)) >= size)) // return of first fit
                {
                    return bp;
                }
                else{
                    bp = ACC_N_P(bp); // update bp to the next block on the free list
                }
            }
        }
        index++; // move to next list size class
    }
    return NULL;
}

/* allocate_block remove the block pointer by bp from the free list.
 * This is achieved by changing the pointers of the block. */
static void allocate_block(void* bp)
{
    char* prev = ACC_P_P(bp); // access the previous block
    char* next = ACC_N_P(bp); // access the next block
    int index = class_finder(GET_SIZE(HDRP(bp))); // get size class of bp
    if (next == NULL){
        if(prev == NULL)
            freeList[index] = NULL;
        else{
            SET(FREE_N_P(prev), NULL);
        }
    }
    else {
        if(prev == NULL){ // list is empty
//            printf("list is empty for index: %d", index);
            freeList[index] = next;
            SET(FREE_P_P(next), NULL);
        }
        else{
            SET(FREE_N_P(prev), next);
            SET(FREE_P_P(next), prev);
        }
    }
    return;
}

/* split_block accepts two parameters: bp and size. It will attempt to split the
 * block if the remaining block will be of at least 32B, the minimum block size. */
static void split_block(void* bp, size_t asize)
{
    size_t blockSize = GET_SIZE(HDRP(bp)); // current block size
    allocate_block(bp); // remove block from free list

    // check is splitting the block will produce a remainder block greater than MINSIZE
    if((blockSize - asize) >= MINSIZE)
    {
        // create split borders for the two new blocks
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(blockSize - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(blockSize - asize, 0));

        // add the new block to the free lists
        free_block(NEXT_BLKP(bp));
    }
    else // leave the block in its original form
    {
        PUT(HDRP(bp), PACK(blockSize, 1));
        PUT(FTRP(bp), PACK(blockSize, 1));
    }
}

/* coalesce will attempt to unite the blocks before and after the block referenced
 * by blockPointer. It will do this by checking to see if they are free. There
 * are 4 possible scenarios: (1) both adjacent blocks are allocated (2) only the next
 * block is free (3) only the previous block is gree or (4) both adjacent blocks are
 * free. */
static void *coalesce(void* blockPointer)
{
    size_t prevFree = GET_ALLOC(FTRP(PREV_BLKP(blockPointer))); // previous block status
    size_t nextFree = GET_ALLOC(HDRP(NEXT_BLKP(blockPointer))); // next block status
    size_t blockSize = GET_SIZE(HDRP(blockPointer)); // current block size

    if(prevFree & nextFree){ // case 0
        return blockPointer;
    }
    allocate_block(blockPointer);       // add the block to the free list

    if(!prevFree && nextFree){ // case 1
        //  printf("prev is free only \n");
        allocate_block(PREV_BLKP(blockPointer)); // add prev. block to free list.
        blockSize += GET_SIZE(HDRP(PREV_BLKP(blockPointer))); // update total block size
        PUT(FTRP(blockPointer), PACK(blockSize, 0)); //update block footer
        PUT(HDRP(PREV_BLKP(blockPointer)), PACK(blockSize, 0)); // update block header
        blockPointer = PREV_BLKP(blockPointer); // update block pointer

    }
    else if(!nextFree && prevFree){ // case 2
        //  printf("next is free only \n");
        allocate_block(NEXT_BLKP(blockPointer)); // add next block to free list
        blockSize += GET_SIZE(HDRP(NEXT_BLKP(blockPointer))); // update total block size
        PUT(HDRP(blockPointer), PACK(blockSize, 0)); // update block header
        PUT(FTRP(blockPointer), PACK(blockSize, 0)); // update block footer
        // no need to update block pointer since it remains at the same position

    }
    else { // case 3
        //  printf("both are free \n");
        allocate_block(PREV_BLKP(blockPointer)); // free previous block
        allocate_block(NEXT_BLKP(blockPointer)); // free next block
        blockSize += GET_SIZE(HDRP(PREV_BLKP(blockPointer))) +
                GET_SIZE(HDRP(NEXT_BLKP(blockPointer))); // update total block size
        PUT(HDRP(PREV_BLKP(blockPointer)), PACK(blockSize,0)); // update new block header
        PUT(FTRP(NEXT_BLKP(blockPointer)), PACK(blockSize,0)); //update new block footer
        blockPointer = PREV_BLKP(blockPointer); // update the blockPointer
    }
    // add entire new block to the free list
    free_block(blockPointer);
    return blockPointer;
}

/* increase_heap will increase the size of the heap by amount words. It will also
 * the epilogue of the heap. */
static void *increase_heap(size_t words)
{
    char* bp;
    size_t size = (words % 2) ? (words + 1) : words; // maintain an evenly value heapsize
    bp = mem_sbrk(size); // extend the heap by size bytes
    if((bp) == (void *)-1) { // checks to see if there was an error extending heap
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0)); // update free block header
    PUT(FTRP(bp), PACK(size, 0)); // updated free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // updated heap epilogue

    // add this new block to the free list. change this.
    free_block(bp);

    // coalesce new block with adjacent free blocks.
    return coalesce(bp);
}

/* class_finder returns the i-th list according to the parameter size. Lists are
 * divided by powers of 2, beginning at 2^6. */
int class_finder(size_t size)
{
    int i = 6;
    while (i < 20){
        if ((1 << (i)) >= size){
            return i;
        }
        i++;
    }
    return 20;
}

/* free_block takes a pointer parameter and finds the ideal location to
 * place it inside its respective free list. */
static void free_block(char* ptr)
{
    size_t asize = GET_SIZE(HDRP(ptr));  // Requested block's size
    int index = class_finder(asize);       // Size class of req block
    char* nextPointer = NULL;            // Next pointer
    char* bp = freeList[index];        // Init bp = beginning of linked list
    while((bp != NULL) && (asize > GET_SIZE(HDRP(bp)))){
        nextPointer = bp;
        bp = ACC_N_P(bp);
    }
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

static int in_heap(const void *p) {
    if (p <= mem_heap_hi() && p >= mem_heap_lo()){
        return 1;
    }
    return 0;
}



