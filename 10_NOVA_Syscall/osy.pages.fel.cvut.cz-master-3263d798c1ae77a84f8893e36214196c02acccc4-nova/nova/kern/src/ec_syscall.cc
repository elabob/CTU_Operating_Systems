#include "ec.h"
#include "ptab.h"
#include "stdio.h"
#include "kalloc.h"
#include "string.h"
#include "memory.h"

// pomocne srandy pre vlakna :

// toto je uzol pre moj kruhovy zoznam vlakien
struct ThreadNode {
    Ec *ec;             // odkaz na kontext vlakna
    ThreadNode *next;   // dalsi v kruhu
    bool used;          // ci je tento slot obsadeny
};

// staticke pole, aby som nemusela dynamicky alokovat uzly
static const int MAX_THREADS = 32;
static ThreadNode thread_pool[MAX_THREADS];
static ThreadNode *current_thread_node = 0;

// funkcia co skontroluje ci mam main vlakno v zozname
static void ensure_main_thread_registered() {
    // ak uz mam nastaveny current node, tak neriesim
    if (current_thread_node != 0) return;

    // prve miesto v poli dam main vlaknu
    current_thread_node = &thread_pool[0];

    // tu si zoberiem aktualne beziace vlakno z triedy Ec
    current_thread_node->ec = Ec::current;

    // zacyklim ho same do seba, lebo je zatial jedine
    current_thread_node->next = current_thread_node;
    current_thread_node->used = true;
}

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
            // toto je moja stara implementacia nbrk z minulej ulohy
            mword addr = r->esi;
            mword old_brk = break_current;
            bool error = false;

            if (addr == 0) {
                r->eax = break_current;
                break;
            }

            if (addr < break_min) {
                r->eax = 0;
                break;
            }

            mword page_mask = ~(PAGE_SIZE - 1);
            mword old_end_page = (break_current + PAGE_SIZE - 1) & page_mask;
            mword new_end_page = (addr + PAGE_SIZE - 1) & page_mask;

            if (new_end_page > old_end_page) {
                for (mword curr = old_end_page; curr < new_end_page; curr += PAGE_SIZE) {
                    if (Ptab::get_mapping(curr) & Ptab::PRESENT)
                        continue;

                    void *page = Kalloc::allocator.alloc_page(1, Kalloc::FILL_0);
                    if (!page) {
                        r->eax = 0;
                        error = true;
                        break;
                    }

                    mword phys = Kalloc::virt2phys(page);
                    if (!Ptab::insert_mapping(curr, phys, Ptab::PRESENT | Ptab::RW | Ptab::USER)) {
                        Kalloc::allocator.free_page(page);
                        r->eax = 0;
                        error = true;
                        break;
                    }
                }
            } else if (new_end_page < old_end_page) {
                for (mword curr = new_end_page; curr < old_end_page; curr += PAGE_SIZE) {
                    mword pte = Ptab::get_mapping(curr);
                    if (pte & Ptab::PRESENT) {
                        mword paddr = pte & page_mask;
                        Kalloc::allocator.free_page(Kalloc::phys2virt(paddr));
                        Ptab::insert_mapping(curr, 0, 0);
                    }
                }
            }

            if (error) break;

            if (addr > break_current) {
                memset(reinterpret_cast<void*>(break_current), 0, addr - break_current);
            }

            break_current = addr;
            r->eax = old_brk;
            break;
        }

        case sys_thr_create: {
            // vytvaram nove vlakno (eax=4)
            ensure_main_thread_registered(); // pre istotu skontrolujem main vlakno

            mword start_routine = r->esi;
            mword stack_top = r->edi;

            // vytvorim novy kontext
            // pouzivam prazdny konstruktor lebo iny nemam
            Ec *new_ec = new Ec();
            if (!new_ec) {
                r->eax = 1; // dosla pamat
                break;
            }

            // nastavim registre
            // pre sysexit musim nastavit edx a ecx, nie eip a esp
            Sys_regs *new_regs = new_ec->sys_regs();
            new_regs->edx = start_routine; // sem ma skocit (instruction pointer)
            new_regs->ecx = stack_top;     // toto je zasobnik (stack pointer)

            // najdem volne miesto v mojom poli
            ThreadNode *free_node = 0;
            for (int i = 0; i < MAX_THREADS; i++) {
                if (!thread_pool[i].used) {
                    free_node = &thread_pool[i];
                    break;
                }
            }

            if (!free_node) {
                r->eax = 2; // nemam uz miesto pre vlakna
                break;
            }

            // pridam ho do zoznamu
            free_node->ec = new_ec;
            free_node->used = true;

            // vlozim ho hned za seba (round robin styl)
            free_node->next = current_thread_node->next;
            current_thread_node->next = free_node;

            r->eax = 0; // vsetko ok
            break;
        }

        case sys_thr_yield: {
            // prepinam vlakno (eax=5)
            ensure_main_thread_registered();

            // posuniem sa na dalsieho v kruhu
            current_thread_node = current_thread_node->next;

            // zmenim globalnu premennu current
            current = current_thread_node->ec;

            // aktivujem tento novy kontext
            current->make_current();

            break;
        }

        default:
            printf ("unknown syscall %d\n", number);
            break;
    };

    ret_user_sysexit();
}