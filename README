Build
-----

Here is how to use it::

    $ ./compile
    $ ./b
    Traceback (most recent call last):
      File "/build/buildd/eglibc-2.10.1/csu/../sysdeps/x86_64/elf/start.S", line 113, in _start
      File "/build/buildd/eglibc-2.10.1/csu/libc-start.c", line 220, in __libc_start_main
      File "/home/ondrej/repos/cpp_stacktrace/b.c", line 19, in main
            f();
      File "/home/ondrej/repos/cpp_stacktrace/b.c", line 13, in f()
            g();
      File "/home/ondrej/repos/cpp_stacktrace/b.c", line 8, in g()
            show_backtrace();
      File "/home/ondrej/repos/cpp_stacktrace/backtrace-symbols.c", line 420, in show_backtrace()
          size = backtrace (array, 10);


Notes
-----

http://stackoverflow.com/questions/105659/how-can-one-grab-a-stack-trace-in-c

http://tombarta.wordpress.com/2008/08/01/c-stack-traces-with-gcc/

You need to use both "-g" (for addr2line to work) and "-rdynamic" (for
addr2line to work and the backtrace to find out the function names)
