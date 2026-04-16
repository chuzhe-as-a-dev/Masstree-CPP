#include "query_masstree.hh"

using namespace Masstree;

// globalepoch / active_epoch / global_log_epoch / initial_timestamp now
// live in the masstree library (kvthread.cc).
relaxed_atomic<bool> recovering = false; // so don't add log entries, and free old value immediately

int
main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    threadinfo* ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    default_table::test(*ti);
}
