Masstree
========

This is the source release for Masstree, a fast, multi-core key-value
store. This document describes how to run Masstree and interpret its
results.

## Contents

* `MTDIR`: this directory
* `MTDIR/doc`: Masstree algorithm specification

## Building

Masstree is built with CMake (minimum 3.20) and a C++20 compiler (clang
or gcc). It is tested on Debian, Ubuntu, and macOS on both x86_64 and
aarch64.

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

That produces `build/libmasstree.a` plus the test/demo programs
(`mttest`, `mtd`, `mtclient`, `scantest`, `jsontest`, `msgpacktest`,
`test_atomics`, `test_string`, `unit-mt`). The test programs are only
built when `masstree-cpp` is the top-level project; when consumed from a
parent project via `add_subdirectory()` they are off by default and can
be turned on with `-DMASSTREE_BUILD_TESTS=ON`.

### Consuming from another CMake project

```cmake
add_subdirectory(path/to/masstree-cpp)
target_link_libraries(my_target PRIVATE masstree::masstree)
```

The library target propagates:

- include dirs (source root + `include/` for the `<masstree/all.hh>` umbrella
  header + generated `config.h`),
- a `-include config.h` compile flag so every TU that uses masstree sees
  the same `HAVE_*` / `ENABLE_*` settings,
- `Threads::Threads`, plus (when enabled) libnuma / jemalloc / tcmalloc.

### Configurable options

| Option | Default | Meaning |
| :-- | :-- | :-- |
| `MASSTREE_BUILD_TESTS` | `ON` standalone, `OFF` when nested | Build mttest/mtd/mtclient/etc. |
| `MASSTREE_ENABLE_NUMA` | `OFF` | Link against libnuma. |
| `MASSTREE_ENABLE_JEMALLOC` | `OFF` | Link against jemalloc. |
| `MASSTREE_ENABLE_TCMALLOC` | `OFF` | Link against gperftools tcmalloc. |
| `MASSTREE_ENABLE_MEMDEBUG` | `OFF` | Enable memory-debugging hooks. |
| `MASSTREE_ENABLE_SUPERPAGE` | `OFF` | Enable hugepage allocation. |
| `MASSTREE_NODEVERSION_IMPL` | `atomic` | `handrolled` / `atomicallfences` / `atomic`. |
| `MASSTREE_STRINGBAG_IMPL` | `atomic` | `original` / `atomic` / `atomicref`. |
| `MASSTREE_RELAX_FENCE` | `pause` | `pause` / `schedyield` / `none`. |
| `MASSTREE_ATOMIC_FENCE_DEFAULT` | `thread` | `thread` (safe on all architectures) / `signal` (matches the old autoconf default; only correct on strongly-ordered archs like x86). |
| `MASSTREE_ROW_TYPE` | `bag` | Default row type used by mttest/mtd/mtclient. |
| `MASSTREE_MAXKEYLEN` | `255` | Maximum key length in bytes. |
| `MASSTREE_CACHE_LINE_SIZE` | `64` | Assumed cache-line size. |

Assertion family (`ENABLE_ASSERTIONS`, `ENABLE_PRECONDITIONS`,
`ENABLE_INVARIANTS`) follows `CMAKE_BUILD_TYPE` — on in Debug /
RelWithDebInfo, off in Release / MinSizeRel.

The legacy autoconf flow (`./bootstrap.sh && bash configure-all.sh`) is
preserved in tree for reference but is no longer the recommended entry
point.

## Testing

The simplest way to try out Masstree is the `./mttest` program.
This test doesn’t involve disk or network overhead.

```
$ ./mttest
1/1 rw1/m
0: now getting
1: now getting
0: {"table":"mb","test":"rw1","trial":0,"thread":0,"puts":13243551,"puts_per_sec":1324492.05531,"gets":13243551,"gets_per_sec":1497267.13928,"ops":26487102,"ops_per_sec":1405590.1258}
1: {"table":"mb","test":"rw1","trial":0,"thread":1,"puts":13242601,"puts_per_sec":1324397.45602,"gets":13242601,"gets_per_sec":1481151.35726,"ops":26485202,"ops_per_sec":1398395.26601}
EXPERIMENT x0
```

