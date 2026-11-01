#include "workstate.h"

long user_main(long argument) {
    (void)argument;
    workstate_main();
    return 0;
}
