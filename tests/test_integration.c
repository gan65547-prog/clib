/**
 * Integration test: simulates the full mebox business flow.
 *
 * Lifecycle tested: init → config → module load → run cycle → cleanup
 * Covers: master startup, module lifecycle, connection pool, message routing,
 *         client registration pattern, cleanup/shutdown sequence.
 */
#include "test_framework.h"

#include "../include/ml_types.h"
#include "../include/ml_code.h"
#include "../include/ml_rbtree.h"
#include "../include/ml_container.h"
#include "../include/ml_pool.h"

/* ===== Mock types (from sws_module.h / sws_message.h) ===== */
typedef struct {
    char    *m_path;
    void    *m_obj;
    void    *m_log;
    void    *m_pool;
} ml_cycle_config_t;

typedef struct ml_module_s {
    int      m_sn;
    int      m_no;
    char    *m_name;
    int    (*f_config)(ml_cycle_config_t *cycle);
    int    (*f_init)(ml_cycle_config_t *cycle);
    void   (*f_exit)(void);
    int    (*f_handler)(struct ml_module_s *m, void *info, void *buff, int blen, int *plen);
    int    (*f_check)(struct ml_module_s *m, void *buff, int blen);
} ml_module_t;

/* Track module lifecycle */
static int g_config_calls = 0;
static int g_init_calls   = 0;
static int g_handler_calls = 0;
static int g_exit_calls   = 0;

static int mock_config(ml_cycle_config_t *cycle)   { g_config_calls++; return 0; }
static int mock_init(ml_cycle_config_t *cycle)     { g_init_calls++; return 0; }
static void mock_exit(void)                          { g_exit_calls++; }
static int mock_handler(ml_module_t *m, void *info, void *buff, int blen, int *plen) {
    (void)m; (void)info; (void)buff; (void)blen;
    g_handler_calls++; *plen = 0; return 1;
}
static int mock_check(ml_module_t *m, void *buff, int blen) {
    (void)m; (void)buff; (void)blen;
    return 0;
}

/* ===== Connection entry for container-backed map ===== */
typedef struct {
    ml_container_node_t node;
    int id;
    int fd;
} conn_entry_t;

static int conn_compare(ml_container_node_t *a, ml_container_node_t *b) {
    conn_entry_t *ca = ml_container_entry(a, conn_entry_t, node);
    conn_entry_t *cb = ml_container_entry(b, conn_entry_t, node);
    return cb->id - ca->id;
}

/* ===== Registration entry for module registry ===== */
typedef struct {
    ml_container_node_t node;
    unsigned int id;
    int active;
} reg_entry_t;

static int reg_compare(ml_container_node_t *a, ml_container_node_t *b) {
    reg_entry_t *ra = ml_container_entry(a, reg_entry_t, node);
    reg_entry_t *rb = ml_container_entry(b, reg_entry_t, node);
    return (int)(rb->id - ra->id);
}

/* ===== Test: Master startup → module lifecycle → shutdown ===== */
static void test_master_lifecycle(void) {
    ml_pool_t pool;
    int ret = ml_pool_create(&pool, 0);
    ASSERT_EQ(ret, 0, "master pool create should succeed");

    ml_cycle_config_t *cycle = pool.f_alloc(&pool, sizeof(ml_cycle_config_t));
    ASSERT_PTR_NOT_NULL(cycle, "cycle config alloc should succeed");
    ml_memzero(cycle, sizeof(ml_cycle_config_t));
    cycle->m_pool = &pool;

    ml_module_t modules[] = {
        { 0, 0x1001, "timer",   mock_config, mock_init, mock_exit, mock_handler, mock_check },
        { 1, 0x1002, "event",   mock_config, mock_init, mock_exit, mock_handler, mock_check },
        { 2, 0x1003, "message", mock_config, mock_init, mock_exit, mock_handler, mock_check },
        { 3, 0x1004, "project", mock_config, mock_init, mock_exit, mock_handler, mock_check },
        { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL },
    };

    /* Phase 1: module_config() */
    g_config_calls = 0;
    for (int m = 0; modules[m].m_name; m++) {
        if (modules[m].f_config)
            ASSERT_EQ(modules[m].f_config(cycle), 0, "module config should succeed");
    }
    ASSERT_EQ(g_config_calls, 4, "all 4 modules config called");

    /* Phase 2: module_initial() */
    g_init_calls = 0;
    for (int m = 0; modules[m].m_name; m++) {
        if (modules[m].f_init)
            ASSERT_EQ(modules[m].f_init(cycle), 0, "module init should succeed");
    }
    ASSERT_EQ(g_init_calls, 4, "all 4 modules init called");

    /* Phase 3: Simulate message check loop */
    int msg_dest = 0x1003, plen;
    int routed = 0;
    for (int m = 0; modules[m].m_name; m++) {
        if (modules[m].f_check && modules[m].f_check(&modules[m], &msg_dest, 4)) {
            routed = 1;
            modules[m].f_handler(&modules[m], NULL, &msg_dest, 4, &plen);
            break;
        }
    }
    ASSERT(routed == 0, "no module should match unregistered dest");

    /* Phase 4: module_final() — reverse order */
    g_exit_calls = 0;
    for (int m = 3; m >= 0; m--) {
        if (modules[m].f_exit) modules[m].f_exit();
    }
    ASSERT_EQ(g_exit_calls, 4, "all 4 modules exit called");

    ml_pool_destroy(&pool);
}

