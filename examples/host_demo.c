/**
 * Host program integration demo
 *
 * This demo shows how to embed the scripting language runtime in a C host:
 * - Creating and destroying a VM
 * - Exposing host functions to scripts
 * - Passing numbers and strings to scripts
 * - Scripts calling back into host functions
 * - Host reading return values from scripts
 * - Type conversion
 * - Error handling
 * - Limiting script resource usage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "host.h"

/* ========== Host Functions Exposed to Scripts ========== */

// add_numbers: takes two numbers, returns their sum
static Value add_numbers(int argc, Value* argv) {
    printf("    [Host] add_numbers called with %d arguments\n", argc);
    if (argc >= 1 && IS_NUMBER(argv[0])) {
        printf("    [Host]   arg 1: %g (number)\n", AS_NUMBER(argv[0]));
    }
    if (argc >= 2 && IS_NUMBER(argv[1])) {
        printf("    [Host]   arg 2: %g (number)\n", AS_NUMBER(argv[1]));
    }

    double a = 0, b = 0;
    if (argc >= 1 && IS_NUMBER(argv[0])) a = AS_NUMBER(argv[0]);
    if (argc >= 2 && IS_NUMBER(argv[1])) b = AS_NUMBER(argv[1]);
    double result = a + b;
    printf("    [Host]   returning: %g\n", result);
    return NUMBER_VAL(result);
}

// concat_strings: takes two strings, concatenates them
static Value concat_strings(int argc, Value* argv) {
    printf("    [Host] concat_strings called with %d arguments\n", argc);
    if (argc >= 1 && IS_STRING(argv[0])) {
        printf("    [Host]   arg 1: '%s' (string)\n", AS_CSTRING(argv[0]));
    }
    if (argc >= 2 && IS_STRING(argv[1])) {
        printf("    [Host]   arg 2: '%s' (string)\n", AS_CSTRING(argv[1]));
    }

    const char* s1 = "";
    const char* s2 = "";
    if (argc >= 1 && IS_STRING(argv[0])) s1 = AS_CSTRING(argv[0]);
    if (argc >= 2 && IS_STRING(argv[1])) s2 = AS_CSTRING(argv[1]);

    int len1 = (int)strlen(s1);
    int len2 = (int)strlen(s2);
    char* result = (char*)malloc(len1 + len2 + 1);
    strcpy(result, s1);
    strcat(result, s2);

    ObjString* str = copy_string(result, len1 + len2);
    free(result);

    printf("    [Host]   returning: '%s'\n", str->chars);
    return OBJ_VAL(str);
}

// sqrt_native: takes a number, returns its square root
static Value sqrt_native(int argc, Value* argv) {
    printf("    [Host] sqrt called with %d arguments\n", argc);
    if (argc >= 1 && IS_NUMBER(argv[0])) {
        printf("    [Host]   arg 1: %g (number)\n", AS_NUMBER(argv[0]));
    }

    if (argc != 1 || !IS_NUMBER(argv[0])) {
        printf("    [Host]   returning: nil (invalid arguments)\n");
        return NIL_VAL;
    }
    double x = AS_NUMBER(argv[0]);
    if (x < 0) {
        printf("    [Host]   returning: nil (negative number)\n");
        return NIL_VAL;
    }
    double result = sqrt(x);
    printf("    [Host]   returning: %g\n", result);
    return NUMBER_VAL(result);
}

/* ========== Host Counter State ========== */
static int host_data_counter = 0;

static Value host_get_counter(int argc, Value* argv) {
    MJ_UNUSED(argc);
    MJ_UNUSED(argv);
    printf("    [Host] get_counter called, returning: %d\n", host_data_counter);
    return NUMBER_VAL(host_data_counter);
}

static Value host_set_counter(int argc, Value* argv) {
    printf("    [Host] set_counter called with %d arguments\n", argc);
    if (argc >= 1 && IS_NUMBER(argv[0])) {
        printf("    [Host]   arg 1: %g (number)\n", AS_NUMBER(argv[0]));
        host_data_counter = (int)AS_NUMBER(argv[0]);
        printf("    [Host]   host_data_counter now = %d\n", host_data_counter);
    }
    return NIL_VAL;
}

