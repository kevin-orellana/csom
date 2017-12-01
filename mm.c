/* mm seg description change this */

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
     "ZuiHaoXueSheng",
     /* First member's full name */
     "Kevin Orellana",
     /* First member's github username*/
     " kevin-orellana",
     /* Second member's full name (leave blank if none) */
     "Anthony Schalhoub",
     /* Second member's github username (leave blank if none) */
     "hkjs"
};

/* Describe the functions you'll use in the implementation
 * Categorize them as "original, seg-list implementation helpers and debugging" change this */

/* Original Functions */
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
void mm_checkheap(int verbose);

/* function helpers change this */
static void *coalesce(void* bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void place(void* bp, size_t asize);
static void addBlock(char* bp);
static void removeBlock(void* bp);

/* debugging functions */
static int in_heap(const void *p);
static int aligned(const void *p);
//void mm_checkheap(int lineno); delete

/* END DESCRIPTION OF FUNCTIONS change this */

/* BEGIN DESCRIPTION OF MACROS */
# define dbg_printf(...) printf(__VA_ARGS__)

#define WSIZE       (8)  /* Word and header/footer size (bytes) */
#define DSIZE       (16)  /* Double word size (bytes) */
#define MINSIZE     (32)  /* Minimum size of block (bytes) */
#define CHUNKSIZE   (1<<12)  /* Extend heap by this amount (bytes) */

/* gets the value that's larger from x and y */
#define MAX(x, y)   ((x) > (y)? (x) : (y))

/* packs together a size and an allocate bit into val ot put in header */
#define PACK(size, bit)   ((size) | (bit))

/* Read and write a word at address p */
#define GET(p)              (*(unsigned long *)(p))
#define PUT(p, val)         (*(unsigned long *)(p) = (val))

/* Set the memory in address location P1 to be the address of P2 */
#define SET(P1, P2)          (*(unsigned long *)(P1) = (unsigned long)(P2))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* given block pointer bp, compute the address of the header/footer */
#define HDRP(bp)            ((char *) (bp) - WSIZE)
#define FTRP(bp)            ((char *) (bp) + (GET_SIZE(HDRP(bp)) - DSIZE))

/*Given block ptr bp, compute address of previous and next block */
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/*Given block ptr bp, compute address of the location of the next/prev ptr in
  list CHANGE/DESCRIBE THIS */
#define NEXT_LSTP(bp)       ((char*) (bp) + WSIZE)
#define PREV_LSTP(bp)       ((char*) (bp))

/*Given address to next location, derefernece it CHANGE/DESCRIBE THIS*/
#define DREF_NP(bp)          (*(char**) NEXT_LSTP(bp))
#define DREF_PP(bp)          (*(char**) PREV_LSTP(bp))

/* DEBUG MODE DELETE THIS EVENTUALLY*/
#define DEBUGMODE            0  // 0 is off, 1 is on

/* GLOBAL VARIABLES DESCRIBE THIS */
char* heapStart;
char** tableStart;


/*============================================================================*/
//                                  mm_init                                   //
//----------------------------------------------------------------------------//
// Performs any necessary initializations, such as allocating the initial     //
// heap area. The return value should be -1 if there was a problem in         //
// performing the initialization, 0 otherwise. Every time the driver executes //
// a new trace, it resets your heap to the empty heap by calling your mm init //
// function.                                                                  //
/*============================================================================*/
int mm_init(void)
{
    // Allocate enough memory in beginning for 20 blocks of
    // space for linked list
    if((long)(tableStart = mem_sbrk(20*WSIZE)) == -1)
        return -1;

    // Initialize all entries for the linked list to be NULL
    for(int i = 0; i < 20; i++){
        tableStart[i] = NULL;
    }

    // Allocate enough memory for 4 blocks for epilogue, prologue
    // and beginning bits.
    if((long)(heapStart = mem_sbrk(4*WSIZE)) == -1)
        return -1;

    // Place start block, prologue blocks, and epilogue block.
    PUT(heapStart + (0*WSIZE), 0);
    PUT(heapStart + (1*WSIZE), PACK(DSIZE,1));
    PUT(heapStart + (2*WSIZE), PACK(DSIZE,1));
    PUT(heapStart + (3*WSIZE), PACK(0,1));

    // Change the start of the heap to be where the prologue blocks are
    heapStart += (2*WSIZE);

    // Extend the heap by a standard size to start off
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
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
static void *coalesce(void* bp)
{
    // Get the alloc bits of the previous block to bp and the next block
    // to bp. Then get the size of bp
    size_t prevAlloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // If both previous and next blocks are allocated, do not change the block
    if(prevAlloc && nextAlloc){
        return bp;
    }

    // Removes the block since a new one will be added either way
    removeBlock(bp);

    // Cases upon whether or not the next or previous blocks are allocated
    if(!prevAlloc && nextAlloc){
        // previous block isnt allocated while next is
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else if(prevAlloc && !nextAlloc){
        // previous block is allocated while next isnt.
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    }
    else {
        // both blocks are allocated
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }

    // Add the new coalesced block onto the linked list
    addBlock(bp);

    // DEBUGMODE is on if user types -D on terminal command. Debugmode checks
    // the heap, the stack, and the linked list for correctness.
    if(DEBUGMODE){
        mm_checkheap(1);
    }

    return bp;
}

/*============================================================================*/
//                              extend_heap                                   //
//----------------------------------------------------------------------------//
// Takes a size and converts it into words to extend the heap by in the call  //
// of mem_sbrk. The function then adds the header and footer for the new free //
// block, and then moves the prologue block to the end. This block is then    //
// added to the free list and coalesced with its neighboring blocks.          //
/*============================================================================*/
static void *extend_heap(size_t words)
{
    char* bp;
    size_t size;

    // adjusts size to be even and maintain alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // place header and footer blocks on the new extended free block.
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    // place the epilogue block at the end of the next block.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    // add this new block to the free list.
    addBlock(bp);

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
    //blocks is the size divided by the word size
    int blocks = size/WSIZE;

    // size of blocks are split into powers of 2 up to 2^21, which anything
    // over that number of blocks goes into the last free list (19).
    for(int i = 0; i < 19; i++){
        if(blocks < (1<<(i+3)))
            return i;
    }
    return 19;
}

/*============================================================================*/
//                                 find_fit                                   //
//----------------------------------------------------------------------------//
// find_fit finds a pointer from the segregated free list that suits the size //
// given. This is the first fit policy, that chooses the first block that     //
// has a size larger than the size requested.                                 //
/*============================================================================*/
static void *find_fit(size_t size)
{
    int index;
    char* bp;

    index = size_class(size);

    while (index < 20)
    {
        // if the table's entry isn't NULL, there exists a linked list of the
        // size class. Traverse it and find if there's a block that fits
        if(tableStart[index] != NULL){
            // update bp to be the beginning of the linked list
            bp = tableStart[index];

            // traverse the linked list
            while(bp != NULL)
            {
                if((GET_SIZE(HDRP(bp)) >= size))
                {
                    return bp;
                }
                else{
                    // pointer is the next in the linked list
                    bp = DREF_NP(bp);
                }
            }
        }
        // Go to the next size class
        index++;
    }
    // find_fit doesn't find a fit and throws a fit.
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
static void addBlock(char* ptr)
{
    size_t asize = GET_SIZE(HDRP(ptr));  // Requested block's size
    int index = size_class(asize);       // Size class of req block
    char* np = NULL;                     // Next pointer
    char* bp = tableStart[index];        // Init bp = beginning of linked list

    // Find the location where the block's size fits into the linked list
    // that follows an increasing order.
    while((bp != NULL) && (asize > GET_SIZE(HDRP(bp)))){
        np = bp;
        bp = DREF_NP(bp);
    }

    // Determines where the block (end, beginning, middle) to manipulate the
    // pointers to change it to add the block.
    if(bp == NULL){
        if(np == NULL){
            SET(NEXT_LSTP(ptr), NULL);
            SET(PREV_LSTP(ptr), NULL);
            tableStart[index] = ptr;
        }
        else{
            SET(NEXT_LSTP(ptr), NULL);
            SET(PREV_LSTP(ptr), np);
            SET(NEXT_LSTP(np), ptr);
        }
    }
    else{
        if(np == NULL){
            SET(NEXT_LSTP(ptr), bp);
            SET(PREV_LSTP(bp), ptr);
            SET(PREV_LSTP(ptr), NULL);
            tableStart[index] = ptr;
        }
        else{
            SET(NEXT_LSTP(ptr), bp);
            SET(PREV_LSTP(bp), ptr);
            SET(PREV_LSTP(ptr), np);
            SET(NEXT_LSTP(np), ptr);
        }
    }

    // check if the number of free blocks are the same.
    if(DEBUGMODE){
        mm_checkheap(2);
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
static void removeBlock(void* bp)
{
    char* next = DREF_NP(bp);
    char* prev = DREF_PP(bp);

    // Determines the block's location in the linked list (beginning, middle,
    // end). Uses that to manipulate pointers.
    if (next == NULL){
        // block is at the end
        if(prev == NULL){
            // Block is only block in list
            int index = size_class(GET_SIZE(HDRP(bp)));
            tableStart[index] = NULL;
        }
        else{
            // block is at end
            SET(NEXT_LSTP(prev), NULL);
        }
    }
    else{
        // block is in the front/middle
        if(prev == NULL){
            // block is in the beginning
            int index = size_class(GET_SIZE(HDRP(bp)));
            tableStart[index] = next;
            SET(PREV_LSTP(next), NULL);
        }
        else{
            // block is in the middle
            SET(NEXT_LSTP(prev), next);
            SET(PREV_LSTP(next), prev);
        }
    }
    return;
}

/*============================================================================*/
//                               remove_block                                 //
//----------------------------------------------------------------------------//
// place places the requested block at the beginning of the free block.
//        splits only if the size of the remainder would equal or exceed the
//        minimum block size.
/*============================================================================*/
static void place(void* bp, size_t asize)
{
    size_t bsize = GET_SIZE(HDRP(bp));
    // int index = size_class(asize);

    // Remove block from linked list
    removeBlock(bp);

    // If the size is too small or it will be wasteful to compute a split,
    // just place the memory into the whole free block.
    if((bsize - asize) >= MINSIZE && ((bsize-asize) > bsize/16))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // create split block borders
        PUT(HDRP(NEXT_BLKP(bp)), PACK(bsize-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(bsize-asize, 0));

        // add second split block into free list
        addBlock(NEXT_BLKP(bp));
    }
    else
    {
        // don't split the block.
        PUT(HDRP(bp), PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
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
    char* bp = NULL;
    size_t asize;
    size_t extension;

    // ignore extraneous requests.
    if (size == 0)
        return NULL;

    // if the size is less than DSIZE, just make it standard size MINSIZE
    if(size <= DSIZE){
        asize = MINSIZE;
    }
        // if size is large enough, add DSIZE and then round it to the nearest
        // DSIZE multiple
    else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // search for the size in the seg free list. If there is one, place the
    // block into the free list.
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }
    // else extend the size by asize or chunksize, whichever is larger
    // to avoid having to calculate extra information.
    extension = MAX(asize, CHUNKSIZE);

    // extend heap since there arent enough free blocks in the free list.
    // extend by the extension.
    if((bp = extend_heap(extension/WSIZE)) == NULL){
        return NULL;
    }
    // Next, place the block into the newly allocated memory.
    place(bp, asize);

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
    if(!bp) return;
    // get the size of the block freed
    size_t size = GET_SIZE(HDRP(bp));

    // put header and footer blocks in front of the free block to delimit
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    //add the free block to the list
    addBlock(bp);
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
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    size_t nextsize;
    void *newptr;

    // if size == 0, it means just free the pointer
    if(size == 0){
        mm_free(ptr);
        return 0;
    }
    // if pointer is NULL, just malloc the size
    if (ptr == NULL){
        return mm_malloc(size);
    }

    // if the size is less than DSIZE, just make it standard size MINSIZE
    if(size <= DSIZE){
        size = MINSIZE;
    }
        // if size is large enough, add DSIZE and then round it to the nearest
        // DSIZE multiple
    else {
        size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // get the size of the old block
    oldsize = GET_SIZE(HDRP(ptr));
    //if the current size can fit into the oldblock, put it in.
    if(size < oldsize) {
        PUT(HDRP(ptr), PACK(oldsize, 1));
        PUT(FTRP(ptr), PACK(oldsize, 1));
        return ptr;
    }
        // if adding the next block makes the new size fit
    else if(size - oldsize < (nextsize = GET_SIZE(NEXT_BLKP(ptr)))){
        // if the next block is not allocated, merge them.
        // else just leave the block alone.
        if(!GET_ALLOC(NEXT_BLKP(ptr)) && !GET_SIZE(NEXT_BLKP(ptr))){
            // add the size together
            oldsize += nextsize;
            // remove the old block from free list
            removeBlock(NEXT_BLKP(ptr));
            // put new header/footers on block.
            PUT(HDRP(ptr), PACK(oldsize,1));
            PUT(FTRP(ptr), PACK(oldsize,1));
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
    memcpy(newptr, ptr, oldsize);
    // free the old block to be used later.
    mm_free(ptr);
    return newptr;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
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

    char* bp = NEXT_BLKP(heapStart);
    size_t  frontSize;
    int     frontAlloc;
    size_t  backSize;
    int     backAlloc;
    int     implicitFreeCount = 0;

    // Check if the heap beginning is invalid.
    if (heapStart == NULL){
        dbg_printf("heap start is NULL\n");
        exit(1);
    }

    // Check if the table start is invalid.
    if (tableStart == NULL){
        dbg_printf("table start is NULL\n");
        exit(1);
    }

    // Check if the epilogue size bits are intact.
    if(GET_SIZE(HDRP(heapStart)) != DSIZE &&
       GET_SIZE(FTRP(heapStart)) != DSIZE){
        dbg_printf("prologue sizes are wrong\n");
        exit(1);
    }

    //Check if the epilogue alloc bits are intact.
    if(GET_ALLOC(HDRP(heapStart)) != 1 &&
       GET_ALLOC(FTRP(heapStart)) != 1){
        dbg_printf("prologue alloc bits are wrong\n");
        exit(1);
    }

    // Check if the prologue alloc and size bits are intact.
    if(GET_SIZE(mem_heap_hi()-WSIZE+1)!=0
       || GET_ALLOC(mem_heap_hi()-WSIZE+1)!= 1)
    {
        dbg_printf("epilogue block is wrong\n");
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
            dbg_printf("header and footers dont match\n");
            exit(1);
        }
        // Check if the pointer is in the heap.
        if(!in_heap(bp)){
            dbg_printf("this pointer not even in heap\n");
            exit(1);
        }
        // Check if the pointer is aligned.
//        if(!aligned(bp)){
//            dbg_printf("pointer is not aligned.\n");
//            exit(1);
//        }
        // Check for minimum size
        if(frontSize <= DSIZE && frontSize != 0){
            dbg_printf("block less than minimum size\n");
            exit(1);
        }
        // Check if two adjacent blocks are both free after coalescing
        // has been called. This can be proved by induction to be true
        // for the whole linked list after being run.
        if(frontAlloc == 0 && GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 0){
            dbg_printf("coalescing has failed.\n");
            exit(1);
        }
        // Increments the number of free blocks (used in a check later)
        if(GET_ALLOC(bp) == 1) implicitFreeCount++;
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
        rabbit = tableStart[i];
        turtle = rabbit;

        if(rabbit != NULL && turtle != NULL)
        {
            //rabbit jumps 2 in the linked list
            if(DREF_NP(rabbit) != NULL){
                if(DREF_NP(DREF_NP(rabbit))!= NULL){
                    rabbit = DREF_NP(DREF_NP(rabbit));
                }
                else{
                    rabbit = NULL;
                }
            }
            else{
                rabbit = NULL;
            }

            // turtle crawls one in the linked list
            if(DREF_NP(turtle) != NULL){
                turtle = DREF_NP(turtle);
            }else{
                turtle = NULL;
            }

            // Check if the rabbit is on the turtle (means there's a loop)
            if(rabbit == turtle){
                if(rabbit != NULL){
                    dbg_printf("there is a loop in you linked list.\n");
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
        bp = tableStart[i];
        while(bp != NULL)
        {
            // Checks if the previous pointer of the next pointer is itself
            if((DREF_NP(bp) != NULL && DREF_PP(DREF_NP(bp)) != NULL) &&
               DREF_PP(DREF_NP(bp)) != bp)
            {
                dbg_printf("previous pointer of next is not itself.\n");
                exit(1);
            }

            // Checks if bp is within range of the memory heap's maximum and
            // minimum. Should be able to prove with induction that the prev
            // pointers will also be correct.
            if(!in_heap(bp))
            {
                dbg_printf("bp is not in the right range.\n");
                exit(1);
            }
            // Checks if the next pointer of bp is within range of the
            // memory heap's maximum and minimum.
            if(DREF_NP(bp)!= NULL && !(in_heap(DREF_NP(bp))))
            {
                dbg_printf("next pointer is not in the right range");
                exit(1);
            }

            // Checks if all blocks in the list bucket fall in the correct
            // bucket size range.
            if(GET_SIZE(HDRP(bp))/WSIZE >= (1<<(21))){
                // if size is in the largest size range (19).
                if(i != 19)
                {
                    dbg_printf("block is in wrong index (too large)\n");
                    exit(1);
                }
            }
            else{
                // if size is in other size ranges (0-18)
                if(!(GET_SIZE(HDRP(bp))/WSIZE < (size_t) (1<<(i+3))))
                {
                    dbg_printf("block is in wrong index (too small)\n");
                    exit(1);
                }
            }

            // Change to the next pointer in the linked list
            bp = DREF_NP(bp);
            totalFree++;
        }
    }

    /*====================== Total Free Block Checker ========================*/
    // Operates after the addBlock function, and checks if the total number   //
    // of blocks counted with the implicit list is equal to the total number  //
    // from the segregated freelist count.                                    //
    /*====================== ======================== ========================*/

    if(lineno == 2 && totalFree != implicitFreeCount){
        dbg_printf("free block counts are different! (%d != %d)\n", totalFree,
                   implicitFreeCount);
        exit(1);
    }

    // print statement to tell me if my debug mode is on.
    printf("debug is on\n");

    return;
}

