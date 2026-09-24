#pragma once
/* Coroutines for the parts of the original that are straight-line code with busy waits (the program shell, the story
 * scenes): each runs on its own stack and hands control back where the DOS program would wait. ucontext on POSIX,
 * fibers on Windows. A coroutine yields to whoever resumed it last (coroutines may resume others). */
#include <stddef.h>

typedef struct coro coro;
coro *coro_create(void (*fn)(void), size_t stack_size);   /* fn runs at the first resume; when it returns, the coroutine
                                                             yields for ever (resuming it again returns at once) */
void coro_resume(coro *c);    /* run c until it yields */
void coro_yield(coro *c);     /* inside c: back to the resumer */
void coro_destroy(coro *c);   /* (not while it runs) */
