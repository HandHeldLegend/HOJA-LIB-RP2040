#include "utilities/hooks.h"

static hook_function_t _hooks_0[MAX_HOOKS];
static hook_function_t _hooks_1[MAX_HOOKS];
static size_t _hooks_0_count = 0;
static size_t _hooks_1_count = 0;

void hooks_register(hook_function_t hook, int core) {

    if(core==0)
    {
        if (_hooks_0_count < MAX_HOOKS) 
        {
            _hooks_0[_hooks_0_count++] = hook;
        }
    }
    else if (core==1)
    {
        if (_hooks_1_count < MAX_HOOKS) 
        {
            _hooks_1[_hooks_1_count++] = hook;
        }
    }
}

void hooks_run_all_core0(void) {
    if(!_hooks_0_count) return;

    for (size_t i = 0; i < _hooks_0_count; i++) {
        _hooks_0[i]();
    }
}

void hooks_run_all_core1(void) {
    if(!_hooks_1_count) return;

    for (size_t i = 0; i < _hooks_1_count; i++) {
        _hooks_1[i]();
    }
}