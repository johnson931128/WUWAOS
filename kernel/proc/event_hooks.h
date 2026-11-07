#ifndef WUWAOS_KERNEL_PROC_EVENT_HOOKS_H
#define WUWAOS_KERNEL_PROC_EVENT_HOOKS_H

typedef void (*ProcessPollCallback)(void);
typedef int (*ProcessEventActiveCallback)(void);

/* Boot registers one UI event source; the process loop preserves its poll order. */
void process_register_ui_events(ProcessPollCallback poll, ProcessEventActiveCallback active);

#endif
