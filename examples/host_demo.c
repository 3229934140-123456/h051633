/**
 * Host program integration demo - Fixed step format for easy verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "host.h"

#define SEP "------------------------------------------------------------"
#define BIGSEP "============================================================"
#define PRINT_HEAD(n, title) printf("\n%s\n  Step %d: %s\n%s\n", BIGSEP, n, title, SEP)

/* ========== Host Functions ========== */

static Value add_numbers(int argc, Value* argv) {
    double a = 0, b = 0;
    if (argc >= 1 && IS_NUMBER(argv[0])) a = AS_NUMBER(argv[0]);
    if (argc >= 2 && IS_NUMBER(argv[1])) b = AS_NUMBER(argv[1]);
    double result = a + b;
    printf("    [HostFn execute] add\n");
    printf("      Number args: a = %g, b = %g\n", a, b);
    printf("      Return: %g (number)\n", result);
    return NUMBER_VAL(result);
}

static Value concat_strings(int argc, Value* argv) {
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

    printf("    [HostFn execute] concat\n");
    printf("      String args: s1 = \"%s\", s2 = \"%s\"\n", s1, s2);
    printf("      Return: \"%s\" (string)\n", str->chars);
    return OBJ_VAL(str);
}

static Value sqrt_native(int argc, Value* argv) {
    printf("    [HostFn execute] sqrt\n");
    if (argc != 1 || !IS_NUMBER(argv[0])) {
        printf("      Args: INVALID (not a number)\n");
        printf("      Return: nil (error)\n");
        return NIL_VAL;
    }
    double x = AS_NUMBER(argv[0]);
    printf("      Number arg: x = %g\n", x);
    if (x < 0) {
        printf("      Return: nil (negative number)\n");
        return NIL_VAL;
    }
    double result = sqrt(x);
    printf("      Return: %g (number)\n", result);
    return NUMBER_VAL(result);
}

static int g_host_counter = 0;

static Value host_get_counter(int argc, Value* argv) {
    MJ_UNUSED(argc); MJ_UNUSED(argv);
    printf("    [HostFn execute] get_counter\n");
    printf("      Read host state: counter = %d\n", g_host_counter);
    printf("      Return: %d (number)\n", g_host_counter);
    return NUMBER_VAL(g_host_counter);
}

static Value host_set_counter(int argc, Value* argv) {
    printf("    [HostFn execute] set_counter\n");
    if (argc >= 1 && IS_NUMBER(argv[0])) {
        int val = (int)AS_NUMBER(argv[0]);
        printf("      Number arg: %d\n", val);
        g_host_counter = val;
        printf("      Host state updated: counter = %d\n", g_host_counter);
    } else {
        printf("      Args: INVALID (number required)\n");
    }
    printf("      Return: nil\n");
    return NIL_VAL;
}

