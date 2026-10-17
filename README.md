# clib — C Middleware Library

![CI](https://github.com/nonggengiang/clib/actions/workflows/ci.yml/badge.svg)
![version](https://img.shields.io/badge/version-0.1.0-blue)

clib is a portable C library for embedded and edge-device middleware. It provides
memory pools, data structures, concurrency primitives, event-driven I/O, a JSON
parser, and an HTTP parser.

For the daemon/plugin/message-bus framework built on top of clib, see the sibling
project [mlfw](../mlfw/).

**Target**: Linux ARM64 (RK3588, etc.) and macOS development.

## Quick Start

```c
#include "clib.h"

int main(void) {
    ml_pool_t pool;
    ml_pool_create(&pool, 0);

    char *buf = pool.f_alloc(&pool, 1024);
    ml_snprintf(buf, 1024, "hello from clib");

    printf("%s\n", buf);
    ml_pool_destroy(&pool);
    return 0;
}
```

Build and link:

```sh
cd clib
make          # → libclib.a + libclib.so (or .dylib on macOS)
cc -I./include -o myapp myapp.c -L. -lclib -lpthread -lm
```

## Modules

| Module | Header | Description |
|--------|--------|-------------|
| **Pool** | `ml_pool.h` | Hierarchical memory pool allocator |
| **RBTREE** | `ml_rbtree.h` | Red-black tree (generic) |
| **Container** | `ml_container.h` | Set / Map on top of rbtree |
| **Bitmap** | `ml_bitmap.h` | Bit array with O(1) first-free lookup |
| **Ring Buffer** | `ml_ringbuf.h` | Lock-free SPSC ring buffer |
| **Packet Buffer** | `ml_pkbuf.h` | Network packet buffer with chain support |
| **Dictionary** | `ml_dictionary.h` | Hash-table key-value store |
| **Map Key-Value** | `ml_mapkey.h` | String/integer key-value pairs |
| **JSON** | `ml_json.h` | SAX-style JSON parser + builder |
| **INI Parser** | `ml_iniparser.h` | INI config file reader |
| **String** | `ml_string.h` | Safe string operations (snprintf etc.) |
| **Base64** | `ml_base64.h` | Base64 encode/decode |
| **MD5** | `ml_md5.h` | MD5 hash |
| **Thread** | `ml_thread.h` | Mutex, spinlock, rwlock, condvar |
| **Thread Pool** | `ml_thread_pool.h` | Worker thread pool |
| **Event** | `ml_event.h` | epoll event loop (Linux) / select fallback |
| **Timer** | `ml_timer.h` | Timer wheel (ms precision) |
| **Log** | `ml_log.h` | Ring-buffer file logger |
| **Config** | `ml_config.h` | JSON/INI config loading |
| **MMap** | `ml_mmap.h` | Memory-mapped file I/O |
| **Shared Memory** | `ml_shm_pool.h` | Cross-process shared memory pool |
| **Zero Copy** | `ml_zero_copy.h` | Zero-copy buffer chain |
| **List** | `ml_list.h` | Intrusive doubly-linked list |
| **Inet** | `ml_inet.h` | TCP socket helpers |
| **Connection** | `ml_connection.h` | Connection abstraction |

## Tests

```sh
make test
```

26 test suites, 162 test cases. Full CI on Ubuntu + macOS via GitHub Actions.

## Examples

```sh
cd examples && make && ./pool && ./ringbuf && ./thread_pool && ./json
```

| Example | Demonstrates |
|---------|-------------|
| `pool.c` | Memory pool: create, alloc, free/reuse, destroy |
| `ringbuf.c` | Lock-free SPSC ring buffer: write, read |
| `thread_pool.c` | Parallel task submission with worker threads |
| `json.c` | JSON parse, query, build, print |

## Documentation

- [API Reference](docs/API.md) — all modules with minimal examples
- [Changelog](CHANGELOG.md)
- [Examples](examples/)

## License

MIT — see [LICENSE](LICENSE).
