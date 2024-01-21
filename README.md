# The virtual machine approach to optimised regular expression matching in C

The repo contains separate Rust source files for each stage of compiling
_regular expressions_ into _virtual machine (VM)_ instructions. The main
framework source code is contained over the C files `bru.c`, `compiler.c`,
`parser.c`, `scheduler.h`, `smir.c`, `sre.c`, and `srvm.c` in the `src/`
directory.

The `sre.c` and `srvm.c` files contain the necessary definitions for the parse
tree of a regular expression, and the definitions for the VM, respectively. The
`parser.c` file contains the code to parse regular expressions and produce a
parse tree as an intermediate representation for generating VM instructions.

The `compiler.c` file contains the code to generate VM instructions from the
parse tree using the constructions defined in other files (e.g. `thompson.c` and
`glushkov.c`).

Finally, the actual regular expression matcher is contained in the `srvm.c` file
which contains the source code for the actual execution of the VM with compiled
VM instructions.

## Compiling the source files

This project includes a `Makefile` in which you can compile all the binaries by
executing:

```bash
make
```

## Executing the binaries

You can run the parser to print the parse tree of a given regular expression as
follows:

```bash
./bin/bru parse [OPTIONS] <regex>
```

To compile a VM program from a regular expression and print out the instructions
of the program, you can run:

```bash
./bin/bru compile [OPTIONS] <regex>
```

To run the BRU matcher, simply run:

```bash
./bin/bru match [OPTIONS] <regex> <input>
```

The program compiles the regular expression into VM instructions and tries to
match it over the given input string, and will print whether the regular
expression matched the input string, as well as the capturing information.