The test starts a process which hosts a Masstree, and generates and
executes queries over the tree. It uses all available cores (two in
the above example). The test lasts for 20 seconds. It populates the
key-value store with `put` queries during first 10 seconds, and then
issues `get` queries over the tree during the next 10 seconds. See
`kvtest_rw1_seed` in `kvtest.hh` for more details about the workload.
For a list of workloads, run `./mttest --help`.

The output summarizes the throughput of each core. The `1/1 rw1/m` line says
that `mttest` is running the first trial (out of one trials), of the `rw1`
workload using Masstree (`m` for short) as the internal data structure.
When the run completes (the `now getting` lines are printed during the
test), `mttest` generates a per-core throughput summary, as indicated by
`0: {"table":"mb","test":"rw1",...}`.

If you redirect its standard output to a file or pipe, `mttest` will produce
gnuplot source that plots the median per-core throughput. Each candlestick
has five points for the min,20%,50%,70%,max of the corresponding metrics
among all threads.

`mttest` also writes the output as JSON into file for further analysis. For
example, after `./mttest`, `notebook-mttest.json` will contain:

```
{
  "experiments":{
    "x0":{
      "git-revision":"673994c43d58d46f4ebf3f7d4e1fce19074594cb",
      "time":"Wed Oct 24 14:54:39 2012",
      "machine":"mat",
      "cores":2,
      "runs":["x0\/rw1\/mb\/0"]
    }
  },
  "data":{
    "x0\/rw1\/mb\/0":[
      {
        "table":"mb",
        "test":"rw1",
        "trial":0,
        "thread":0,
        "puts":13243551,
        "puts_per_sec":1324492.05531,
        "gets":13243551,
        "gets_per_sec":1497267.13928,
        "ops":26487102,
        "ops_per_sec":1405590.1258
      },
      {
        "table":"mb",
        "test":"rw1",
        "trial":0,
        "thread":1,
        "puts":13242601,
        "puts_per_sec":1324397.45602,
        "gets":13242601,
        "gets_per_sec":1481151.35726,
        "ops":26485202,
        "ops_per_sec":1398395.26601
      }
    ]
  }
}
```

Run `build/fullatomic-debug/mttest --help` for a list of tests and options.

## Network testing

`mtclient` supports almost the same set of workloads that `mttest` does, but it
sends queries to a Masstree server over the network.

To start the Masstree server, run:

```
$ build/fullatomic-debug/mtd --logdir=[LOG_DIRS] --ckdir=[CHECKPOINT_DIRS]
mb, Bag, pin-threads disabled, logging enabled
no ./kvd-ckp-gen
no ./kvd-ckp-0-0
no ./kvd-ckp-0-1
2 udp threads
2 tcp threads
```

`LOG_DIRS` is a comma-separated list of directories storing Masstree
logs, and `CHECKPOINT_DIRS` is a comma-separated list of directories
storing Masstree checkpoints. Masstree will write its logs to the
`LOG_DIRS` and periodic checkpoints to the `CHECKPOINT_DIRS`. (Both
logging and multithreading are performed using multiple cores, so
there are several log and checkpoint files.) Alternatively, run `./mtd
-n` to turn off logging.

To run the `rw1` workload with `mtclient` on the same machine as
`mtd`, run:

```
$ build/fullatomic-debug/mtclient -s 127.0.0.1 rw1
tcp, w 500, test rw1, children 2
0 now getting
1 now getting
0 total 7632001 763284 put/s 1263548 get/s
1 total 7612501 761423 put/s 1259847 get/s
{"puts":7632001,"puts_per_sec":763284.211682,"gets":7632001,"gets_per_sec":1263548.30195,"ops":15264002,"ops_per_sec":951678.506329}
{"puts":7612501,"puts_per_sec":761423.014367,"gets":7612501,"gets_per_sec":1259847.22076,"ops":15225002,"ops_per_sec":949182.006246}
total 30489004
puts: n 2, total 15244502, average 7622251, min 7612501, max 7632001, stddev 13789
gets: n 2, total 15244502, average 7622251, min 7612501, max 7632001, stddev 13789
puts/s: n 2, total 1524707, average 762354, min 761423, max 763284, stddev 1316
gets/s: n 2, total 2523396, average 1261698, min 1259847, max 1263548, stddev 2617
```
