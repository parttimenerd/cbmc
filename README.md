[CProver Wiki](http://www.cprover.org/wiki)

[CProver Documentation](http://cprover.diffblue.com)

(Original) About
================

CBMC is a Bounded Model Checker for C and C++ programs. It supports C89, C99,
most of C11 and most compiler extensions provided by gcc and Visual Studio. It
also supports SystemC using Scoot. It allows verifying array bounds (buffer
overflows), pointer safety, exceptions and user-specified assertions.
Furthermore, it can check C and C++ for consistency with other languages, such
as Verilog. The verification is performed by unwinding the loops in the program
and passing the resulting equation to a decision procedure.

For full information see [cprover.org](http://www.cprover.org/cbmc).

For an overview of the various tools that are part of CProver and
how to use them see [TOOLS_OVERVIEW.md](TOOLS_OVERVIEW.md).

Modifications
=============

This version of CBMC is modified to capture information needed for over-approximating loops and unbound recursions (when
passed an unwinding limit). The idea is to not make any assumptions on the bodies of aborted loops and recursion.
Meta information is collected that can be read with a tool like [dsharpy](https://git.scc.kit.edu/gp1285/dsharpy)
(and is emitted when using the DIMACS output).

The goal is to use it first and foremost in the field of quantitative information flow. The tool itself should
be usable as a soundier replacement for CBMC in tools like [ApproxFlow](https://github.com/approxflow/approxflow)
to provide over approximations (but be aware that these over-approximations can be far to high, as every aborted
loop or recursion introduces new entropy, ongoing work into dsharpy tries to work mitigate some of these effects):
Preliminary evaluations based the benchmarks from [nildumu](https://github.com/parttimenerd/nildumu) showed that
ApproxFlow is sound for common benchmarks in the field of quantitative information flow for all admissible unwinding
limits (a limit >= 3 is required, use the modified version of [ApproxFlow](https://github.com/parttimenerd/approxflow/)).

Loops
-----
Consider the following program (found under `examples/test_loop.cpp`)

```c
#include <assert.h>

void main()
{
   int i = 0;
   while(i < 1000)
   {
      i = i + 1;
   }
   assert(false);
}
```

which we can analyze with our modified CBMC and an unwinding limit of for example 3 using
`cbmc --unwind 3 --dimacs examples/test_loop.cpp`.
This results in the following truncated output:
```c
[…]
p cnf 250 525
[…]
-2 250 0
2 -250 0
c __CPROVER_malloc_is_new_array#1 FALSE
c goto_symex::\guard#1 1
c __CPROVER_malloc_is_new_array#2 FALSE
c goto_symex::\guard#2 2
[…]
c main::1::i!0@1#7 134 135 136 137 138 139 140 141 142 143 144 145 146 147 148 149 150 151 152 153 154 155 156 157 158 159 160 161 162 163 164 165
c __CPROVER_rounding_mode#1 FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE
c loop 0 main 0 -1 | sfoa 0 | guards goto_symex::\guard#1 | lguard goto_symex::\guard#1 | linput main::1::i!0@1 oa_constant::$1!0 | lmisc_input | linner_input main::1::i!0@1 main::1::i!0@1#5 | linner_output main::1::i!0@1 main::1::i!0@1#6 | loutput main::1::i!0@1 main::1::i!0@1#7
```

The `c loop` lines contain the following information for every aborted loop (use the mentioned variables
and the variable → SAT variable mapping to obtain the SAT variables for further processing):

```c
c loop [loop id] [function name] [loop nr] [parent loop or -1]
| sfoa [1: loop can be fully over approximated, 0: not, just a hint]`
| guards ['-' if negative][constraint var 1, conditions that are satisfied at the start of the loop] […]
| lguard [… same but for the last (and therefore abstract) iteration]
| linput [loop input/read variable 1] [instantiation 1] […]
| lmisc_input [guard, uninitialized or constant variable 1] […] [just treat it as input]
| linner_input [abstract iteration input/read variable 1] [instantiation 1] […]
| linner_output [abstract iteration output/written variable 1] [instantiation 1] […]
| loutput [loop output/written variable 1] [instantiation 1] […]
```
The line breaks are only for readability.

Recursion
---------
Consider the following program (found under `examples/test_recursion.cpp`)

```c
#include <assert.h>

int non_det();

int fib(int num)
{
   if(num > 2)
   {
      return fib(num - 1) + 1;
   }
   return num;
}

void main()
{
   int b = fib(non_det());
   assert(b);
}
```

which we can analyze with our modified CBMC and an unwinding limit of for example 3 using
`cbmc --unwind 3 --dimacs examples/test_recursion.cpp`.
This results in the following truncated output:
```c
…
p cnf 1368 3643
…
1366 1367 1368 0
-11 1368 0
11 -1368 0
c __CPROVER_malloc_is_new_array#1 FALSE
c goto_symex::\guard#1 1
c goto_symex::\guard#2 2
c goto_symex::\guard#3 4
c goto_symex::\guard#4 6
[…]
c fib(signed_int)#return_value!0#2 -426 459 461 463 465 467 469 471 473 475 477 479 481 483 485 487 489 491 493 495 497 499 501 503 505 507 509 511 513 515 517 519
c __CPROVER_deallocated#1 FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE FALSE
c fib(signed_int)::$tmp::return_value_fib!0@3#2 582 583 584 585 586 587 588 589 590 591 592 593 594 595 596 597 598 599 600 601 602 603 604 605 606 607 608 609 610 611 612 613
c rec child 0 fib(signed_int) | input fib(signed_int)::num fib(signed_int)::num!0@5#1 | output fib(signed_int)#return_value fib(signed_int)#return_value!0#1 | constraint goto_symex::\guard#1 goto_symex::\guard#2 goto_symex::\guard#3 goto_symex::\guard#4
```

Format of the `c rec` lines:
`c rec child [id of this application] [name of the function] | input [input/read var 1] [instantiation 1] […] |
 output [output/written var 1] [instantiation 1] | constraint ['-' if negative][constraint var 1] […]`

This line gives us information on the aborted recursion: the recursive has not been evaluated, but the new variables
for the result (and the effects) of the function call have been introduced.

It is also possible to gain knownledge on the recursive call by executing an abstract version (all read variables are
unknown at the start of the function application), this can be done be setting the environment variable 
`ENABLE_REC_GRAPH`: The call `ENABLE_REC_GRAPH="" cbmc --unwind 3 --dimacs examples/test_recursion.cpp`
results in
```c
[…]
c rec node fib(signed_int) | input fib(signed_int)::num fib(signed_int)::num!0@1#2 | output fib(signed_int)#return_value fib(signed_int)#return_value!0#19
c rec child 0 fib(signed_int) | input fib(signed_int)::num fib(signed_int)::num!0@5#1 | output fib(signed_int)#return_value fib(signed_int)#return_value!0#1 | constraint goto_symex::\guard#1 goto_symex::\guard#2 goto_symex::\guard#3 goto_symex::\guard#4
c rec child 1 fib(signed_int) | input fib(signed_int)::num fib(signed_int)::num!0@6#1 | output fib(signed_int)#return_value fib(signed_int)#return_value!0#15 | constraint goto_symex::\guard#5
```
With `c rec node […]` describing the abstract recursion and `c rec child 1 […]` describing the recursive call
aborted during this execution (this is evident from the dependencies of the variables).

The format of `c rec node` is: `c rec node [func name] | input [input var 1] [instantiation 1] […] | output [output var 1] [instantiation 1] […]`.
The abstract recursions take place after the program itself is fully processed.

Options
-------

| environment variable | if set with any value, the following happens
|--------- | ------
| LOG_SAT  | log the names that belong to all named SAT variables
| SKIP_CPROVER_VARIABLES | skip CProver variables (all that start with an underscore) in comments output
| LOG_LOOP        | log start and end of last loop iteration and variable assignments
| LOG_LOOP_MERGES | log name merges on phis and more loop info
| LOG_ACCESS | log accessed variables
| LOG_ASSIGN | log assigned variables
| LOG_REC  | log stuff related to aborted recursions
| REC      | depth of recursion for a function (e.g. REC=0 → online inline the first occurrence in the depth tree) 
| ENABLE_REC_GRAPH   | enable abstract calls (same as passing REC_GRAPH_INLINING without an argument) with abstract recursions
| REC_GRAPH_INLINING | depth of recursion in abstract calls, default is 0 → no inlining in abstract calls

Development
-----------
This modification is developed alongside [dsharpy](https://git.scc.kit.edu/gp1285/dsharpy) and
currently rebased on the commit `26f9de7b` from the [CBMC GitHub repo](https://github.com/diffblue/cbmc).

Todos direcly related to the modification are labeled `dtodo`.

Versions
========

Get the [latest release](https://github.com/diffblue/cbmc/releases)
* Releases are tested and for production use.

Get the current *develop* version: `git clone https://github.com/diffblue/cbmc.git`
* Develop versions are not recommended for production use.

Installing
==========

### Windows

For windows you can install cbmc binaries via the .msi's found on the
[releases](https://github.com/diffblue/cbmc/releases) page.

Note that we depend on the Visual C++ redistributables. You likely
already have these, if not please download and run vcredist.x64.exe from
[Microsoft](https://support.microsoft.com/en-gb/help/2977003/the-latest-supported-visual-c-downloads) to install them prior to running
cbmc.

### Linux

For different linux environments, you have these choices:

1. Install CBMC through the distribution's repositories, with the downside
   that this might install an older version of cbmc, depending on what the
   package maintenance policy of the distribution is, or
2. Install CBMC through the `.deb` package built by each release, available
   on the [releases](https://github.com/diffblue/cbmc/releases) page. To
   do that, download the `.deb` package and run `apt install cbmc-x.y.deb`
   with `root` privileges, with `x.y` being substituted for the version
   you are attempting to install.

   *NOTE*: Because of libc/libc++ ABI compatibility and package
   dependency names, if you follow this path make sure you install the
   package appropriate for the version of operating system you are using.
3. Compile from source using the instructions [here](COMPILING.md)

### macOS

For macOS there is a [Homebrew](https://brew.sh) package
[available](https://formulae.brew.sh/formula/cbmc). Once you have installed
Homebrew, simply run

    brew install cbmc

to install cbmc, or if you already have it installed via homebrew

    brew upgrade cbmc

to get an up-to-date version.

Report bugs
===========

If you encounter a problem please file a bug report:
* Create an [issue](https://github.com/diffblue/cbmc/issues)

Contributing to the code base
=============================

1. Fork the repository
2. Clone the repository `git clone git@github.com:YOURNAME/cbmc.git`
3. Create a branch from the `develop` branch (default branch)
4. Make your changes (follow the [coding guidelines](https://github.com/diffblue/cbmc/blob/develop/CODING_STANDARD.md))
5. Push your changes to your branch
6. Create a Pull Request targeting the `develop` branch

New contributors can look through the [mini
projects](https://github.com/diffblue/cbmc/blob/develop/MINI-PROJECTS.md)
page for small, focussed feature ideas.

License
=======
4-clause BSD license, see `LICENSE` file.


[codebuild]: https://us-east-1.console.aws.amazon.com/codesuite/codebuild/projects/cbmc/history?region=us-east-1
[codebuild_img]: https://codebuild.us-east-1.amazonaws.com/badges?uuid=eyJlbmNyeXB0ZWREYXRhIjoiajhxcmNGUEgyV0xZa2ZFaVd3czJmbm1DdEt3QVdJRVdZaGJuMTUwOHFrZUM3eERwS1g4VEQ3Ymw3bmFncldVQXArajlYL1pXbGZNVTdXdndzUHU4Ly9JPSIsIml2UGFyYW1ldGVyU3BlYyI6IkVUUEdWVEt0SUFONlhyNVAiLCJtYXRlcmlhbFNldFNlcmlhbCI6MX0%3D&branch=develop
[codebuild_windows]: https://us-east-1.console.aws.amazon.com/codesuite/codebuild/projects/cbmc-windows/history?region=us-east-1
[codebuild_windows_img]: https://codebuild.us-east-1.amazonaws.com/badges?uuid=eyJlbmNyeXB0ZWREYXRhIjoiTFQ4Q0lCSEc1Rk5NcmlzaFZDdU44Vk8zY0c1VCtIVWMwWnJMRitmVFI5bE94Q3dhekVPMWRobFU2Q0xTTlpDSWZUQ3J1eksrWW1rSll1OExXdll2bExZPSIsIml2UGFyYW1ldGVyU3BlYyI6InpqcloyaEdxbjBiQUtvNysiLCJtYXRlcmlhbFNldFNlcmlhbCI6MX0%3D&branch=develop
[coverity]: https://scan.coverity.com/projects/diffblue-cbmc
[coverity_img]: https://scan.coverity.com/projects/13552/badge.svg
[codecov]: https://codecov.io/gh/diffblue/cbmc
[codecov_img]: https://codecov.io/gh/diffblue/cbmc/branch/develop/graphs/badge.svg
