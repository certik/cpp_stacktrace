C++ Stacktrace
==============

This library allows you to print the stacktrace just like Python, with
filenames, line numbers, function names and the line itself.

It works for shared libraries too.

Usage
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

Here is an example of a traceback in a shared library::

    Traceback (most recent call last):
      File "/build/buildd/eglibc-2.10.1/csu/../sysdeps/x86_64/elf/start.S", line 113, in _start
      File "/build/buildd/eglibc-2.10.1/csu/libc-start.c", line 220, in __libc_start_main
      File "/home/ondrej/repos/hermes2d/benchmarks/bessel/main.cpp", line 140, in main
            rs.assemble();
      File "/home/ondrej/repos/hermes2d/src/refsystem.cpp", line 178, in RefSystem::assemble(bool)
          if (this->linear == true) LinSystem::assemble(rhsonly);
      File "/home/ondrej/repos/hermes2d/src/linsystem.cpp", line 618, in LinSystem::assemble(bool)
                      bi = eval_form(jfv, NULL, fu, fv, &refmap[n], &refmap[m]) * an->coef[j] * am->coef[i];
      File "/home/ondrej/repos/hermes2d/src/linsystem.cpp", line 837, in LinSystem::eval_form(WeakForm::JacFormVol*, Solution**, PrecalcShapeset*, PrecalcShapeset*, RefMap*, RefMap*)
          show_backtrace();
      File "/home/ondrej/repos/cpp_stacktrace/backtrace-symbols.c", line 420, in show_backtrace()
          size = backtrace (array, 10);


If you want to use it in your project, link with ``-lbfd -liberty``, compile
with ``-g`` and apply this (or a similar patch) to your main.cpp::

    --- a/benchmarks/bessel/main.cpp
    +++ b/benchmarks/bessel/main.cpp
    @@ -83,8 +83,11 @@ scalar essential_bc_values(int ess_bdy_marker, double x, double y)
       return 0;
     }

    +#include "/home/ondrej/repos/cpp_stacktrace/stacktrace.cpp"
    +
     int main(int argc, char* argv[])
     {
    +    print_stack_on_segfault();
       // Time measurement
       TimePeriod cpu_time;
       cpu_time.tick();


And that's it. When it segfaults, it will print a nice stacktrace.


Notes
-----

You need to compile with debugging symbols "-g", otherwise it will not find the
filenames and line numbers (only function names).

Links
-----

http://stackoverflow.com/questions/105659/how-can-one-grab-a-stack-trace-in-c

http://tombarta.wordpress.com/2008/08/01/c-stack-traces-with-gcc/
