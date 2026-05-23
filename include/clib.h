/*
 * clib — C Middleware Library
 *
 * Single-include header.  Add -I/path/to/clib/include to your
 * CFLAGS and write:
 *
 *   #include "clib.h"
 *
 * Link with -lclib (static) or -lclib -lpthread -lm (dynamic).
 */
#ifndef CLIB_H
#define CLIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Base types & constants ---- */
#include "ml_types.h"
#include "ml_code.h"

/* ---- Memory ---- */
#include "ml_pool.h"
#include "ml_mmap.h"
#include "ml_shm_pool.h"

/* ---- Data structures ---- */
#include "ml_rbtree.h"
#include "ml_container.h"
#include "ml_set_rbtree.h"
#include "ml_bitmap.h"
#include "ml_ringbuf.h"
#include "ml_mapkey.h"
#include "ml_pkbuf.h"
#include "ml_dictionary.h"

/* ---- String / encoding ---- */
#include "ml_string.h"
#include "ml_base64.h"
#include "ml_md5.h"
#include "ml_json.h"
#include "ml_iniparser.h"

/* ---- Concurrency ---- */
#include "ml_thread.h"
#include "ml_thread_pool.h"

/* ---- I/O ---- */
#include "ml_event.h"
#include "ml_timer.h"
#include "ml_log.h"
#include "ml_config.h"

/* ---- Networking (optional, POSIX) ---- */
#include "ml_inet.h"
#include "ml_connection.h"

/* ---- Containers (linked-lists) ---- */
#include "ml_list.h"
#include "ml_llist.h"
#include "ml_dll.h"

/* ---- OS helpers ---- */
#include "ml_os.h"

/* ---- mlfw framework (optional, Linux-only) ---- */
#include "mlfw/mlfw_module.h"
#include "mlfw/mlfw_daemon.h"
#include "mlfw/mlfw_message.h"
#include "mlfw/mlfw_message_bus.h"
#include "mlfw/mlfw_plugin.h"
#include "mlfw/mlfw_process.h"
#include "mlfw/mlfw_http.h"
#include "mlfw/mlfw_http_server.h"
#include "mlfw/mlfw_bridge.h"
#include "mlfw/mlfw_connection_pool.h"
#include "mlfw/mlfw_set.h"
#include "mlfw/mlfw_stream.h"
#include "mlfw/mlfw_list.h"

#ifdef __cplusplus
}
#endif

#endif /* CLIB_H */
