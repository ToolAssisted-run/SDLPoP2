/* Coroutines (coro.h): ucontext on POSIX, fibers on Windows. */
#include <stdlib.h>
#include "coro.h"

#ifdef _WIN32
#include <windows.h>
struct coro { LPVOID fiber, caller; void (*fn)(void); };
static LPVOID running;   /* the fiber running now (the thread's own until a coroutine starts) */
static void WINAPI fiber_main(LPVOID p)
{
	coro *c = p;
	c->fn();
	for (;;) coro_yield(c);
}
coro *coro_create(void (*fn)(void), size_t stack_size)
{
	coro *c = calloc(1, sizeof *c); if (!c) return NULL;
	c->fn = fn;
	c->fiber = CreateFiber(stack_size, fiber_main, c);
	if (!c->fiber) { free(c); return NULL; }
	return c;
}
void coro_resume(coro *c)
{
	if (!running) running = ConvertThreadToFiber(NULL);
	c->caller = running; running = c->fiber;
	SwitchToFiber(c->fiber);
}
void coro_yield(coro *c) { running = c->caller; SwitchToFiber(c->caller); }
void coro_destroy(coro *c) { if (!c) return; DeleteFiber(c->fiber); free(c); }

#else
#include <ucontext.h>
struct coro { ucontext_t ctx, caller; char *stack; void (*fn)(void); int started; };
static coro *starting;   /* (makecontext passes no pointer portably: the coroutine being started) */
static void trampoline(void)
{
	coro *c = starting;
	c->fn();
	for (;;) swapcontext(&c->ctx, &c->caller);
}
coro *coro_create(void (*fn)(void), size_t stack_size)
{
	coro *c = calloc(1, sizeof *c); if (!c) return NULL;
	c->fn = fn; c->stack = malloc(stack_size);
	if (!c->stack) { free(c); return NULL; }
	getcontext(&c->ctx);
	c->ctx.uc_stack.ss_sp = c->stack; c->ctx.uc_stack.ss_size = stack_size; c->ctx.uc_link = NULL;
	makecontext(&c->ctx, trampoline, 0);
	return c;
}
void coro_resume(coro *c) { if (!c->started) { c->started = 1; starting = c; } swapcontext(&c->caller, &c->ctx); }
void coro_yield(coro *c) { swapcontext(&c->ctx, &c->caller); }
void coro_destroy(coro *c) { if (!c) return; free(c->stack); free(c); }
#endif
