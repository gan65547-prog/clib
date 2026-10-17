/*
 * json.c — parse, query, build, and print JSON
 *
 * Build: cd .. && make && cd examples && make json && ./json
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clib.h"

int main(void)
{
    /* ---- Parse ---- */
    const char *input =
        "{"
        "  \"name\": \"RK3588\","
        "  \"npu_tops\": 6.0,"
        "  \"cores\": [\"A76\", \"A76\", \"A76\", \"A76\", \"A55\", \"A55\", \"A55\", \"A55\"],"
        "  \"features\": {"
        "    \"pcie\": true,"
        "    \"hdmi_in\": true"
        "  }"
        "}";

    ml_json_t *root = ml_json_parse(input);
    if (!root) { fprintf(stderr, "parse failed\n"); return 1; }

    printf("[parse]\n");
    printf("  name:    %s\n", ml_json_get_string(root, "name", "unknown"));
    printf("  npu:     %.1f TOPS\n", ml_json_get_double(root, "npu_tops", 0));
    /* Nested keys require manual traversal (no dot-path support) */
    ml_json_t *features = ml_json_get_object_item(root, "features");
    if (features) {
        printf("  pcie:    %s\n",
               ml_json_get_bool(features, "pcie", 0) ? "yes" : "no");
        printf("  hdmi_in: %s\n",
               ml_json_get_bool(features, "hdmi_in", 0) ? "yes" : "no");
    }

    /* Iterate array */
    ml_json_t *cores = ml_json_get_object_item(root, "cores");
    if (cores && cores->type == ML_JSON_ARRAY) {
        int n = ml_json_get_array_size(cores);
        printf("  cores:   %d (", n);
        for (int i = 0; i < n; i++) {
            ml_json_t *item = ml_json_get_array_item(cores, i);
            printf("%s%s", item ? item->valuestring : "?", i < n - 1 ? ", " : "");
        }
        printf(")\n");
    }

    ml_json_delete(root);

    /* ---- Build ---- */
    ml_json_t *doc = ml_json_create_object();
    ml_json_add_string_to_object(doc, "status", "ok");
    ml_json_add_number_to_object(doc, "count", 42);

    ml_json_t *arr = ml_json_create_array();
    ml_json_add_item_to_array(arr, ml_json_create_string("edge"));
    ml_json_add_item_to_array(arr, ml_json_create_number(3.14));
    ml_json_add_item_to_object(doc, "tags", arr);

    char *out = ml_json_print(doc);
    printf("\n[build]\n%s\n", out);
    ml_json_free_str(out);
    ml_json_delete(doc);

    return 0;
}
