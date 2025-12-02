#include "mem_alloc.h"
#include <stdio.h>
#include <stddef.h>

/*
 * Template for 11malloc. If you want to implement it in C++, rename
 * this file to mem_alloc.cc.
 */

#ifdef NOVA
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

//IMPLEMENTACIA ALOKATORA
// Definicia struktury pre C (typedef aby som nemusela pisat vsade struct)
typedef struct BlockHeader {
    unsigned long size;          // velkost dat (bez hlavicky)
    unsigned int is_free;        // 1 = volny, 0 = obsadeny
    struct BlockHeader *next;    // nasledujuci blok
    struct BlockHeader *prev;    // predchadzajuci blok
} BlockHeader;

// globalne pointery na zaciatok a koniec mojho zoznamu
static BlockHeader *head = NULL;
static BlockHeader *tail = NULL;

void *my_malloc(unsigned long size)
{
    if (size == 0) return NULL;

    // zarovnam velkost na 4 bajty kvoli efektivite procesora
    unsigned long aligned_size = (size + 3) & ~3;

    // hladam volny blok (First Fit strategia)
    BlockHeader *curr = head;
    while (curr) {
        if (curr->is_free && curr->size >= aligned_size) {
            // nasla som blok. skusim ho rozdelit, ak je prilis velky?
            // musi ostat dost miesta na hlavicku + aspon 4 bajty dat
            if (curr->size >= aligned_size + sizeof(BlockHeader) + 4) {
                // vypocitam adresu noveho bloku pointerovou aritmetikou
                BlockHeader *new_block = (BlockHeader *)((char*)curr + sizeof(BlockHeader) + aligned_size);

                new_block->size = curr->size - aligned_size - sizeof(BlockHeader);
                new_block->is_free = 1;
                new_block->next = curr->next;
                new_block->prev = curr;

                if (curr->next) {
                    curr->next->prev = new_block;
                } else {
                    tail = new_block; // ak delim posledny blok, tail sa posuva
                }

                curr->next = new_block;
                curr->size = aligned_size;
            }

            curr->is_free = 0;
            return (void*)(curr + 1); // vratim pointer za hlavicku
        }
        curr = curr->next;
    }

    // nenasla som volne miesto, musim zvacsit heap
    void *current_brk = nbrk(0);
    unsigned long alloc_size = sizeof(BlockHeader) + aligned_size;

    // poziadam system o pamat
    void *request = nbrk((char*)current_brk + alloc_size);
    if (request == (void*)0 || request == (void*)-1) {
        return NULL; // chyba alokacie, dosla pamat
    }

    // inicializujem novy blok na konci
    BlockHeader *block = (BlockHeader *)current_brk;
    block->size = aligned_size;
    block->is_free = 0;
    block->next = NULL;
    block->prev = tail;

    if (tail) {
        tail->next = block;
    } else {
        head = block; // toto je uplne prvy blok
    }
    tail = block;

    return (void*)(block + 1);
}

int my_free(void *address)
{
    if (!address) return 0;

    // ziskam pointer na hlavicku (posun o velkost struktury dozadu)
    BlockHeader *block = (BlockHeader*)((char*)address - sizeof(BlockHeader));

    // VALIDACIA: musim overit, ci pointer naozaj existuje v mojom zozname.
    // zadanie hovori, ze free na nealokovanu pamat je chyba.
    BlockHeader *curr = head;
    int valid = 0; // 0 = false, 1 = true (lebo sme v C)
    while (curr) {
        if (curr == block) {
            valid = 1;
            break;
        }
        curr = curr->next;
    }

    if (!valid){
     return 1; // chyba: pointer neexistuje v mojom heape
    }

    if (block->is_free){
        return 2; // chyba: double free (uz bol uvolneny)
    }
    block->is_free = 1;

    // SPAJANIE BLOKOV (Coalescing)
    // vdaka 'prev' a 'next' pointerom to viem spravit efektivne skusim spojit s nasledujucim blokom (smerom doprava)
    if (block->next && block->next->is_free) {
        BlockHeader *next_block = block->next;
        block->size += sizeof(BlockHeader) + next_block->size;
        block->next = next_block->next;

        if (block->next) {
            block->next->prev = block;
        } else {
            tail = block; // ak som pohltila posledny blok, tail je teraz tento blok
        }
    }

    // skusim spojit s predchadzajucim blokom (smerom dolava)
    if (block->prev && block->prev->is_free) {
        BlockHeader *prev_block = block->prev;
        prev_block->size += sizeof(BlockHeader) + block->size;
        prev_block->next = block->next;

        if (block->next) {
            block->next->prev = prev_block;
        } else {
            tail = prev_block;
        }
        // po tomto spojeni je 'block' uz neplatny, vsetko je v 'prev_block'
    }

    return 0;
}