/* ===== Test: Connection pool with map lookup ===== */
static void test_connection_pool_pattern(void) {
    ml_container_t conn_map;
    ml_map_init(&conn_map);

    conn_entry_t entries[10];
    for (int i = 0; i < 10; i++) {
        entries[i].id = 1000 + i;
        entries[i].fd = i * 10;
        ml_container_node_init(&entries[i].node);
        ASSERT_EQ(conn_map.f_insert(&conn_map, conn_compare, &entries[i].node), 0,
                  "map insert should succeed");
    }

    /* Lookup existing */
    conn_entry_t search;
    search.id = 1005;
    ml_container_node_init(&search.node);
    ml_container_node_t *found = conn_map.f_lookup(&conn_map, conn_compare, &search.node, NULL);
    ASSERT_PTR_NOT_NULL(found, "should find connection by id");
    ASSERT_EQ(ml_container_entry(found, conn_entry_t, node)->id, 1005,
              "found id should be 1005");

    /* Lookup non-existent */
    search.id = 9999;
    ml_container_node_init(&search.node);
    found = conn_map.f_lookup(&conn_map, conn_compare, &search.node, NULL);
    ASSERT_PTR_NULL(found, "non-existent id should return NULL");

    /* Erase and verify count */
    conn_map.f_erase(&conn_map, &entries[3].node);
    int count = 0;
    for (ml_rbtree_node_t *n = rb_first(&conn_map.m_root); n; n = rb_next(n)) count++;
    ASSERT_EQ(count, 9, "9 connections after erase");
}

/* ===== Test: Message header operations ===== */
static void test_message_header_ops(void) {
    typedef struct {
        unsigned int   m_source;
        unsigned int   m_dest;
        unsigned short m_code;
        unsigned short m_resp;
        unsigned short m_type;
        unsigned short m_length;
    } msg_hdr_t;

#define MSGHDR_SET_SOURCE(h, v) ((h)->m_source = (v))
#define MSGHDR_SET_DEST(h, v)   ((h)->m_dest = (v))
#define MSGHDR_SET_RESP(h, v)   ((h)->m_resp = (v))
#define MSGHDR_SET_LENGTH(h, v) ((h)->m_length = (v))
#define MSGHDR_GET_SOURCE(h)    ((h)->m_source)
#define MSGHDR_GET_DEST(h)      ((h)->m_dest)
#define MSGHDR_GET_RESP(h)      ((h)->m_resp)
#define MSGHDR_GET_LENGTH(h)    ((h)->m_length)

    msg_hdr_t hdr;
    ml_memzero(&hdr, sizeof(hdr));

    MSGHDR_SET_SOURCE(&hdr, 1001);
    MSGHDR_SET_DEST(&hdr, 2002);
    MSGHDR_SET_RESP(&hdr, 0x8080);    /* SWS_REGIST_EVENT_ADD */
    MSGHDR_SET_LENGTH(&hdr, 256);

    ASSERT_EQ(MSGHDR_GET_SOURCE(&hdr), 1001u, "source = 1001");
    ASSERT_EQ(MSGHDR_GET_DEST(&hdr), 2002u, "dest = 2002");
    ASSERT_EQ(MSGHDR_GET_RESP(&hdr), 0x8080, "resp = REGIST_EVENT_ADD");
    ASSERT_EQ(MSGHDR_GET_LENGTH(&hdr), 256, "length = 256");
}

