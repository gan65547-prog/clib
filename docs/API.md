# clib API Reference v0.1.0

## 快速开始

```c
#include "clib.h"
// 链接: -lclib -lpthread -lm
```

## 核心模块

### Memory Pool (ml_pool.h)

内存池，减少 malloc/free 碎片和系统调用。小内存从页中分配，大内存从堆分配。

```c
ml_pool_t pool;
ml_pool_create(&pool, 0);              // 0 = 默认页大小 (4KB)
void *p = pool.f_alloc(&pool, 128);    // 从池中分配
pool.f_free(&pool, p);                 // 归还（同大小可复用）
ml_pool_destroy(&pool);                // 一次性释放所有内存
```

见 `examples/pool.c`

---

### Ring Buffer (ml_ringbuf.h)

无锁 SPSC ring buffer，基于 mmap 共享内存，用于跨进程通信。长度必须是 2 的幂。

```c
ml_kring_t ring;
ml_kring_create("/tmp/my_ring", 1048576, &ring);  // 1 MiB
ml_kring_input(&ring, buf, len);                   // 写入
ml_kring_output(&ring, buf, len);                  // 读取
ml_kring_destroy(&ring);
```

见 `examples/ringbuf.c`

---

### Thread Pool (ml_thread_pool.h)

固定大小的线程池，支持任务队列、暂停/恢复、等待完成。

```c
ml_pool_t pool;
ml_pool_create(&pool, 0);

ml_thread_pool_t *tp = ml_thread_pool_create(&pool, 4, 32);  // 4 workers, 32 queue
ml_thread_pool_submit(tp, my_func, my_arg);                    // 提交任务
ml_thread_pool_wait(tp);                                       // 等待全部完成
ml_thread_pool_destroy(tp);
ml_pool_destroy(&pool);
```

见 `examples/thread_pool.c`

---

### JSON (ml_json.h)

轻量 JSON 解析器和构建器，支持对象、数组、字符串、数字、布尔、null。

```c
// 解析
ml_json_t *root = ml_json_parse("{\"key\": 42}");
const char *s = ml_json_get_string(root, "key", "default");
ml_json_delete(root);

// 构建
ml_json_t *doc = ml_json_create_object();
ml_json_add_string_to_object(doc, "status", "ok");
ml_json_add_number_to_object(doc, "count", 42);
char *text = ml_json_print(doc);
ml_json_free_str(text);
ml_json_delete(doc);
```

见 `examples/json.c`

---

### Red-Black Tree (ml_rbtree.h)

通用红黑树，支持插入/查找/删除/最小/最大。用于有序容器、定时器等。

```c
ml_rbtree_t tree;
ml_rbtree_init(&tree);
ml_rbtree_insert(&tree, node);      // O(log n)
ml_rbtree_t *node = ml_rbtree_find(&tree, key);
ml_rbtree_delete(&tree, node);
```

---

### Container (ml_container.h)

内存池分配的红黑树容器，用于池内空闲 block 管理。

---

### HashMap / MapKey (ml_mapkey.h)

基于红黑树的键值映射。

```c
ml_mapkey_t map;
ml_mapkey_init(&map);
ml_mapkey_insert(&map, key, value);
void *v = ml_mapkey_find(&map, key);
```

---

### String (ml_string.h)

安全的字符串操作：split、trim、大小写转换、格式化。

```c
ml_str_t s;
ml_str_init(&s);
ml_str_append(&s, "hello");
ml_str_append_fmt(&s, " %s %d", "world", 42);
ml_str_destroy(&s);
```

---

### Base64 (ml_base64.h)

Base64 编解码。

```c
char *enc = ml_base64_encode(data, len);
ml_uint8_t *dec = ml_base64_decode(enc, &out_len);
```

---

### MD5 (ml_md5.h)

MD5 哈希。

```c
ml_uint8_t digest[16];
ml_md5(data, len, digest);
```

---

### INI Parser (ml_iniparser.h)

INI 配置文件解析。

```c
ml_ini_t *ini = ml_ini_load("/path/to/config.ini", pool);
const char *v = ml_ini_get(ini, "section", "key");
ml_ini_free(ini);
```

---

### Event (ml_event.h)

epoll 事件循环（Linux）/ kqueue 适配（macOS）。

---

### Timer (ml_timer.h)

基于红黑树的定时器。

```c
ml_timer_add(&tree, delay_ms, callback, data);
ml_timer_expire(&tree, now_ms);  // 主循环中调用
```

---

### Log (ml_log.h)

多级别异步日志。

```c
ml_log_t log;
ml_log_create(&log, "/tmp/app.log", ML_LOG_INFO);
ml_log_print(&log, ML_LOG_INFO, "server started on port %d", port);
```

---

### Shared Memory Pool (ml_shm_pool.h)

跨进程共享内存池，基于 POSIX shm + mmap。

**注意**: 依赖 /dev/shm 写入权限。macOS sandbox 和某些容器环境下测试会失败。

---

### Bitmap (ml_bitmap.h)

位图数据结构。

---

### Dictionary (ml_dictionary.h)

键值字典，支持嵌套。

---

### Packet Buffer (ml_pkbuf.h)

网络包缓冲区管理。

---

### Networking (ml_inet.h, ml_connection.h)

POSIX 网络工具：socket、bind、connect、非阻塞 I/O。

---

### OS Helpers (ml_os.h)

跨平台系统信息：pid、pagesize、进程名设置。

---

### Config (ml_config.h)

JSON 配置文件加载。

## 编译与链接

```bash
# 静态库
cc -I/path/to/clib/include myapp.c -L/path/to/clib -lclib -lpthread -lm

# 动态库
cc -I/path/to/clib/include myapp.c -L/path/to/clib -lclib -lpthread -lm \
   -Wl,-rpath,/path/to/clib
```
