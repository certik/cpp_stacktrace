#include <stdio.h>
#include <stdlib.h>

#include "stacktrace.h"

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
    print_stack_on_segfault();

    f();

    // This will segfault:
    char *p; *p=0;

    return 0;
}
