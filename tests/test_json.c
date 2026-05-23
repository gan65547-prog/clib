/*
 * test_json.c — JSON parser + builder tests.
 */
#include "test_framework.h"
#include "../include/ml_types.h"
#include "../include/ml_string.h"
#include "../include/ml_json.h"

void test_json_suite(void)
{
    TEST_SUITE("JSON Parser & Builder");

    /* --- parse --- */

    RUN_TEST("parse empty object", {
        ml_json_t *obj = ml_json_parse("{}");
        ASSERT_PTR_NOT_NULL(obj, "parse {}");
        ASSERT_EQ(obj->type, ML_JSON_OBJECT, "type is object");
        ml_json_delete(obj);
    });

    RUN_TEST("parse simple key-value", {
        ml_json_t *obj = ml_json_parse("{\"a\":1}");
        ASSERT_PTR_NOT_NULL(obj, "parse");
        ASSERT_EQ(ml_json_get_int(obj, "a", -1), 1, "a=1");
        ml_json_delete(obj);
    });

    RUN_TEST("parse string value", {
        ml_json_t *obj = ml_json_parse("{\"name\":\"hello\"}");
        ASSERT_PTR_NOT_NULL(obj, "parse");
        const char *s = ml_json_get_string(obj, "name", NULL);
        ASSERT_PTR_NOT_NULL(s, "get string");
        ASSERT_STR_EQ(s, "hello", "value");
        ml_json_delete(obj);
    });

    RUN_TEST("parse nested object", {
        ml_json_t *obj = ml_json_parse("{\"srv\":{\"port\":8080}}");
        ASSERT_PTR_NOT_NULL(obj, "parse");
        ml_json_t *srv = ml_json_get_object_item(obj, "srv");
        ASSERT_PTR_NOT_NULL(srv, "nested exists");
        ASSERT_EQ(ml_json_get_int(srv, "port", -1), 8080, "port=8080");
        ml_json_delete(obj);
    });

    RUN_TEST("parse array", {
        ml_json_t *obj = ml_json_parse("[1,2,3]");
        ASSERT_PTR_NOT_NULL(obj, "parse array");
        ASSERT_EQ(ml_json_get_array_size(obj), 3, "size=3");
        ml_json_t *item = ml_json_get_array_item(obj, 1);
        ASSERT_PTR_NOT_NULL(item, "item[1]");
        ASSERT_EQ((int)item->valuedouble, 2, "item[1]=2");
        ml_json_delete(obj);
    });

    RUN_TEST("parse bool and null", {
        ml_json_t *obj = ml_json_parse("{\"ok\":true,\"no\":false,\"nil\":null}");
        ASSERT_PTR_NOT_NULL(obj, "parse");
        ASSERT_EQ(ml_json_get_bool(obj, "ok", 0), 1, "ok=true");
        ASSERT_EQ(ml_json_get_bool(obj, "no", 1), 0, "no=false");
        ml_json_delete(obj);
    });

    RUN_TEST("parse null returns NULL gracefully", {
        ml_json_t *obj = ml_json_parse(NULL);
        ASSERT_PTR_NULL(obj, "NULL parse → NULL");
    });

    RUN_TEST("parse invalid returns NULL", {
        ml_json_t *obj = ml_json_parse("not json");
        ASSERT_PTR_NULL(obj, "invalid → NULL");
    });

    RUN_TEST("default values for missing keys", {
        ml_json_t *obj = ml_json_parse("{}");
        ASSERT_PTR_NOT_NULL(obj, "parse");
        ASSERT_EQ(ml_json_get_int(obj, "missing", 42), 42, "default int");
        ASSERT_STR_EQ(ml_json_get_string(obj, "missing", "fallback"), "fallback", "default str");
        ml_json_delete(obj);
    });

    /* --- builder --- */

    RUN_TEST("build object → print → re-parse", {
        ml_json_t *root = ml_json_create_object();
        ASSERT_PTR_NOT_NULL(root, "create object");
        ml_json_add_item_to_object(root, "id", ml_json_create_number(123));
        ml_json_add_item_to_object(root, "name", ml_json_create_string("test"));

        ml_json_t *arr = ml_json_create_array();
        ml_json_add_item_to_array(arr, ml_json_create_number(1));
        ml_json_add_item_to_array(arr, ml_json_create_number(2));
        ml_json_add_item_to_object(root, "items", arr);

        char *printed = ml_json_print_unformatted(root);
        ASSERT_PTR_NOT_NULL(printed, "print");

        ml_json_t *reparsed = ml_json_parse(printed);
        ASSERT_PTR_NOT_NULL(reparsed, "re-parse");
        ASSERT_EQ(ml_json_get_int(reparsed, "id", -1), 123, "id round-trip");

        ml_json_free_str(printed);
        ml_json_delete(reparsed);
        ml_json_delete(root);
    });

    RUN_TEST("array detach and delete", {
        ml_json_t *arr = ml_json_create_array();
        ml_json_add_item_to_array(arr, ml_json_create_number(10));
        ml_json_add_item_to_array(arr, ml_json_create_number(20));
        ml_json_add_item_to_array(arr, ml_json_create_number(30));
        ASSERT_EQ(ml_json_get_array_size(arr), 3, "size 3");

        ml_json_t *detached = ml_json_detach_item_from_array(arr, 1);
        ASSERT_PTR_NOT_NULL(detached, "detach idx 1");
        ASSERT_EQ(ml_json_get_array_size(arr), 2, "size 2 after detach");
        ASSERT_EQ((int)detached->valuedouble, 20, "detached value");
        ml_json_delete(detached);

        ml_json_delete_item_from_array(arr, 0);
        ASSERT_EQ(ml_json_get_array_size(arr), 1, "size 1 after delete");
        ml_json_delete(arr);
    });

    RUN_TEST("object item detach and delete", {
        ml_json_t *obj = ml_json_create_object();
        ml_json_add_item_to_object(obj, "x", ml_json_create_number(5));
        ml_json_add_item_to_object(obj, "y", ml_json_create_number(10));

        ml_json_t *y = ml_json_detach_item_from_object(obj, "y");
        ASSERT_PTR_NOT_NULL(y, "detach y");
        ASSERT_EQ(ml_json_get_int(obj, "y", -1), -1, "y gone");
        ml_json_delete(y);

        ml_json_delete_item_from_object(obj, "x");
        ASSERT_EQ(ml_json_get_int(obj, "x", -1), -1, "x gone");
        ml_json_delete(obj);
    });

    RUN_TEST("convenience macros", {
        ml_json_t *obj = ml_json_create_object();
        ml_json_add_item_to_object(obj, "msg", ml_json_create_string("ok"));
        char *s = ml_json_print_unformatted(obj);
        ASSERT_STR_EQ(s, "{\"msg\":\"ok\"}", "short object");
        ml_json_free_str(s);
        ml_json_delete(obj);
    });

    RUN_TEST("get double", {
        ml_json_t *obj = ml_json_parse("{\"pi\":3.14}");
        ASSERT_PTR_NOT_NULL(obj, "parse");
        ASSERT(ml_json_get_double(obj, "pi", 0.0) > 3.13
               && ml_json_get_double(obj, "pi", 0.0) < 3.15, "pi ~3.14");
        ml_json_delete(obj);
    });
}
