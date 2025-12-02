#include "ec.h"
#include "ptab.h"
#include "stdio.h"
#include "kalloc.h"
#include "string.h"
#include "memory.h"

typedef enum {
    sys_print      = 1,
    sys_sum        = 2,
    sys_nbrk       = 3,
    sys_thr_create = 4,
    sys_thr_yield  = 5,
} Syscall_numbers;

void Ec::syscall_handler (uint8 a)
{
    Sys_regs * r = current->sys_regs();
    Syscall_numbers number = static_cast<Syscall_numbers> (a);

    switch (number) {
        case sys_print: {
            char *data = reinterpret_cast<char*>(r->esi);
            unsigned len = r->edi;
            for (unsigned i = 0; i < len; i++)
                printf("%c", data[i]);
            break;
        }
        case sys_sum: {
            int first_number = r->esi;
            int second_number = r->edi;
            r->eax = first_number + second_number;
            break;
        }

        case sys_nbrk: {
            mword addr = r->esi;
            mword old_brk = break_current;
            bool error = false;

            // ak mi pride nula, len vratim aktualnu adresu
            if (addr == 0) {
                r->eax = break_current;
                break;
            }

            // nemozem ist pod pociatocny break
            if (addr < break_min) {
                r->eax = 0;
                break;
            }

            mword page_mask = ~(PAGE_SIZE - 1);
            mword old_end_page = (break_current + PAGE_SIZE - 1) & page_mask;
            mword new_end_page = (addr + PAGE_SIZE - 1) & page_mask;

            // zvacsujem heap
            if (new_end_page > old_end_page) {
                for (mword curr = old_end_page; curr < new_end_page; curr += PAGE_SIZE) {

                    if (Ptab::get_mapping(curr) & Ptab::PRESENT)
                        continue;

                    void *page = Kalloc::allocator.alloc_page(1, Kalloc::FILL_0);
                    if (!page) {
                        printf("sys_nbrk: Out of memory!\n");
                        r->eax = 0;
                        error = true;

                        // dosla pamat, musim vratit to co som v tomto cykle uz alokovala (rollback)
                        for (mword cleanup = old_end_page; cleanup < curr; cleanup += PAGE_SIZE) {
                            mword pte = Ptab::get_mapping(cleanup);
                            if (pte & Ptab::PRESENT) {
                                // PREMENOVANE na paddr kvoli tieneniu (shadowing)
                                mword paddr = pte & page_mask;
                                Ptab::insert_mapping(cleanup, 0, 0);
                                Kalloc::allocator.free_page(Kalloc::phys2virt(paddr));
                            }
                        }
                        break;
                    }

                    mword phys = Kalloc::virt2phys(page);
                    if (!Ptab::insert_mapping(curr, phys, Ptab::PRESENT | Ptab::RW | Ptab::USER)) {
                        Kalloc::allocator.free_page(page);
                        printf("sys_nbrk: Mapping failed!\n");
                        r->eax = 0;
                        error = true;

                        // mapovanie zlyhalo, tiez musim spravit rollback
                        for (mword cleanup = old_end_page; cleanup < curr; cleanup += PAGE_SIZE) {
                            mword pte = Ptab::get_mapping(cleanup);
                            if (pte & Ptab::PRESENT) {
                                // PREMENOVANE na paddr kvoli tieneniu (shadowing)
                                mword paddr = pte & page_mask;
                                Ptab::insert_mapping(cleanup, 0, 0);
                                Kalloc::allocator.free_page(Kalloc::phys2virt(paddr));
                            }
                        }
                        break;
                    }
                }
            }
            // zmensujem heap
            else if (new_end_page < old_end_page) {
                for (mword curr = new_end_page; curr < old_end_page; curr += PAGE_SIZE) {
                    mword pte = Ptab::get_mapping(curr);

                    if (pte & Ptab::PRESENT) {
                        // PREMENOVANE na paddr pre konzistenciu
                        mword paddr = pte & page_mask;
                        void *virt_kalloc = Kalloc::phys2virt(paddr);

                        Kalloc::allocator.free_page(virt_kalloc);
                        Ptab::insert_mapping(curr, 0, 0);
                    }
                }
            }

            if (error) {
                break;
            }

            // ak som zvacsovala, musim vynulovat pamat kvoli testom
            if (addr > break_current) {
                memset(reinterpret_cast<void*>(break_current), 0, addr - break_current);
            }

            break_current = addr;
            r->eax = old_brk;
            break;
        }

        default:
            printf ("unknown syscall %d\n", number);
            break;
    };

    ret_user_sysexit();
}