/* ===== Test: Error code consistency ===== */
static void test_error_codes(void) {
    ASSERT_EQ(ML_OK, 0, "ML_OK = 0");
    ASSERT(ML_ERROR == -1, "ML_ERROR = -1");
    ASSERT(ML_ENOMEM < ML_ERROR, "ENOMEM after ERROR");
    ASSERT(ML_EPARAM < ML_ERROR, "EPARAM after ERROR");
    ASSERT(ML_ENOREG < ML_ERROR, "ENOREG after ERROR");
    ASSERT(ML_ESOCKT < ML_ERROR, "ESOCKT after ERROR");
}

/* ===== Test: Module client registration pattern (from sws_module_message.c) ===== */
static void test_module_registration(void) {
    ml_container_t registry;
    ml_map_init(&registry);

    reg_entry_t clients[5];
    for (int i = 0; i < 5; i++) {
        clients[i].id = 5000u + (unsigned int)i * 100;
        clients[i].active = 1;
        ml_container_node_init(&clients[i].node);
        registry.f_insert(&registry, reg_compare, &clients[i].node);
    }

    /* Duplicate registration must be rejected */
    reg_entry_t dup;
    dup.id = 5100;
    ml_container_node_init(&dup.node);
    ASSERT_EQ(registry.f_insert(&registry, reg_compare, &dup.node), -1,
              "duplicate registration rejected");

    /* Lookup after erase should fail */
    registry.f_erase(&registry, &clients[2].node);
    reg_entry_t srch;
    srch.id = 5200;
    ml_container_node_init(&srch.node);
    ASSERT_PTR_NULL(registry.f_lookup(&registry, reg_compare, &srch.node, NULL),
                    "erased client not found");

    /* Erased key can now be re-registered */
    ASSERT_EQ(registry.f_insert(&registry, reg_compare, &clients[2].node), 0,
              "re-register after erase should succeed");
}

/* ===== Test: Pre-allocation pool pattern (from sws_connection.c) ===== */
static void test_prealloc_pool_pattern(void) {
    ml_pool_t pool;
    ml_pool_create(&pool, 0);

    typedef struct mock_conn_s {
        int index;
        int fd;
        struct mock_conn_s *next;
    } mock_conn_t;

    int N = 64;
    mock_conn_t *conns = pool.f_alloc(&pool, (size_t)N * sizeof(mock_conn_t));
    ASSERT_PTR_NOT_NULL(conns, "pre-alloc array should succeed");

    /* Build free list (same pattern as sws_connection.c) */
    mock_conn_t *free_list = NULL;
    for (int i = N; i > 0; ) {
        i--;
        ml_memzero(&conns[i], sizeof(mock_conn_t));
        conns[i].index = i;
        conns[i].next = free_list;
        free_list = &conns[i];
    }

    /* Allocate 10 connections from free list */
    mock_conn_t *allocd[10];
    for (int i = 0; i < 10; i++) {
        allocd[i] = free_list;
        ASSERT_PTR_NOT_NULL(allocd[i], "should get connection from pool");
        free_list = allocd[i]->next;
        allocd[i]->next = NULL;
    }

    /* Return 3 connections */
    for (int i = 0; i < 3; i++) {
        allocd[i]->next = free_list;
        free_list = allocd[i];
    }

    /* Should be able to alloc 3 more */
    for (int i = 0; i < 3; i++) {
        mock_conn_t *c = free_list;
        ASSERT_PTR_NOT_NULL(c, "should get returned connection");
        free_list = c->next;
    }

    ml_pool_destroy(&pool);
}

void test_integration_suite(void) {
    TEST_SUITE("Integration - Business Flow");
    RUN_TEST("master lifecycle: init→config→run→cleanup", test_master_lifecycle());
    RUN_TEST("connection pool with map lookup", test_connection_pool_pattern());
    RUN_TEST("message header operations", test_message_header_ops());
    RUN_TEST("error code system", test_error_codes());
    RUN_TEST("module client registration", test_module_registration());
    RUN_TEST("pre-allocation pool pattern", test_prealloc_pool_pattern());
}
