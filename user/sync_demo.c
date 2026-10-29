#include "api.h"
#include "print.h"
#include "sync_demo.h"

static void error(const char *label, long code) {
    user_puts(label); user_number(code); user_puts("\n");
}

static _Noreturn void child_run(long mutex, long gate) {
    user_puts("[child] Trying mutex; parent holds it.\n");
    long result = mutex_lock(mutex);
    if (result < 0) { error("[child] mutex error=", result); exit(result); }
    user_puts("[child] Mutex acquired; writing to shared terminal.\n");
    result = mutex_unlock(mutex);
    if (result < 0) { error("[child] unlock error=", result); exit(result); }
    user_puts("[child] Mutex released. Waiting for semaphore.\n");
    result = semaphore_wait(gate);
    if (result < 0) { error("[child] semaphore error=", result); exit(result); }
    user_puts("[child] Semaphore acquired; continuing and exiting.\n");
    exit(0);
}

void sync_demo(void) {
    long mutex = mutex_create();
    if (mutex < 0) { error("[demo] mutex create error=", mutex); return; }
    long gate = semaphore_create(0);
    if (gate < 0) { error("[demo] semaphore create error=", gate); sync_destroy(mutex); return; }
    long result = mutex_lock(mutex);
    if (result < 0) { error("[demo] mutex lock error=", result); goto cleanup; }
    user_puts("[parent] Mutex locked; writing to shared terminal.\n");
    user_puts("[demo] Semaphore starts at 0; creating child.\n");
    long child = fork();
    if (child == 0) child_run(mutex, gate);
    if (child < 0) {
        error("[demo] fork error=", child);
        mutex_unlock(mutex);
        goto cleanup;
    }
    user_puts("[demo] parent PID="); user_number(getpid());
    user_puts(" child PID="); user_number(child); user_puts("\n");
    /* Give the child time to reach its blocking call, then show real states. */
    sleep_ms(200);
    user_puts("[demo] Snapshot before parent unlocks:\n");
    ps();
    user_puts("[parent] Unlocking mutex.\n");
    result = mutex_unlock(mutex);
    if (result < 0) { error("[demo] unlock error=", result); kill(child); wait(child, 0); goto cleanup; }
    sleep_ms(200);
    user_puts("[demo] Snapshot before semaphore post:\n");
    ps();
    user_puts("[parent] Posting semaphore.\n");
    result = semaphore_post(gate);
    if (result < 0) { error("[demo] post error=", result); kill(child); wait(child, 0); goto cleanup; }
    long status = 0;
    result = wait(child, &status);
    if (result < 0) error("[demo] wait error=", result);
    else if (status != 0) error("[demo] child exit status=", status);
    else user_puts("[demo] Child completed and was collected.\n");
cleanup:
    result = sync_destroy(gate);
    if (result < 0) error("[demo] semaphore destroy error=", result);
    result = sync_destroy(mutex);
    if (result < 0) error("[demo] mutex destroy error=", result);
    user_puts("[demo] Finished (check any errors above).\n");
}
