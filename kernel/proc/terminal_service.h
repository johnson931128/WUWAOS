#ifndef WUWAOS_KERNEL_PROC_TERMINAL_SERVICE_H
#define WUWAOS_KERNEL_PROC_TERMINAL_SERVICE_H

#include "kernel/io/tty.h"

/* Kernel-owned service used by UI adapters to request the existing GUI shell. */
long process_start_gui_terminal(TtyOutputConsumer consumer, void *context, TtyId *id_out);

#endif
