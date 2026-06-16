/**
 * Host program integration demo
 *
 * This demo shows how to embed the scripting language runtime in a C host:
 * - Creating and destroying a VM
 * - Exposing host functions to scripts
 * - Exposing host data to scripts
 * - Executing script code
 * - Reading global variables from scripts
 * - Type conversion
 * - Error handling
 * - Limiting script resource usage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "host.h"

static Value add_numbers(int argc, Value* argv) {
    if (argc != 2) {
        return NUMBER_VAL(0);
    }
    double a = 0, b = 0;
    if (IS_NUMBER(argv[0])) a = AS_NUMBER(argv[0]);
    if (IS_NUMBER(argv[1])) b = AS_NUMBER(argv[1]);
    return NUMBER_VAL(a + b);
}

static Value sqrt_native(int argc, Value* argv) {
    if (argc != 1 || !IS_NUMBER(argv[0])) {
        return NUMBER_VAL(0);
    }
    double x = AS_NUMBER(argv[0]);
    if (x < 0) return NIL_VAL;
    return NUMBER_VAL(sqrt(x));
}

static int host_data_counter = 0;

static Value host_get_counter(int argc, Value* argv) {
    MJ_UNUSED(argc);
    MJ_UNUSED(argv);
    return NUMBER_VAL(host_data_counter);
}

static Value host_set_counter(int argc, Value* argv) {
    if (argc >= 1 && IS_NUMBER(argv[0])) {
        host_data_counter = (int)AS_NUMBER(argv[0]);
    }
    return NIL_VAL;
}

int main() {
    printf("=== Moonjit Host Integration Demo ===\n\n");

    MjState* L = mj_open();
    if (!L) {
        fprintf(stderr, "Failed to create VM\n");
        return 1;
    }

    printf("[1] Setting resource limits...\n");
    mj_set_max_steps(L, 100000);
    mj_set_max_memory(L, 10 * 1024 * 1024);
    printf("    Max execution steps: 100000\n");
    printf("    Max memory: 10MB\n\n");

    printf("[2] Exposing host functions to script...\n");

    mj_push_cfunction(L, add_numbers, 2, "add");
    mj_set_global(L, "add");

    mj_push_cfunction(L, sqrt_native, 1, "sqrt");
    mj_set_global(L, "sqrt");

    mj_push_cfunction(L, host_get_counter, 0, "get_counter");
    mj_set_global(L, "get_counter");

    mj_push_cfunction(L, host_set_counter, 1, "set_counter");
    mj_set_global(L, "set_counter");

    printf("    Registered: add, sqrt, get_counter, set_counter\n\n");

    printf("[3] Exposing host data to script (via globals)...\n");
    mj_push_string(L, "Hello from host!");
    mj_set_global(L, "host_message");

    mj_push_number(L, 42);
    mj_set_global(L, "host_number");

    mj_new_table(L);
        mj_push_string(L, "version");
        mj_push_string(L, "1.0.0");
        mj_set_table(L, -3);
        mj_push_string(L, "debug");
        mj_push_bool(L, true);
        mj_set_table(L, -3);
    mj_set_global(L, "host_config");
    printf("    Set: host_message, host_number, host_config\n\n");

    printf("[4] Executing script code...\n");
    const char* script =
        "print(\"host_message:\", host_message)\n"
        "print(\"host_number:\", host_number)\n"
        "print(\"host_config.version:\", host_config.version)\n"
        "print(\"host_config.debug:\", host_config.debug)\n"
        "\n"
        "print(\"add(2, 3) =\", add(2, 3))\n"
        "print(\"sqrt(16) =\", sqrt(16))\n"
        "\n"
        "print(\"counter initial:\", get_counter())\n"
        "set_counter(100)\n"
        "print(\"counter after set:\", get_counter())\n"
        "\n"
        "let script_result = 12345\n";

    int result = mj_load_string(L, script);
    if (result != RESULT_OK) {
        char* err_msg = NULL;
        int err_line = mj_get_error(L, &err_msg);
        fprintf(stderr, "Script error at line %d: %s\n",
                err_line, err_msg ? err_msg : "unknown error");
        mj_close(L);
        return 1;
    }
    printf("    Script executed successfully\n\n");

    printf("[5] Reading global variables from script...\n");
    mj_get_global(L, "script_result");
    if (mj_is_number(L, -1)) {
        double val = mj_to_number(L, -1);
        printf("    script_result = %g\n", val);
    }
    mj_pop(L);

    mj_get_global(L, "host_message");
    if (mj_is_string(L, -1)) {
        int len;
        const char* s = mj_to_string(L, -1, &len);
        printf("    host_message = '%s' (length: %d)\n", s, len);
    }
    mj_pop(L);
    printf("\n");

    printf("[6] Error handling demo - intentionally causing error...\n");
    const char* bad_script = "let x = 10; let y = x + nil;";
    result = mj_load_string(L, bad_script);
    if (result != RESULT_OK) {
        char* err_msg = NULL;
        int err_line = mj_get_error(L, &err_msg);
        printf("    Caught error (line %d): %s\n",
               err_line, err_msg ? err_msg : "unknown");
    }
    printf("\n");

    printf("[7] Verifying host data modified by script...\n");
    printf("    host_data_counter = %d\n", host_data_counter);
    printf("\n");

    printf("[8] Resource limit test - infinite loop (should be terminated)...\n");
    const char* loop_script =
        "let x = 0;\n"
        "while (true) {\n"
        "    x = x + 1;\n"
        "}\n";
    mj_set_max_steps(L, 1000);
    result = mj_load_string(L, loop_script);
    if (result == RESULT_TIMEOUT) {
        char* err_msg = NULL;
        int err_line = mj_get_error(L, &err_msg);
        printf("    Script terminated due to timeout (line %d): %s\n",
               err_line, err_msg ? err_msg : "timeout");
    } else {
        printf("    Script not properly limited? result=%d\n", result);
    }
    printf("\n");

    mj_close(L);
    printf("=== Demo Complete ===\n");
    return 0;
}
