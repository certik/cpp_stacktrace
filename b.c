#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

void show_backtrace (void);

void g()
{
    show_backtrace();
}

void f()
{
    g();
}

void segfault_callback(int sig_num)
{
    printf("\nSegfault caught. Printing stacktrace:\n\n");
    show_backtrace();
    printf("\nDone.\n");
    exit(-1);
}


int main()
{
    // Install the segfault signal handler. It will print a stacktrace and then
    // exit:
    signal(SIGSEGV, segfault_callback);

    f();

    // This will segfault:
    char *p; *p=0;

    return 0;
}
