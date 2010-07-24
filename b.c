#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

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
