/*Yibin Zhang
 *U01501319
 *No Colaborator
 *CS210 pa3
 *A Basic Memory Management System
 */

#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define ALIGNMENT 0x8
#define MAX_R     65536

#define prdtsc(lo, hi)      __asm__ __volatile__("rdtsc \n\t" : "=A" (*(lo)), "=d" (*(hi)))

struct {
    long long int num_alloc_blocks;
    long long int num_free_blocks;
    long long int num_raw_bytes_alloc;
    long long int num_total_bytes_alloc;
    long long int num_mallocs;
    long long int num_frees;
    long long int num_fails;
    unsigned long long int total_clock_malloc;
    unsigned long long int total_clock_free;
    long long int heap_size;
    int           is_virtual;
} heap_checker;

void heap_checker_report(void)
{
    if (heap_checker.is_virtual == 0) {
        fprintf(stdout, "<<Part 1 for Region M1>>\n"); 
        fprintf(stdout, "Number of allocated blocks : %lld\n", heap_checker.num_alloc_blocks);
        fprintf(stdout, "Number of free blocks : %lld\n", heap_checker.num_free_blocks);
        fprintf(stdout, "Raw total number of bytes allocated : %lld\n", heap_checker.num_raw_bytes_alloc);
        fprintf(stdout, "Padded total number of bytes allocated : %lld\n", heap_checker.num_total_bytes_alloc);
        fprintf(stdout, "Raw total number of bytes free : %lld\n",  heap_checker.heap_size - heap_checker.num_raw_bytes_alloc);
        fprintf(stdout, "Aligned total number of bytes free : %lld\n", heap_checker.heap_size - heap_checker.num_total_bytes_alloc);
        fprintf(stdout, "Total number of Malloc requests : %lld\n", heap_checker.num_mallocs);
        fprintf(stdout, "Total number of Free requests : %lld\n", heap_checker.num_frees);
        fprintf(stdout, "Total number of request failures : %lld\n", heap_checker.num_fails);
        fprintf(stdout, "Average clock cycles for a Malloc request : %lld\n", heap_checker.total_clock_malloc / heap_checker.num_mallocs);
        fprintf(stdout, "Average clock cycles for a Free request : %lld\n", heap_checker.total_clock_free / heap_checker.num_frees);
        fprintf(stdout, "Total clock cycles for all requests : %llu\n", heap_checker.total_clock_malloc + heap_checker.total_clock_free);
    } else {
        fprintf(stdout, "<<Part 2 for Region M2>>\n"); 
        fprintf(stdout, "Number of allocated blocks : %lld\n", heap_checker.num_alloc_blocks);
        fprintf(stdout, "Number of free blocks : %lld\n", heap_checker.num_free_blocks);
        fprintf(stdout, "Raw total number of bytes allocated : %lld\n", heap_checker.num_raw_bytes_alloc);
        fprintf(stdout, "Padded total number of bytes allocated : %lld\n", heap_checker.num_total_bytes_alloc);
        fprintf(stdout, "Raw total number of bytes free : %lld\n",  heap_checker.heap_size - heap_checker.num_raw_bytes_alloc);
        fprintf(stdout, "Aligned total number of bytes free : %lld\n", heap_checker.heap_size - heap_checker.num_total_bytes_alloc);
        fprintf(stdout, "Total number of VMalloc requests : %lld\n", heap_checker.num_mallocs);
        fprintf(stdout, "Total number of VFree requests : %lld\n", heap_checker.num_frees);
        fprintf(stdout, "Total number of request failures : %lld\n", heap_checker.num_fails);
        fprintf(stdout, "Average clock cycles for a VMalloc request : %lld\n", heap_checker.total_clock_malloc / heap_checker.num_mallocs);
        fprintf(stdout, "Average clock cycles for a VFree request : %lld\n", heap_checker.total_clock_free / heap_checker.num_frees);
        fprintf(stdout, "Total clock cycles for all requests : %llu\n", heap_checker.total_clock_malloc + heap_checker.total_clock_free);
    }
}

typedef char* addrs_t;
typedef void* any_t;

// Use an array based data structure to track free space
// This is to avoid Malloc() and Free() on state tracking
// nodes, which makes the system rather complex
typedef struct space_node {
    addrs_t            start;
    size_t             length;
} space_node_st;

// A global structure that keeps track of the memory system
typedef struct mem_system_st{
    space_node_st *freelist;
    int unsigned   num_free;  // number of free nodes
    int unsigned   num_alloc;
    int unsigned   free_capacity;
    int unsigned   num_r;
    addrs_t       *refs;
} mem_system_st;

