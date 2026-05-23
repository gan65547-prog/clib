#include <unistd.h>
#include <fcntl.h>
#include "test_framework.h"
#include "../include/ml_iniparser.h"
#include "../include/ml_string.h"

static const char *sample_ini =
    "[server]\n"
    "host = 192.168.0.1\n"
    "port = 8080\n"
    "\n"
    "[database]\n"
    "name   = mydb\n"
    "user   = admin\n"
    "pass   = secret123\n"
    "enabled = true\n"
    "\n"
    "; comment line\n"
    "pi = 3.14\n";

static void write_temp(const char *content, char *path, int path_sz)
{
    int fd;
    ssize_t n = (ssize_t)strlen(content);
    ml_snprintf(path, path_sz, "/tmp/clib_test_ini_%d.ini", getpid());
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, content, n); close(fd); }
}

void test_iniparser_suite(void)
{
    TEST_SUITE("INI Parser");

    RUN_TEST("load and parse sections", {
        char path[256];
        write_temp(sample_ini, path, sizeof(path));

        ml_dictionary_t *ini = ml_ini_load(path);
        ASSERT_PTR_NOT_NULL(ini, "load");

        int nsec = ml_ini_get_nsec(ini);
        ASSERT_EQ(nsec, 2, "2 sections");

        ASSERT_STR_EQ(ml_ini_get_secname(ini, 0), "server", "sec0");
        ASSERT_STR_EQ(ml_ini_get_secname(ini, 1), "database", "sec1");

        ml_ini_free(ini);
        unlink(path);
    });

    RUN_TEST("get typed values", {
        char path[256];
        write_temp(sample_ini, path, sizeof(path));

        ml_dictionary_t *ini = ml_ini_load(path);
        ASSERT_PTR_NOT_NULL(ini, "load");

        ASSERT_STR_EQ(ml_ini_get_string(ini, "server:host", NULL), "192.168.0.1", "host");
        ASSERT_EQ(ml_ini_get_int(ini, "server:port", -1), 8080, "port");
        ASSERT_EQ(ml_ini_get_bool(ini, "database:enabled", -1), 1, "enabled=true");

        ml_ini_free(ini);
        unlink(path);
    });

    RUN_TEST("get double from section key", {
        char path[256];
        write_temp(sample_ini, path, sizeof(path));

        ml_dictionary_t *ini = ml_ini_load(path);
        ASSERT_PTR_NOT_NULL(ini, "load");

        double pi = ml_ini_get_double(ini, "database:pi", -1.0);
        ASSERT(pi > 3.13 && pi < 3.15, "pi ~3.14");

        ml_ini_free(ini);
        unlink(path);
    });

    RUN_TEST("default values for missing keys", {
        char path[256];
        write_temp("[s]\na = hello\n", path, sizeof(path));

        ml_dictionary_t *ini = ml_ini_load(path);
        ASSERT_PTR_NOT_NULL(ini, "load");

        ASSERT_STR_EQ(ml_ini_get_string(ini, "s:nope", "FB"), "FB", "string def");
        ASSERT_EQ(ml_ini_get_int(ini, "s:missing", 42), 42, "int def");
        ASSERT_EQ(ml_ini_get_bool(ini, "s:missing", 2), 2, "bool def");

        ml_ini_free(ini);
        unlink(path);
    });

    RUN_TEST("section keys enumeration", {
        char path[256];
        write_temp("[db]\nuser=root\npass=123\n", path, sizeof(path));

        ml_dictionary_t *ini = ml_ini_load(path);
        ASSERT_PTR_NOT_NULL(ini, "load");

        ASSERT_EQ(ml_ini_get_sec_nkeys(ini, "db"), 2, "2 keys in db");

        const char *keys[8];
        const char **k = ml_ini_get_sec_keys(ini, "db", keys);
        ASSERT_PTR_NOT_NULL(k, "keys");

        ml_ini_free(ini);
        unlink(path);
    });

    RUN_TEST("find entry", {
        char path[256];
        write_temp("[net]\nip=1.1.1.1\n", path, sizeof(path));

        ml_dictionary_t *ini = ml_ini_load(path);
        ASSERT_PTR_NOT_NULL(ini, "load");

        ASSERT_EQ(ml_ini_find_entry(ini, "net:ip"), 1, "found");
        ASSERT_EQ(ml_ini_find_entry(ini, "net:nope"), 0, "not found");

        ml_ini_free(ini);
        unlink(path);
    });

    RUN_TEST("set and unset", {
        ml_dictionary_t *d = ml_dictionary_new(0);
        ml_ini_set(d, "s:key", "val");
        ASSERT_STR_EQ(ml_ini_get_string(d, "s:key", NULL), "val", "set");

        ml_ini_unset(d, "s:key");
        ASSERT_STR_EQ(ml_ini_get_string(d, "s:key", "FB"), "FB", "unset");

        ml_dictionary_delete(d);
    });

    RUN_TEST("load non-existent file returns NULL", {
        ml_dictionary_t *ini = ml_ini_load("/tmp/no_such_ini_file_42.ini");
        ASSERT_PTR_NULL(ini, "no file");
    });

    RUN_TEST("null parameter checks", {
        ASSERT_EQ(ml_ini_get_nsec(NULL), -1, "nsec NULL");
        ASSERT_PTR_NULL(ml_ini_get_secname(NULL, 0), "secname NULL");
        ASSERT_PTR_NULL(ml_ini_get_string(NULL, "k", NULL), "getstr NULL");
        ASSERT_EQ(ml_ini_get_int(NULL, "k", 7), 7, "getint NULL");
        ASSERT_EQ(ml_ini_get_bool(NULL, "k", 3), 3, "getbool NULL");
        ASSERT_EQ(ml_ini_find_entry(NULL, "k"), 0, "find NULL");
    });
}
