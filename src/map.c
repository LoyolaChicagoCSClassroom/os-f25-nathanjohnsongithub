#include <stddef.h>
#include <stdint.h>
#include "map.h"
#include "page.h"

struct page_directory_entry pd[1024] __attribute__((aligned(4096)));
struct page pt[1024] __attribute__((aligned(4096)));


void *map_pages(void *vaddr, struct ppage *pglist, struct page_directory_entry *pd) {
    // Keep the original base to return
    void *const vaddr_base = vaddr;

    // Convert input address to int so that arithmetic doens't get messed up
    uintptr_t virt = (uintptr_t)vaddr;

    struct ppage *cursor = pglist;
    while (cursor != NULL) {

        // Compute indices from current virtual address
        const uint32_t vpn = (uint32_t)(virt >> 12);
        const uint32_t page_dir_index = (vpn >> 10) & 0x3FF;
        const uint32_t page_table_index = vpn & 0x3FF;

        // Check that page_dir_index is 0 we only have the one page table
        if(page_dir_index == 0) {

            // Use our single page table
            struct page *const page_table_base = pt;

            // Fill one 4KB from the current physical page
            const uintptr_t phys = (uintptr_t)cursor->physical_addr;

            if (!page_table_base[page_table_index].present) {
                page_table_base[page_table_index].frame   = (uint32_t)(phys >> 12);
                page_table_base[page_table_index].present = 1;
                page_table_base[page_table_index].rw      = 1;
                page_table_base[page_table_index].user    = 0;
            }

            // Advance one 4KB page virtually and advance the physical list
            virt   += 0x1000;
            cursor  = cursor->next;
        }
    }

    return vaddr_base;
}

void loadPageDirectory(struct page_directory_entry *pd) {
    asm("mov %0,%%cr3"
        :
        : "r"(pd)
        :);
}

void enablePaging(void) {
    // Enable Paging
    asm("mov %cr0, %eax\n"
        "or $0x80000001,%eax\n"
        "mov %eax,%cr0");
}