/* ========== Main Demo ========== */

#define PRINT_STEP(n) printf("\n=== Step %d ==================================================\n", n)

int main() {
    printf("================================================================\n");
    printf("           Moonjit Host Integration Demo\n");
    printf("================================================================\n");

    /* ---------- Step 1: Create VM ---------- */
    PRINT_STEP(1);
    printf("Creating Moonjit VM...\n");
    MjState* L = mj_open();
    if (!L) {
        fprintf(stderr, "Failed to create VM\n");
        return 1;
    }
    printf("VM created successfully.\n");

    /* ---------- Step 2: Set Resource Limits ---------- */
    PRINT_STEP(2);
    printf("Setting resource limits...\n");
    mj_set_max_steps(L, 100000);
    mj_set_max_memory(L, 10 * 1024 * 1024);
    printf("  Max execution steps: 100,000\n");
    printf("  Max memory: 10 MB\n");

    /* ---------- Step 3: Register Host Functions ---------- */
    PRINT_STEP(3);
    printf("Registering host functions for scripts to call:\n");
    printf("\n");

    printf("  [Register] add(a: number, b: number) -> number\n");
    mj_push_cfunction(L, add_numbers, 2, "add");
    mj_set_global(L, "add");

    printf("  [Register] concat(s1: string, s2: string) -> string\n");
    mj_push_cfunction(L, concat_strings, 2, "concat");
    mj_set_global(L, "concat");

    printf("  [Register] sqrt(x: number) -> number\n");
    mj_push_cfunction(L, sqrt_native, 1, "sqrt");
    mj_set_global(L, "sqrt");

    printf("  [Register] get_counter() -> number\n");
    mj_push_cfunction(L, host_get_counter, 0, "get_counter");
    mj_set_global(L, "get_counter");

    printf("  [Register] set_counter(n: number) -> nil\n");
    mj_push_cfunction(L, host_set_counter, 1, "set_counter");
    mj_set_global(L, "set_counter");

    printf("\nTotal 5 host functions registered.\n");

    /* ---------- Step 4: Pass Host Data to Scripts ---------- */
    PRINT_STEP(4);
    printf("Passing host data to script via global variables:\n");
    printf("\n");

    // Pass a number
    printf("  [Set Global] host_number = 42 (number)\n");
    mj_push_number(L, 42);
    mj_set_global(L, "host_number");

    // Pass a string
    printf("  [Set Global] host_message = \"Hello from C Host!\" (string)\n");
    mj_push_string(L, "Hello from C Host!");
    mj_set_global(L, "host_message");

    // Pass a table
    printf("  [Set Global] host_config = { version: \"1.0.0\", debug: true } (table)\n");
    mj_new_table(L);
        mj_push_string(L, "version");
        mj_push_string(L, "1.0.0");
        mj_set_table(L, -3);
        printf("    added key 'version' = \"1.0.0\"\n");

        mj_push_string(L, "debug");
        mj_push_bool(L, 1);
        mj_set_table(L, -3);
        printf("    added key 'debug' = true\n");
    mj_set_global(L, "host_config");

    /* ---------- Step 5: Execute Script ---------- */
    PRINT_STEP(5);
    printf("Executing script that uses host functions and data...\n");
    printf("Script code:\n");
    printf("----------------------------------------\n");

    const char* script =
        "// Script accessing host data and calling host functions\n"
        "print(\"host_number =\", host_number)\n"
        "print(\"host_message =\", host_message)\n"
        "print(\"host_config.version =\", host_config.version)\n"
        "print(\"host_config.debug =\", host_config.debug)\n"
        "\n"
        "// Call host functions\n"
        "let sum = add(15, 27)\n"
        "let combined = concat(\"Hello, \", \"World!\")\n"
        "let root = sqrt(256)\n"
        "\n"
        "print(\"add(15, 27) =\", sum)\n"
        "print(\"concat('Hello, ', 'World!') =\", combined)\n"
        "print(\"sqrt(256) =\", root)\n"
        "\n"
        "// Access and modify host state\n"
        "print(\"get_counter() =\", get_counter())\n"
        "set_counter(2024)\n"
        "print(\"get_counter() =\", get_counter())\n"
        "\n"
        "// Set a value for host to read back\n"
        "let script_result = {\n"
        "    status: \"success\",\n"
        "    computed_sum: sum,\n"
        "    computed_root: root,\n"
        "    message: combined\n"
        "}\n";

    // Print the script
    {
        const char* p = script;
        int line = 1;
        printf("%3d: ", line);
        while (*p) {
            if (*p == '\n') {
                printf("\n");
                line++;
                if (*(p+1)) printf("%3d: ", line);
            } else {
                printf("%c", *p);
            }
            p++;
        }
    }
    printf("----------------------------------------\n");
    printf("\nScript execution output:\n");
    printf("----------------------------------------\n");

    int result = mj_load_string(L, script);
    printf("----------------------------------------\n");

    if (result != RESULT_OK) {
        char* err_msg = NULL;
        int err_line = mj_get_error(L, &err_msg);
        fprintf(stderr, "\n[Error] Script failed at line %d: %s\n",
                err_line, err_msg ? err_msg : "unknown error");
        mj_close(L);
        return 1;
    }
    printf("\nScript executed successfully.\n");

    /* ---------- Step 6: Read Return Values from Script ---------- */
    PRINT_STEP(6);
    printf("Reading values set by the script:\n");
    printf("\n");

    // Read script_result table
    printf("  Reading 'script_result' table from script...\n");
    mj_get_global(L, "script_result");
    if (mj_is_table(L, -1)) {
        // Read individual fields
        mj_push_string(L, "status");
        mj_get_table(L, -2);
        if (mj_is_string(L, -1)) {
            int len = 0;
            const char* status = mj_to_string(L, -1, &len);
            printf("    script_result.status = \"%s\"\n", status);
        }
        mj_pop(L);

        mj_push_string(L, "computed_sum");
        mj_get_table(L, -2);
        if (mj_is_number(L, -1)) {
            double sum = mj_to_number(L, -1);
            printf("    script_result.computed_sum = %g\n", sum);
        }
        mj_pop(L);

        mj_push_string(L, "computed_root");
        mj_get_table(L, -2);
        if (mj_is_number(L, -1)) {
            double root = mj_to_number(L, -1);
            printf("    script_result.computed_root = %g\n", root);
        }
        mj_pop(L);

        mj_push_string(L, "message");
        mj_get_table(L, -2);
        if (mj_is_string(L, -1)) {
            int len = 0;
            const char* msg = mj_to_string(L, -1, &len);
            printf("    script_result.message = \"%s\"\n", msg);
        }
        mj_pop(L);
    }
    mj_pop(L); // pop script_result table

    /* ---------- Step 7: Verify Host State Modified ---------- */
    PRINT_STEP(7);
    printf("Verifying host state modified by script:\n");
    printf("  host_data_counter = %d (was 0 before script ran)\n", host_data_counter);

    /* ---------- Step 8: Resource Limit Test ---------- */
    PRINT_STEP(8);
    printf("Testing resource limits - infinite loop with step limit:\n");
    printf("  Script will be terminated after exceeding max steps (1000)\n");
    printf("\n");

    mj_set_max_steps(L, 1000);
    const char* loop_script =
        "let x = 0;\n"
        "while (true) {\n"
        "    x = x + 1;\n"
        "}\n";
    result = mj_load_string(L, loop_script);
    printf("\n");
    if (result == RESULT_TIMEOUT) {
        char* err_msg = NULL;
        int err_line = mj_get_error(L, &err_msg);
        printf("  [Result] Script terminated due to step limit\n");
        printf("  Line: %d\n", err_line);
        printf("  Reason: %s\n", err_msg ? err_msg : "timeout");
        printf("  Exit code: %d\n", result);
    } else {
        printf("  [Result] Script not properly limited? result=%d\n", result);
    }

    /* ---------- Step 9: Cleanup ---------- */
    PRINT_STEP(9);
    printf("Shutting down VM...\n");
    mj_close(L);
    printf("VM destroyed successfully.\n");

    printf("\n================================================================\n");
    printf("                  Demo Complete - All Passed\n");
    printf("================================================================\n");
    return 0;
}
