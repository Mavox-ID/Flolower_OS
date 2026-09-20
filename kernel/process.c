#include <stddef.h>
#include "process.h"

extern void *malloc(size_t size);
extern char *strncpy(char *dst, const char *src, size_t n);
extern void context_switch(uint32_t *old_esp_ptr, uint32_t new_esp);

typedef struct {
    uint32_t     esp;
    ProcessState state;
    char         name[32];
    int          pid;
    void       (*entry)(void);
    uint8_t     *stack_base;
} Process;

static Process processes[MAX_PROCESSES];
static int      current_pid = 0;
static bool     g_initialized = false;

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) processes[i].state = PROC_UNUSED;

    processes[0].state = PROC_RUNNING;
    processes[0].pid = 0;
    processes[0].esp = 0;
    strncpy(processes[0].name, "shell", sizeof(processes[0].name) - 1);
    processes[0].name[sizeof(processes[0].name) - 1] = '\0';

    current_pid = 0;
    g_initialized = true;
}

void process_trampoline(void) {
    Process *p = &processes[current_pid];
    if (p->entry) p->entry();
    process_exit();
}

int process_create(const char *name, void (*entry)(void)) {
    if (!g_initialized) return -1;

    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_UNUSED) continue;

        Process *p = &processes[i];
        p->stack_base = (uint8_t *)malloc(PROCESS_STACK_SIZE);
        if (!p->stack_base) return -1;

        uint32_t *sp = (uint32_t *)(p->stack_base + PROCESS_STACK_SIZE);
        *(--sp) = (uint32_t)process_trampoline;
        *(--sp) = 0x200;
        *(--sp) = 0;
        *(--sp) = 0;
        *(--sp) = 0;
        *(--sp) = 0;

        p->esp = (uint32_t)sp;
        p->entry = entry;
        p->pid = i;
        strncpy(p->name, name, sizeof(p->name) - 1);
        p->name[sizeof(p->name) - 1] = '\0';
        p->state = PROC_READY;
        return i;
    }
    return -1;
}

static void reschedule(bool old_still_runnable) {
    int old_pid = current_pid;
    int next = -1;
    for (int off = 1; off < MAX_PROCESSES; off++) {
        int cand = (old_pid + off) % MAX_PROCESSES;
        if (processes[cand].state == PROC_READY) { next = cand; break; }
    }
    if (next < 0) return;

    if (old_still_runnable) processes[old_pid].state = PROC_READY;
    processes[next].state = PROC_RUNNING;
    current_pid = next;
    context_switch(&processes[old_pid].esp, processes[next].esp);
}

void scheduler_tick(void) {
    if (!g_initialized) return;
    reschedule(true);
}

void process_yield(void) {
    if (!g_initialized) return;
    reschedule(true);
}

void process_exit(void) {
    processes[current_pid].state = PROC_TERMINATED;
    reschedule(false);
    for (;;) { __asm__ volatile("hlt"); }
}

int process_current_pid(void) { return current_pid; }
