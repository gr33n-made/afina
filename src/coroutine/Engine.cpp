#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char stack_end;

    ctx.Hight = StackBottom;
    ctx.Low = &stack_end;

    if (std::get<0>(ctx.Stack) != nullptr)
        delete std::get<0>(ctx.Stack);

    ctx.Stack = std::make_tuple(new char[ctx.Hight-ctx.Low], ctx.Hight-ctx.Low);
    memcpy(std::get<0>(ctx.Stack), ctx.Low, ctx.Hight-ctx.Low);
}

void Engine::Restore(context &ctx) {

    char stack_end;
    if (ctx.Low < &stack_end)
        //Restore(ctx);

    memcpy(StackBottom - std::get<1>(ctx.Stack), std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    if (alive == nullptr)
        return;

    context *ctx_for_schedule = alive;
    while (ctx_for_schedule == cur_routine) {
        ctx_for_schedule = ctx_for_schedule->next;
        if (ctx_for_schedule == nullptr)
            return;
    }

    if (cur_routine != nullptr) {
        Store(*cur_routine);
        if (setjmp(cur_routine->Environment)!=0)
            return;
    }

    cur_routine = ctx_for_schedule;
    Restore(*cur_routine);
}

void Engine::sched(void *routine_) {
    context *ctx = (context*)routine_, *ctx_for_schedule;

    // find routine in list for execute
    ctx_for_schedule = alive;
    while (ctx_for_schedule != ctx) {
        ctx_for_schedule = ctx_for_schedule->next;
        if (ctx_for_schedule == nullptr)
            return;
    }

    if (cur_routine != nullptr) {
        Store(*cur_routine);

        if (setjmp(cur_routine->Environment) != 0)
            return;
    }
    cur_routine = ctx;
    Restore(*cur_routine);
}

} // namespace Coroutine
} // namespace Afina
