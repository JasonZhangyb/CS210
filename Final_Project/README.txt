-Yibin Zhang
-U01501319
-No Colaborator
-CS210 Final Assiginment

- Part 1 and Part 2 are all implemented in pa3.c. There is no ifdef guarding of the code because
  Part 2 uses some of the functions implemented in Part 1.
- The Heap Checker source code is implmented in pa3.c
  - To use the heap checker, call heap_checker_report() in main() to print the results.
- For more details about the actual code, please see the comments in pa3.c

Design Decisions:
- The meta-information of the simulated heap is stored at the end of the heap. This will eat up some of
  the heap space.
- To track available space, an array implementation is used. Every node will populate a slot in the array.
  If there is no space available in the array, grow the array by doubling its capacity.
  - A linked list is not used because it creates a very messy situation of having to dynamically
    allocate and de-allocate memory within the heap when I am trying to dynamically allocate and de-allocate
    mmeory.
- To track the size of memory allocated for each Malloc() call, every time we allocate memory, we allocate 
  one extra 8-byte block to store the size of allocation. This is stored right before the return address.
- For part 2. To find a free slot in the indirection table, we iterate each entry one by one until we find an entry
  with content == NULL. Also, instead of growing the table after we exceed its capacity, simply fail the malloc() call. 
  - This is because we can not always grow the indirection table indefinitely while maintaining the same start pointer.
    Say if we use realloc(), and it had to re-allocate the indirection table to a different location.
    Then all pointers that were previously returned by VMalloc() would have a segmentation fault.
- For the heap checker, the instructions said to use rdtsc() for benchmarking. However,after doing some research, I fhound that the given implementation only
  carries over the lower 32-bits of the timer. Even in 64-bit Intel processors, RDTSC still stores the results in EAX, and EDX.
