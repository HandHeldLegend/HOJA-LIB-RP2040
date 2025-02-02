#ifndef UTILITIES_HOOKS_H
#define UTILITIES_HOOKS_H

#include <stddef.h>

// Define hook function type
typedef void (*hook_function_t)(void);

// Maximum number of hooks (adjust as needed)
#define MAX_HOOKS 16

// Declare the hook registration function
void hooks_register(hook_function_t hook, int core);

// Run all registered hooks
void hooks_run_all_core0(void);
void hooks_run_all_core1(void);

// Helper macro to register hooks in init code
#define HOOK_REGISTER(func, corenum) \
    hooks_register(func, corenum)

#endif // UTILITIES_HOOKS_H