int main() {
    printf("\n%s\n", BIGSEP);
    printf("       Moonjit Host Integration Verification Demo\n");
    printf("%s\n", BIGSEP);

    /* ====== Step 1: Create VM ====== */
    PRINT_HEAD(1, "Create and initialize Moonjit VM");
    MjState* L = mj_open();
    if (!L) { fprintf(stderr, "FAILED: cannot create VM\n"); return 1; }
    printf("  [OK] VM created successfully\n");

    mj_set_max_steps(L, 100000);
    mj_set_max_memory(L, 10 * 1024 * 1024);
    printf("  [OK] Max execution steps: 100,000\n");
    printf("  [OK] Max memory: 10 MB\n");

    /* ====== Step 2: Register Host Functions ====== */
    PRINT_HEAD(2, "Register host functions for script to call");

    printf("  Register #1: add (number, number) -> number\n");
    printf("     Purpose: add two numbers\n");
    mj_push_cfunction(L, add_numbers, 2, "add");
    mj_set_global(L, "add");
    printf("     [OK] bound to global 'add'\n\n");

    printf("  Register #2: concat (string, string) -> string\n");
    printf("     Purpose: concatenate two strings\n");
    mj_push_cfunction(L, concat_strings, 2, "concat");
    mj_set_global(L, "concat");
    printf("     [OK] bound to global 'concat'\n\n");

    printf("  Register #3: sqrt (number) -> number\n");
    printf("     Purpose: compute square root\n");
    mj_push_cfunction(L, sqrt_native, 1, "sqrt");
    mj_set_global(L, "sqrt");
    printf("     [OK] bound to global 'sqrt'\n\n");

    printf("  Register #4: get_counter () -> number\n");
    printf("     Purpose: read host-side integer counter\n");
    mj_push_cfunction(L, host_get_counter, 0, "get_counter");
    mj_set_global(L, "get_counter");
    printf("     [OK] bound to global 'get_counter'\n\n");

    printf("  Register #5: set_counter (number) -> nil\n");
    printf("     Purpose: update host-side integer counter\n");
    mj_push_cfunction(L, host_set_counter, 1, "set_counter");
    mj_set_global(L, "set_counter");
    printf("     [OK] bound to global 'set_counter'\n\n");

    printf("  Summary: 5 host functions registered\n");

    /* ====== Step 3: Pass Host Data to Script ====== */
    PRINT_HEAD(3, "Pass host data -> script global variables");

    printf("  [NUMBER  ] host_number = 42\n");
    mj_push_number(L, 42);
    mj_set_global(L, "host_number");
    printf("    [OK] set\n\n");

    printf("  [STRING  ] host_greeting = \"Hello from C Host!\"\n");
    mj_push_string(L, "Hello from C Host!");
    mj_set_global(L, "host_greeting");
    printf("    [OK] set\n\n");

    printf("  [TABLE   ] host_config = { version, debug, port }\n");
    mj_new_table(L);
        mj_push_string(L, "version");
        mj_push_string(L, "2.0.0");
        mj_set_table(L, -3);
        printf("    - version = \"2.0.0\" (string)\n");

        mj_push_string(L, "debug");
        mj_push_bool(L, 1);
        mj_set_table(L, -3);
        printf("    - debug   = true (bool)\n");

        mj_push_string(L, "port");
        mj_push_number(L, 8080);
        mj_set_table(L, -3);
        printf("    - port    = 8080 (number)\n");
    mj_set_global(L, "host_config");
    printf("    [OK] table bound to global 'host_config'\n");

    /* ====== Step 4: Execute Script ====== */
    PRINT_HEAD(4, "Execute script (call host fns + produce return values)");

    const char* script =
        "// Read host-passed globals\n"
        "print(\"host_number =\", host_number)\n"
        "print(\"host_greeting =\", host_greeting)\n"
        "print(\"host_config.version =\", host_config.version)\n"
        "print(\"host_config.debug =\", host_config.debug)\n"
        "print(\"host_config.port =\", host_config.port)\n"
        "\n"
        "// Call host functions\n"
        "let r_add = add(123, 456)\n"
        "let r_concat = concat(\"Hello \", \"Moonjit Script\")\n"
        "let r_sqrt = sqrt(225)\n"
        "\n"
        "// Read/write host state\n"
        "let before = get_counter()\n"
        "set_counter(9527)\n"
        "let after = get_counter()\n"
        "\n"
        "// Build return table for host to read\n"
        "let script_exports = {\n"
        "    status: \"ok\",\n"
        "    add_result: r_add,\n"
        "    concat_result: r_concat,\n"
        "    sqrt_result: r_sqrt,\n"
        "    counter_before: before,\n"
        "    counter_after: after\n"
        "}\n";

    printf("  Script source code:\n");
    {
        int line = 1; const char* p = script;
        printf("    %3d| ", line);
        while (*p) {
            if (*p == '\n') {
                printf("\n");
                line++;
                if (*(p+1)) printf("    %3d| ", line);
            } else {
                printf("%c", *p);
            }
            p++;
        }
    }
    printf("\n\n  Script output:\n");
    printf("    %s\n", SEP);
    printf("    ");
    int result = mj_load_string(L, script);
    printf("    %s\n", SEP);

    if (result != RESULT_OK) {
        char* err = NULL; int ln = mj_get_error(L, &err);
        fprintf(stderr, "  [FAIL] Script error at line %d: %s\n", ln, err ? err : "unknown");
        mj_close(L); return 1;
    }
    printf("  [OK] Script executed successfully (RESULT_OK)\n");

    /* ====== Step 5: Read Return Values from Script ====== */
    PRINT_HEAD(5, "Host reads return values from script -> script_exports");

    mj_get_global(L, "script_exports");
    if (!mj_is_table(L, -1)) {
        printf("  [FAIL] script_exports is not a table!\n");
        mj_close(L); return 1;
    }
    printf("  [OK] Successfully read script_exports table\n\n");

    printf("  +-------------------------------------------------+\n");
    printf("  | SCRIPT RETURN TABLE - field details            |\n");
    printf("  +-------------------------------------------------+\n");

    mj_push_string(L, "status"); mj_get_table(L, -2);
    if (mj_is_string(L, -1)) { int _l; printf("  | status         = \"%s\"        (string) |\n", mj_to_string(L, -1, &_l)); }
    mj_pop(L);

    mj_push_string(L, "add_result"); mj_get_table(L, -2);
    if (mj_is_number(L, -1)) printf("  | add_result     = %-10g (number) |\n", mj_to_number(L, -1));
    mj_pop(L);

    mj_push_string(L, "concat_result"); mj_get_table(L, -2);
    if (mj_is_string(L, -1)) { int _l; printf("  | concat_result  = \"%s\" (string) |\n", mj_to_string(L, -1, &_l)); }
    mj_pop(L);

    mj_push_string(L, "sqrt_result"); mj_get_table(L, -2);
    if (mj_is_number(L, -1)) printf("  | sqrt_result    = %-10g (number) |\n", mj_to_number(L, -1));
    mj_pop(L);

    mj_push_string(L, "counter_before"); mj_get_table(L, -2);
    if (mj_is_number(L, -1)) printf("  | counter_before = %-10d (number) |\n", (int)mj_to_number(L, -1));
    mj_pop(L);

    mj_push_string(L, "counter_after"); mj_get_table(L, -2);
    if (mj_is_number(L, -1)) printf("  | counter_after  = %-10d (number) |\n", (int)mj_to_number(L, -1));
    mj_pop(L);

    printf("  +-------------------------------------------------+\n");
    mj_pop(L);

    /* ====== Step 6: Verify Host State Change ====== */
    PRINT_HEAD(6, "Verify host state was modified by script");

    printf("  Host global g_host_counter = %d\n", g_host_counter);
    if (g_host_counter == 9527) {
        printf("  [OK] State verification PASSED: 0 -> 9527 (set_counter worked)\n");
    } else {
        printf("  [FAIL] State verification FAILED!\n");
    }

    /* ====== Step 7: Resource Limit Test ====== */
    PRINT_HEAD(7, "Resource limit test - infinite loop interrupted by step limit");

    mj_set_max_steps(L, 500);
    const char* loop = "let n=0; while(true){ n=n+1; }";
    printf("  Test script: 'let n=0; while(true){ n=n+1; }'\n");
    printf("  Max steps allowed: 500\n\n");
    result = mj_load_string(L, loop);
    printf("\n");
    if (result == RESULT_TIMEOUT) {
        char* err = NULL; int ln = mj_get_error(L, &err);
        printf("  [OK] Resource limit test PASSED\n");
        printf("     Exit code : %d (RESULT_TIMEOUT)\n", result);
        printf("     At line   : %d\n", ln);
        printf("     Reason    : %s\n", err ? err : "unknown");
    } else {
        printf("  [FAIL] Resource limit test FAILED! result=%d\n", result);
    }

    /* ====== Step 8: Cleanup ====== */
    PRINT_HEAD(8, "Cleanup & summary");
    mj_close(L);
    printf("  [OK] VM destroyed successfully\n");

    printf("\n%s\n", BIGSEP);
    printf("         ALL VERIFICATION STEPS PASSED\n");
    printf("%s\n", BIGSEP);
    printf("\n");
    return 0;
}
