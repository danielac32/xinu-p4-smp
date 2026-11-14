/**
 * @file core1smp.c
 * Entrada SMP de core1, idle e IPI de replanificación.
 */
#include <percpu.h>
#include <thread.h>
#include <queue.h>
#include <interrupt.h>
#include <platform.h>
#include <clock.h>
#include <esp32p4.h>
#include <riscv.h>

#define HP_SYSTEM_CPU_INT_FROM_CPU_0_REG (*(volatile uint32_t *)(HP_SYS_BASE + 0x10))
#define HP_SYSTEM_CPU_INT_FROM_CPU_1_REG (*(volatile uint32_t *)(HP_SYS_BASE + 0x14))
#define ETS_FROM_CPU_INTR0_SOURCE 100
#define ETS_FROM_CPU_INTR1_SOURCE 101
#define IPI_CLIC_LINE0 28
#define IPI_CLIC_LINE1 29

#ifndef IRQ_SOFTWARE
#define IRQ_SOFTWARE IRQ_M_SOFT
#endif

extern void core1_idle(void);
extern int resched(void);
extern volatile uint32 core1_ticks;
extern interrupt clkhandler(void);

/* IPI por HP_SYSTEM CPU_INT_FROM_CPU: despierta/preempta al núcleo destino. */
void send_ipi(int cpu)
{
    if (cpu == 1)
    {
        HP_SYSTEM_CPU_INT_FROM_CPU_1_REG = 1;
        __asm__ volatile("fence rw, rw" ::: "memory");
    }
    else if (cpu == 0)
    {
        HP_SYSTEM_CPU_INT_FROM_CPU_0_REG = 1;
        __asm__ volatile("fence rw, rw" ::: "memory");
    }
}

/* Handler de IPI: limpia la señal de CPU origen y replanifica. */
interrupt ipihandler(void)
{
    if (cpu_id() == 1)
    {
        HP_SYSTEM_CPU_INT_FROM_CPU_1_REG = 0;
    }
    else
    {
        HP_SYSTEM_CPU_INT_FROM_CPU_0_REG = 0;
    }
    __asm__ volatile("fence rw, rw" ::: "memory");
    resched();
}

static inline uint64_t mtime64(void)
{
    uint32_t hi, lo;
    do
    {
        hi = P4_MTIME_HI;
        lo = P4_MTIME_LO;
    } while (hi != P4_MTIME_HI);
    return ((uint64_t)hi << 32) | lo;
}

void core1_idle(void)
{
    extern void clkupdate1(ulong cycles);
    uint64_t next;

    next = mtime64() + (uint64_t)(platform.clkfreq / CLKTICKS_PER_SEC);

    for (;;)
    {
        /* Tick por sondeo: respaldo si la IRQ de timer no llega al hart 1. */
        if ((int64_t)(mtime64() - next) >= 0)
        {
            clkupdate1(platform.clkfreq / CLKTICKS_PER_SEC);
            core1_ticks++;
            if (thrcurrent >= 0 && thrcurrent < NTHREAD)
            {
                thrtab[thrcurrent].cpu_ticks++;
            }
            next += (uint64_t)(platform.clkfreq / CLKTICKS_PER_SEC);
            /* evitar deriva grande si hubo pausa larga */
            if ((int64_t)(mtime64() - next) > (int64_t)(platform.clkfreq / CLKTICKS_PER_SEC))
            {
                next = mtime64() + (uint64_t)(platform.clkfreq / CLKTICKS_PER_SEC);
            }
            resched();
            continue;
        }

        if (percpu[1].need_resched)
        {
            percpu[1].need_resched = 0;
            resched();
            continue;
        }
        if (nonempty(percpu[1].readyq))
        {
            resched();
        }
        else
        {
            /* pausa breve; una IPI puede despertar y forzar replanificación */
            __asm__ volatile("nop; nop; nop; nop");
        }
    }
}

void core1_smp_entry(void)
{
    struct percpu *c = &percpu[1];
    extern void clkupdate1(ulong cycles);

    c->nest = 0;
    c->resdef = 0;
    c->idle = 1;
    c->current = 1;

    thrtab[1].state = THRCURR;
    thrtab[1].intmask = 0;
    thrtab[1].cpu_last = 1;

    /* Tick local en core1 (CLINT mtimecmp hart1 / IRQ_TIMER). */
    interruptVector[IRQ_TIMER] = clkhandler;
    clkupdate1(platform.clkfreq / CLKTICKS_PER_SEC);
    enable_irq(IRQ_TIMER);
    __asm__ volatile("csrs mie, %0" :: "r"((1UL << 7))); /* MTIE */

    /* IPI por HP_SYSTEM FROM_CPU (ruta en matriz CLIC). */
    extern void route_irq(int source, int line);
    extern void enable_irq_line(int line);
    route_irq(ETS_FROM_CPU_INTR1_SOURCE, IPI_CLIC_LINE1);
    interruptVector[IPI_CLIC_LINE1] = ipihandler;
    enable_irq_line(IPI_CLIC_LINE1);

    enable();

    core1_idle();
}
