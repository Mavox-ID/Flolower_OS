#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PROCESSES      16
#define PROCESS_STACK_SIZE 8192

typedef enum {
    PROC_UNUSED,
    PROC_READY,
    PROC_RUNNING,
    PROC_TERMINATED
} ProcessState;

void process_init(void);

int process_create(const char *name, void (*entry)(void));

void process_yield(void);

void process_exit(void);

void scheduler_tick(void);

int  process_current_pid(void);

#endif
