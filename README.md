# The virtual machine approach to optimised regular expression matching in C

The repo contains separate Rust source files for each stage of compiling
_regular expressions_ into _virtual machine (VM)_ instructions. The relevant
source code is contained over the C files `compile.c`, `compiler.c`, `parse.c`,
`parser.c`, `sre.c`, and `srvm.c` in the `src/` directory.

The `sre.c` and `srvm.c` files contain the necessary definitions for the parse
tree of a regular expression, and the definitions for the VM, respectively. The
`parser.c` file contains the code to parse regular expressions and produce a
parse tree as an intermediate representation for generating VM instructions.

The `compiler.c` file contains the code (along with `sre.c`) to generate VM
instructions from the parse tree using either the Thompson or Glushkov
constructions.

Finally, the actual regular expression matcher is contained in the `srvm.c` file
which contains the source code for the actual execution of the VM with compiled
VM instructions.

## Compiling the source files

This project includes a `Makefile` in which you can compile all of the binaries
by executing:

```bash
make
```

Otherwise, you can compile each individual binary with separate `make` targets.
The `parse` binary can be compiled with `make parse`, the `compile` binary can
be compiled with `make compile`, and the `srvm` binary can be compiled using
`make srvm`. All binaries will be contained in the `bin/` directory.

## Executing the binaries

You can run the parser to print the parse tree of a given regular expression as
follows:

```bash
./bin/parse <regex>
```

To compile a VM program from a regular expression and print out the instructions
of the program, you can run:

```bash
./bin/compile <regex>
```

To run the BRU matcher, simply run:

```bash
./bin/srvm <regex> <input>
```

The program compiles the regular expression into VM instructions and tries to
match it over the given input string, and will print whether the regular
expression matched the input string, as well as the capturing information, and
maximum number of threads created during lock-step execution.
