#include <stdio.h>
#include <stdlib.h>

void show_backtrace (void);
void print_stack_on_segfault();

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
