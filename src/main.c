#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "host.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        InterpretResult result = vm_interpret(line);
        if (result != RESULT_OK) {
            fprintf(stderr, "Error: ");
            switch (result) {
                case RESULT_COMPILE_ERROR:
                    fprintf(stderr, "Compile error");
                    break;
                case RESULT_RUNTIME_ERROR:
                    fprintf(stderr, "Runtime error: %s (line %d)",
                            vm.error_message ? vm.error_message : "unknown",
                            vm.error_line);
                    break;
                case RESULT_OUT_OF_MEMORY:
                    fprintf(stderr, "Out of memory");
                    break;
                case RESULT_STACK_OVERFLOW:
                    fprintf(stderr, "Stack overflow");
                    break;
                case RESULT_TIMEOUT:
                    fprintf(stderr, "Execution timeout (too many steps)");
                    break;
                default:
                    fprintf(stderr, "Unknown error (code %d)", result);
                    break;
            }
            fprintf(stderr, "\n");
        }
    }
}

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

static void run_file(const char* path) {
    char* source = read_file(path);
    InterpretResult result = vm_interpret(source);
    free(source);

    switch (result) {
        case RESULT_OK:
            break;
        case RESULT_COMPILE_ERROR:
            fprintf(stderr, "Compile error.\n");
            exit(65);
        case RESULT_RUNTIME_ERROR:
            fprintf(stderr, "Runtime error: %s (line %d)\n",
                    vm.error_message ? vm.error_message : "unknown",
                    vm.error_line);
            exit(70);
        case RESULT_OUT_OF_MEMORY:
            fprintf(stderr, "Out of memory.\n");
            exit(71);
        case RESULT_STACK_OVERFLOW:
            fprintf(stderr, "Stack overflow.\n");
            exit(72);
        case RESULT_TIMEOUT:
            fprintf(stderr, "Execution timeout: script exceeded maximum steps.\n");
            exit(73);
    }
}

int main(int argc, const char* argv[]) {
    vm_init();

    vm.max_execution_steps = 10000000;
    vm.max_memory = 64 * 1024 * 1024;

    const char* script_path = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("moonjit %s\n", MJ_VERSION);
            printf("A small, fast, embeddable scripting language\n");
            exit(0);
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: moonjit [options] [script]\n");
            printf("\n");
            printf("Options:\n");
            printf("  -h, --help              Show this help message\n");
            printf("  -v, --version           Show version information\n");
            printf("  --max-steps <N>         Maximum execution steps (default: 10,000,000)\n");
            printf("  --max-memory <MB>       Maximum memory in MB (default: 64 MB)\n");
            printf("  --no-gc                 Disable garbage collector (for debugging)\n");
            printf("\n");
            printf("If no script is given, enters interactive REPL mode.\n");
            exit(0);
        }
        if (strcmp(argv[i], "--max-steps") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --max-steps requires a value\n");
                exit(64);
            }
            vm.max_execution_steps = atoi(argv[i + 1]);
            i++;
            continue;
        }
        if (strcmp(argv[i], "--max-memory") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --max-memory requires a value\n");
                exit(64);
            }
            int mb = atoi(argv[i + 1]);
            vm.max_memory = (size_t)mb * 1024 * 1024;
            i++;
            continue;
        }
        if (strcmp(argv[i], "--no-gc") == 0) {
            vm.gc_enabled = false;
            continue;
        }
        // If not an option, it's the script path
        if (argv[i][0] != '-' && script_path == NULL) {
            script_path = argv[i];
        }
    }

    if (script_path == NULL) {
        repl();
    } else {
        run_file(script_path);
    }

    vm_free();
    return 0;
}
