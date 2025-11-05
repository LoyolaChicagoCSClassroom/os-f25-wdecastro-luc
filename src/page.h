// page.h
#include <stdint.h>
#ifndef PAGE_H
#define PAGE_H



struct ppage {
uint32_t frame_number;
struct ppage *next;
struct ppage *prev;
void *physical_addr;
int is_free;
unsigned int refcount;
};


struct ppage *allocate_physical_pages(unsigned int npages);
void free_physical_pages(struct ppage *ppage_list);
extern struct ppage physical_page_array[128];
extern struct ppage* free_list_head;

void init_pfa_list(void);

struct page_directory_entry {
    uint32_t present       : 1;  // Page table is present in memory
    uint32_t rw            : 1;  // Read/write: 1 = read/write, 0 = read-only
    uint32_t user          : 1;  // User/supervisor: 1 = user, 0 = supervisor
    uint32_t pwt           : 1;  // Page-level write-through: 1 = write-through, 0 = write-back
    uint32_t pcd           : 1;  // Page-level cache disable: 1 = no cache, 0 = use cache
    uint32_t accessed      : 1;  // Accessed: 1 = accessed, 0 = not accessed
    uint32_t ignored       : 1;  // Reserved, should always be 0
    uint32_t pagesize      : 1;  // Page size: 1 = large page (4MB), 0 = 4KB page
    uint32_t ignored2      : 2;  // Reserved, should always be 0
    uint32_t frame_number  : 20; // Frame address (physical address of the page table) shifted right by 12 bits
} __attribute__((packed));

struct page_entry {
    uint32_t present    : 1;   // Page present in memory
    uint32_t rw         : 1;   // Read-only if clear, read-write if set
    uint32_t user       : 1;   // Supervisor level only if clear
    uint32_t accessed   : 1;   // Has the page been accessed since last refresh?
    uint32_t dirty      : 1;   // Has the page been written to since last refresh?
    uint32_t pwt        : 1;   // Page-level write-through: 1 if write-through, 0 if write-back
    uint32_t pcd        : 1;   // Page-level cache disable: 1 if no cache, 0 if use cache
    uint32_t unused     : 1;   // Reserved bit, must be 0
    uint32_t global     : 1;   // Global page, not flushed on task switch
    uint32_t frame_number : 20; // Frame address (shifted right 12 bits)
} __attribute__((packed));


extern struct page_directory_entry pd[1024];
extern struct page_entry pt[1024];












#endif // PAGE_H