mem_system_st *mem_system = NULL;

// Function Prototypes
addrs_t Malloc(size_t size);

// Find the node that represents a piece of space that is big enough
addrs_t find_free_space(size_t size)
{
    int i;
    for (i=0; i<mem_system->num_free; i++) {
        if (mem_system->freelist[i].length >= size) {
            addrs_t addr = mem_system->freelist[i].start;
            mem_system->freelist[i].start  += size;
            mem_system->freelist[i].length -= size;
            return addr;
        }
    }

    return NULL;
}

// Insert a new free space node at freelist[i]
void insert_free_space_node(addrs_t addr, size_t size, int idx)
{
    int dst_capacity = mem_system->free_capacity;
    if (mem_system->num_free == mem_system->free_capacity) {
        dst_capacity = mem_system->free_capacity + 8;
    }

    // create a temporary place to copy the contents to
    space_node_st *temp = (space_node_st*)malloc(sizeof(space_node_st) * dst_capacity);
    
    // copy the nodes before i
    memcpy(temp, mem_system->freelist, sizeof(space_node_st) * idx);

    // set the inserted node
    temp[idx].start  = addr;
    temp[idx].length = size;
    
    // copy the nodes after i
    memcpy(temp + idx + 1, mem_system->freelist + idx, sizeof(space_node_st) * (mem_system->num_free - idx));

    // increment the number of free blocks
    mem_system->num_free++;

    // copy from temp back to freelist
    if (mem_system->free_capacity == dst_capacity) {
        memcpy(mem_system->freelist, temp, sizeof(space_node_st) * mem_system->num_free);
    } else {
        int i;
        int allocated = 0;
        for (i=mem_system->num_free; i>=0; i--) {
            // Allocate the space for the free space tracker
            if ((temp[i].start + temp[i].length) == (addrs_t)(mem_system->freelist)) {
                if (temp[i].length >= 8 * sizeof(space_node_st)) {
                    temp[i].length -= 8 * sizeof(space_node_st);
                    allocated = 1;
                    heap_checker.heap_size -= 8 * sizeof(space_node_st);
                }
                break;
            }
        }

        if (allocated) {
            memcpy(mem_system->freelist, temp, sizeof(space_node_st) * mem_system->num_free);
            mem_system->free_capacity = dst_capacity;
        } else {
            printf("Not enough space left to track empty spaces.\n");
        }
        
    }

    free(temp);

}

// Traverse the linked list to see if any nodes are adjacent
void merge_free_space(void)
{
    int i = 0;
    space_node_st *temp = (space_node_st*)malloc(sizeof(space_node_st) * mem_system->num_free);
    int free_idx = 0;
    
    while (i < mem_system->num_free) {
        // See if the consecutive blocks can be merged
        int j = i;
        while (j < mem_system->num_free - 1) {
            if (mem_system->freelist[j].start + mem_system->freelist[j].length != mem_system->freelist[j+1].start) {
                break; 
            }
            j++;
        }

        // Write the merged list into temp
        if (j != i) {
            temp[free_idx].start  = mem_system->freelist[i].start;
            temp[free_idx].length = mem_system->freelist[j].start - mem_system->freelist[i].start + mem_system->freelist[j].length;
            i = j;
            free_idx++;
        } else {
            // drop the 0 length nodes
            if (mem_system->freelist[i].length > 0) {
                temp[free_idx].start  = mem_system->freelist[i].start;
                temp[free_idx].length = mem_system->freelist[i].length;
                free_idx++;
            }
        }
        i++;
    }

    mem_system->num_free = free_idx;
    // Copy temp back
    memcpy(mem_system->freelist, temp, sizeof(space_node_st) * mem_system->num_free);

    free(temp);
}

// Add back the free space to the linked list again
void add_free_space(addrs_t addr, size_t size)
{
    int i;
    for (i=0; i<mem_system->num_free; i++) {
        // see if the space can be directly added to one of the nodes
        if (mem_system->freelist[i].start == addr + size) {
            mem_system->freelist[i].start   = addr;
            mem_system->freelist[i].length += size;
            break;
        } else if (mem_system->freelist[i].start + mem_system->freelist[i].length == addr) {
            mem_system->freelist[i].length += size;
            break;
        }

        // insert a new node
        if (mem_system->freelist[i].start > addr) {
            insert_free_space_node(addr, size, i);
            break;
        }
    }

    // merge the resulting list
    merge_free_space();
}


