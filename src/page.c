#include "page.h"
#include <stdint.h>

struct ppage physical_page_array[128];
struct ppage* free_list_head = 0;
struct page_directory_entry pd[1024] __attribute__((aligned(4096)));
struct page_entry pt[1024] __attribute__((aligned(4096)));

void init_pfa_list(void) {
    for (int i = 0; i < 128; i++) {
        physical_page_array[i].is_free = 1;
        physical_page_array[i].refcount = 0;

        // Link to the next page, or NULL if last
        if (i < 127) {
            physical_page_array[i].next = &physical_page_array[i + 1];
        } else {
            physical_page_array[i].next = 0;
        }
    }

    // Set head of the free list
    free_list_head = &physical_page_array[0];
}

struct ppage* allocate_physical_pages(unsigned int npages) {
    if (npages == 0 || free_list_head == 0) {
        return 0;  // Invalid request or no pages available
    }

    // Check if there are at least npages available
    struct ppage* temp = free_list_head;
    for (unsigned int i = 0; i < npages; i++) {
        if (temp == 0) {
            return 0;  // Not enough pages available
        }
        temp = temp->next;
    }

    // Allocate npages by removing them from the free list
    struct ppage* alloc_list = free_list_head;
    struct ppage* current = alloc_list;
    struct ppage* prev = 0;

    for (unsigned int i = 0; i < npages; i++) {
        current->is_free = 0;
        current->refcount = 1;

        prev = current;
        current = current->next;
    }

    // Detach allocated pages from free list
    if (prev != 0) {
        prev->next = 0;
    }

    // Update global free list head
    free_list_head = current;

    return alloc_list;
}

void free_physical_pages(struct ppage* ppage_list) {
    if (ppage_list == 0) {
        return;
    }

    struct ppage* current = ppage_list;
    while (current != 0) {
        current->is_free = 1;
        current->refcount = 0;

        struct ppage* next = current->next;

        // Insert this page back to the front of the free list
        current->next = free_list_head;
        free_list_head = current;

        current = next;
    }
}

static inline void load_page_directory(uint32_t *pd_phys_addr)
{
    asm volatile("mov %0, %%cr3" :: "r"(pd_phys_addr) : "memory");
}


