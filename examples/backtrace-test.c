#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

void print_diagnostics(void) {
    printf("\n=== DIAGNOSTICS ===\n");
    
    // Check if we're on musl
    #ifdef __GLIBC__
    printf("libc: glibc %d.%d\n", __GLIBC__, __GLIBC_MINOR__);
    #else
    printf("libc: likely musl or other (not glibc)\n");
    #endif
    
    // Print frame pointer info
    void *fp;
    #ifdef __x86_64__
    __asm__("movq %%rbp, %0" : "=r" (fp));
    #elif defined(__i386__)
    __asm__("movl %%ebp, %0" : "=r" (fp));
    #elif defined(__aarch64__)
    __asm__("mov %0, x29" : "=r" (fp));
    #elif defined(__arm__)
    __asm__("mov %0, fp" : "=r" (fp));
    #else
    fp = __builtin_frame_address(0);
    #endif
    printf("Current frame pointer: %p\n", fp);
    
    // Try __builtin_return_address
    void *ret_addr = __builtin_return_address(0);
    printf("Return address: %p\n", ret_addr);
    
    printf("===================\n\n");
}

#ifdef HAVE_LIBUNWIND
void print_libunwind_backtrace(void) {
    printf("\n=== LIBUNWIND BACKTRACE ===\n");
    
    unw_cursor_t cursor;
    unw_context_t context;
    
    // Get current context
    int ret = unw_getcontext(&context);
    if (ret < 0) {
        printf("ERROR: unw_getcontext() failed: %d\n", ret);
        return;
    }
    
    // Initialize cursor
    ret = unw_init_local(&cursor, &context);
    if (ret < 0) {
        printf("ERROR: unw_init_local() failed: %d\n", ret);
        return;
    }
    
    int frame = 0;
    do {
        unw_word_t ip;
        ret = unw_get_reg(&cursor, UNW_REG_IP, &ip);
        if (ret < 0) {
            printf("ERROR: unw_get_reg() failed: %d\n", ret);
            break;
        }
        
        // Try to get symbol name
        char symbol[256];
        unw_word_t offset;
        ret = unw_get_proc_name(&cursor, symbol, sizeof(symbol), &offset);
        
        if (ret == 0) {
            printf("Frame %2d: 0x%016lx in %s+0x%lx\n", frame, (unsigned long)ip, symbol, (unsigned long)offset);
        } else {
            printf("Frame %2d: 0x%016lx in <unknown>\n", frame, (unsigned long)ip);
        }
        
        frame++;
        if (frame > 20) {  // Prevent infinite loops
            printf("... (truncated, too many frames)\n");
            break;
        }
        
    } while ((ret = unw_step(&cursor)) > 0);
    
    if (ret < 0) {
        printf("ERROR: unw_step() failed: %d\n", ret);
    }
    
    printf("Total frames: %d\n", frame);
    printf("===========================\n\n");
}
#endif

void func5(void) {
    printf("In func5 - about to call backtrace()\n");
    
    print_diagnostics();
    
    void *buffer[10];
    memset(buffer, 0, sizeof(buffer));
    
    printf("Calling backtrace() with buffer size 10...\n");
    int nptrs = backtrace(buffer, 10);
    
    printf("backtrace() returned %d addresses\n", nptrs);
    
    // Print raw addresses even if nptrs is 0
    printf("Raw buffer contents:\n");
    for (int i = 0; i < 10; i++) {
        printf("  buffer[%d] = %p\n", i, buffer[i]);
    }
    
    if (nptrs > 0) {
        printf("\nCalling backtrace_symbols_fd() to stderr:\n");
        fflush(stdout);
        backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);
        
        printf("\nTrying backtrace_symbols() to get symbol names:\n");
        char **symbols = backtrace_symbols(buffer, nptrs);
        if (symbols) {
            for (int i = 0; i < nptrs; i++) {
                printf("  [%d] %s\n", i, symbols[i]);
            }
            free(symbols);
        } else {
            printf("  backtrace_symbols() returned NULL\n");
        }
    } else {
        printf("ERROR: backtrace() returned %d addresses\n", nptrs);
        printf("This is common on musl libc systems.\n");
        printf("Possible solutions:\n");
        printf("1. Install libunwind-dev and use libunwind instead\n");
        printf("2. Compile with -funwind-tables and -fasynchronous-unwind-tables\n");
        printf("3. Use alternative stack walking methods\n");
    }
    
#ifdef HAVE_LIBUNWIND
    print_libunwind_backtrace();
#else
    printf("\nLibunwind not available (compile with -DHAVE_LIBUNWIND and link -lunwind)\n");
#endif
    
    printf("\nDone with backtrace in func5\n");
}

void func4(void) {
    printf("In func4 - calling func5\n");
    func5();
    printf("Back in func4\n");
}

void func3(void) {
    printf("In func3 - calling func4\n");
    func4();
    printf("Back in func3\n");
}

void func2(void) {
    printf("In func2 - calling func3\n");
    func3();
    printf("Back in func2\n");
}

void func1(void) {
    printf("In func1 - calling func2\n");
    func2();
    printf("Back in func1\n");
}

int main(void) {
    printf("Starting backtrace test - calling func1\n");
    func1();
    printf("Back in main - test complete\n");
    return 0;
}