// Remove the node that represents the allocated space 
size_t remove_alloc_space(addrs_t addr)
{
    size_t *mem_size = (size_t*)(addr - ALIGNMENT);
    return *mem_size;
}

// Initialize the simulated Memory system
void Init(size_t size)
{
    addrs_t baseptr;
    baseptr = (addrs_t)memalign(ALIGNMENT, size);

    // Reserve the end of the heap for system state tracking
    size -= sizeof(mem_system_st);
    mem_system = (mem_system_st*)(baseptr + size);

    mem_system->free_capacity = 8;

    size -= sizeof(space_node_st) * mem_system->free_capacity;

    // Init malloc system state tracking
    // Allocate a node that represent the entire mem space is available
    mem_system->freelist = (space_node_st*)(baseptr + size); 
    mem_system->num_free          = 1;

    mem_system->freelist[0].start  = baseptr;
    mem_system->freelist[0].length = size;

    // Nothing is allocated yet
    mem_system->num_alloc  = 0;

    // Initialize Heap Checker
    memset(&heap_checker, 0, sizeof(heap_checker));
    heap_checker.is_virtual            = 0;
    heap_checker.num_alloc_blocks      = 0;
    heap_checker.num_free_blocks       = 1;
    heap_checker.num_raw_bytes_alloc   = 0;
    heap_checker.num_total_bytes_alloc = 0;
    heap_checker.num_mallocs           = 0;
    heap_checker.num_frees             = 0;
    heap_checker.num_fails             = 0;
    heap_checker.total_clock_malloc    = 0;
    heap_checker.total_clock_free      = 0;
    heap_checker.heap_size             = size;
}

