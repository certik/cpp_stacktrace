#include <stdio.h>
#include <stdlib.h>

void show_backtrace (void);

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
