#include "mem_alloc.h"
#include <stdio.h>
#include <stddef.h>

/*
 * Template for 11malloc. If you want to implement it in C++, rename
 * this file to mem_alloc.cc.
 */

static inline void *nbrk(void *address);

#ifdef NOVA

/**********************************/
/* nbrk() implementation for NOVA */
/**********************************/

static inline unsigned syscall2(unsigned w0, unsigned w1)
{
    asm volatile("   mov %%esp, %%ecx    ;"
                 "   mov $1f, %%edx      ;"
                 "   sysenter            ;"
                 "1:                     ;"
                 : "+a"(w0)
                 : "S"(w1)
                 : "ecx", "edx", "memory");
    return w0;
}

static void *nbrk(void *address)
{
    return (void *)syscall2(3, (unsigned)address);
}
#else

/***********************************/
/* nbrk() implementation for Linux */
/***********************************/

#include <unistd.h>

static void *nbrk(void *address)
{
    void *current_brk = sbrk(0);
    if (address != NULL) {
        int ret = brk(address);
        if (ret == -1)
            return NULL;
    }
    return current_brk;
}

#endif

/* --- MOJA IMPLEMENTACIA --- */

// zadefinovala som si strukturu pre hlavicku bloku
typedef struct header {
    unsigned long size;     // velkost dat
    struct header *next;    // pointer na dalsi blok
    int is_free;            // ci je blok volny
} header_t;

// globalne pointery na zaciatok a koniec mojho zoznamu
static header_t *head = NULL;
static header_t *tail = NULL;

void *my_malloc(unsigned long size)
{
    if (size == 0) {
        // ak odo mna nic nechce, vratim NULL
        return NULL;
    }

    // prechadzam zoznam a hladam volne miesto (first-fit)
    header_t *curr = head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // nasla som volny blok, ktory je dost velky
            curr->is_free = 0;
            // vratim pointer az za hlavicku, priamo na data
            return (void *)(curr + 1);
        }
        curr = curr->next;
    }

    // nenasla som miesto, musim zvacsit heap
    void *block = nbrk(0);

    // vypocitam kolko celkovo potrebujem (hlavicka + data)
    unsigned long total_size = sizeof(header_t) + size;

    // poziadam system o novu pamat
    void *request = nbrk((char *)block + total_size);

    if (request == (void*)0 || request == (void*)-1) {
        // nepodarilo sa alokovat pamat
        return NULL;
    }

    // na novom mieste vytvorim hlavicku
    header_t *header = (header_t *)block;
    header->size = size;
    header->is_free = 0;
    header->next = NULL;

    // pridam novy blok na koniec zoznamu
    if (head == NULL) {
        head = header;
    }
    if (tail) {
        tail->next = header;
    }
    tail = header;

    // vratim pointer na data (preskocim hlavicku)
    return (void *)(header + 1);
}

int my_free(void *address)
{
    if (!address) {
        return -1;
    }

    // ziskam pointer na hlavicku, ktora je hned pred datami
    header_t *header = (header_t *)address - 1;

    // oznacim tento blok ako volny
    header->is_free = 1;

    // teraz prejdem zoznam a pospajam susedne volne bloky (upratujem po sebe)
    header_t *curr = head;
    while (curr) {
        if (curr->is_free && curr->next && curr->next->is_free) {
            // nasla som dva volne bloky vedla seba, spojim ich
            curr->size += sizeof(header_t) + curr->next->size;
            curr->next = curr->next->next;

            // ak som zmazala posledny blok, musim aktualizovat tail
            if (curr->next == NULL) {
                tail = curr;
            }
        } else {
            // posuniem sa dalej
            curr = curr->next;
        }
    }

    return 0;
}