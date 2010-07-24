#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include "stdio.h"

void show_backtrace (void) {
  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip, sp;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    printf ("ip = %lx, sp = %lx\n", (long) ip, (long) sp);
  }
}

void g()
{
    show_backtrace();
}

void f()
{
    g();
}


int main()
{
    f();
    return 0;
}
