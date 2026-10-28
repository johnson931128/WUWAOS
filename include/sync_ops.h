#ifndef WUWAOS_SYNC_OPS_H
#define WUWAOS_SYNC_OPS_H
#define SYNC_MUTEX_CREATE 1
#define SYNC_MUTEX_LOCK 2
#define SYNC_MUTEX_UNLOCK 3
#define SYNC_SEM_CREATE 4
#define SYNC_SEM_WAIT 5
#define SYNC_SEM_POST 6
#define SYNC_DESTROY 7
/* Owner died while holding the mutex: state is unusable until destroyed. */
#define SYNC_ERR_OWNER_DIED (-130L)
#endif