// Allocate a mem region of size from the simulated system
addrs_t Malloc(size_t size)
{
    size_t raw_size = size;
    unsigned long long start, finish;
    unsigned long long start_hi, finish_hi;
    prdtsc(&start, &start_hi);
    start = start | (start_hi << 32);

    heap_checker.num_mallocs++;

    // since memory needs to be 8-byte aligned, we modify the size a bit so we make sure
    // all allocations are multiple of 8-byte blocks
    size = ((size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;
    size += ALIGNMENT; // reserve an extra 8 byte to track how much memory this block takes

    // Find the node from linked list that contains enough space
    addrs_t addr = find_free_space(size);
    if (addr == NULL) {
        heap_checker.num_fails++;
        prdtsc(&finish, &finish_hi);
        finish = finish | (finish_hi << 32);
        heap_checker.total_clock_malloc += finish - start;
        return NULL;
    }

    // Store information on how much memory is allocated for this address
    size_t *mem_size = (size_t*)addr;
    *mem_size = raw_size; 

    prdtsc(&finish, &finish_hi);
    finish = finish | (finish_hi << 32);
    heap_checker.total_clock_malloc += finish - start;

    // Heap checker
    mem_system->num_alloc++;
    
    heap_checker.num_alloc_blocks      += 1;
    heap_checker.num_raw_bytes_alloc   += raw_size;
    heap_checker.num_total_bytes_alloc += size;
     
    return addr + ALIGNMENT;
}

void Free(addrs_t addr)
{
    unsigned long long start, finish;
    unsigned long start_hi, finish_hi;
    prdtsc(&start, &start_hi);
    start = start | (start_hi << 32);

    size_t raw_size = remove_alloc_space(addr);
    size_t size     = ((raw_size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;
    size           += ALIGNMENT;
    add_free_space(addr - ALIGNMENT, size);

    prdtsc(&finish, &finish_hi);
    finish = finish | (finish_hi << 32);
    heap_checker.total_clock_malloc += finish - start;

    // Heap checker
    mem_system->num_alloc--;

    heap_checker.num_alloc_blocks      -= 1;
    heap_checker.num_free_blocks        = mem_system->num_free;
    heap_checker.num_raw_bytes_alloc   -= raw_size;
    heap_checker.num_total_bytes_alloc -= size;
    heap_checker.num_frees             += 1;
    heap_checker.total_clock_free      += finish - start;

}

addrs_t Put(any_t data, size_t size)
{
    addrs_t dst = Malloc(size);
    if (dst != NULL) {
        memcpy(dst, data, size);
    }

    return dst;
}

void Get(any_t return_data, addrs_t addr, size_t size)
{
    memcpy(return_data, addr, size);

    Free(addr);
}

// Part 2:
void VInit(size_t size)
{
    // Call the normal Init() implementation
    Init(size);
    
    // Initialize the ref table related values
    mem_system->num_r = MAX_R;
    mem_system->refs  = (addrs_t*)malloc(sizeof(addrs_t) * mem_system->num_r);
    memset(mem_system->refs, 0, sizeof(addrs_t) * mem_system->num_r);

    // Heap checker
    heap_checker.is_virtual = 1;
}

addrs_t *VMalloc(size_t size)
{
    addrs_t addr = Malloc(size); 
    if (addr == NULL) {
        return NULL;
    }

    unsigned long long start, finish;
    unsigned long start_hi, finish_hi;
    prdtsc(&start, &start_hi);
    start = start | (start_hi << 32);

    // Find a free slot on the reference table
    int i;
    for (i=0; i<mem_system->num_r; i++) {
        if (mem_system->refs[i] == NULL) {
            mem_system->refs[i] = addr;
            prdtsc(&finish, &finish_hi);
            finish = finish | (finish_hi << 32);
            heap_checker.total_clock_malloc += finish - start;
            return &(mem_system->refs[i]);
        }
    }

    // If we reached here, we exceeded the R table 
    // just return back the memory, and say allocation failed
    add_free_space(addr - ALIGNMENT, size + ALIGNMENT);

    prdtsc(&finish, &finish_hi);
    finish = finish | (finish_hi << 32);
    heap_checker.total_clock_malloc += finish - start;

    heap_checker.num_free_blocks = mem_system->num_free;
    heap_checker.num_fails++;
    heap_checker.num_alloc_blocks -= 1;
    return NULL;
}

// Find the ref container for the specified address
addrs_t *find_ref_alloc(addrs_t addr)
{
    int i;
    for (i=0; i<mem_system->num_r; i++) {
        //printf("mem_system->refs[%d] = %p\n", i, mem_system->refs[i]);
        if (mem_system->refs[i] == addr) {
            return mem_system->refs + i;
        }
    }

    return NULL;
}

void VFree(addrs_t *addr)
{
    // Free the space
    Free(*addr);
    *addr = NULL; // Set ref pointer as NULL to mark it as available

    unsigned long long start, finish;
    unsigned long start_hi, finish_hi;
    prdtsc(&start, &start_hi);
    start = start | (start_hi << 32);

    // We only try to compact if there are more than 1 free memory block
    addrs_t free_start, alloc_start, old_start;
    addrs_t *ref;
    size_t  *size;
    size_t   total_size;
    while (mem_system->num_free > 1) {
        //printf("NUM_FREE = %ld\n", mem_system->num_free);
        //printf("freelist[0] = %p, size = %x\n", mem_system->freelist[0].start, mem_system->freelist[0].length);
        //printf("freelist[1] = %p, size = %x\n", mem_system->freelist[1].start, mem_system->freelist[1].length);
        free_start  = mem_system->freelist[0].start; // point to the earliest start
        alloc_start = free_start + mem_system->freelist[0].length + ALIGNMENT;
        ref = find_ref_alloc(alloc_start); 
        size = (size_t*)(*ref - ALIGNMENT);
        total_size = *size + ALIGNMENT;
        old_start = *ref - ALIGNMENT;
        // If the free space is big enough to fit the entire thing
        if (mem_system->freelist[0].length >= total_size) {
            // move the memory content
            memmove(mem_system->freelist[0].start, *ref - ALIGNMENT, total_size);
            *ref = mem_system->freelist[0].start + ALIGNMENT;
            mem_system->freelist[0].start  += total_size;
            mem_system->freelist[0].length -= total_size;
            add_free_space(old_start, total_size);
        } else {
            // If the free space overlaps with the allocated space
            size_t overlap_size = total_size - mem_system->freelist[0].length;
            memmove(mem_system->freelist[0].start, *ref - ALIGNMENT, total_size);
            *ref = mem_system->freelist[0].start + ALIGNMENT;
            mem_system->freelist[0].length = 0;     
            add_free_space(old_start + overlap_size, total_size - overlap_size);
        }
    }

    prdtsc(&finish, &finish_hi);
    finish = finish | (finish_hi << 32);
    heap_checker.total_clock_free += finish - start;
}

addrs_t *VPut(any_t data, size_t size)
{
    addrs_t *dst = VMalloc(size);
    if (dst != NULL) {
        memcpy(*dst, data, size);
    }

    return dst;
}

void VGet(any_t return_data, addrs_t *addr, size_t size)
{
    memcpy(return_data, *addr, size);

    VFree(addr);
}
