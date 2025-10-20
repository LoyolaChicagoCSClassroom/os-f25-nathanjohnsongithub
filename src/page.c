#include "page.h"
#include <stddef.h>

struct ppage physical_page_array[128];
static struct ppage *free_list = NULL;

void init_pfa_list(void) {
    for (int i = 0; i < 128; i++) {
        physical_page_array[i].physical_addr = (void *)(i * (2 * 1024 * 1024));
        physical_page_array[i].next = (i < 127) ? &physical_page_array[i + 1] : NULL;
        physical_page_array[i].prev = (i > 0)  ? &physical_page_array[i - 1] : NULL;
    }
    free_list = &physical_page_array[0];
}

struct ppage *allocate_physical_pages(unsigned int npages) {
    if (npages == 0 || free_list == NULL) return NULL;
    unsigned int count = 0;
    struct ppage *probe = free_list;
    while (probe && count < npages) {
        probe = probe->next;
        count++;
    }
    if (count < npages) return NULL;
    struct ppage *alloc_head = free_list;
    struct ppage *alloc_tail = alloc_head;
    for (unsigned int i = 1; i < npages; i++) {
        alloc_tail = alloc_tail->next;
    }
    struct ppage *remainder = alloc_tail->next;
    if (remainder) remainder->prev = NULL;
    alloc_tail->next = NULL;
    alloc_head->prev = NULL;
    free_list = remainder;
    return alloc_head;
}

void free_physical_pages(struct ppage *ppage_list) {
    if (!ppage_list) return;
    struct ppage *tail = ppage_list;
    while (tail->next) tail = tail->next;
    tail->next = free_list;
    if (free_list) free_list->prev = tail;
    ppage_list->prev = NULL;
    free_list = ppage_list;
}

