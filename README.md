# The virtual machine approach to optimised regular expression matching in C

The repo contains separate Rust source files for each stage of compiling
_regular expressions_ into _virtual machine (VM)_ instructions. The relevant
source code is contained over the Python files `parse.rs`, `sre.rs`, `srvm.rs`,
`compile.rs`, and `lib.rs`.

## Executing the Rust binaries

The `sre.rs` and `srvm.rs` files contain the necessary definitions for the parse
tree of a regular expression, and the definitions for the VM, respectively. The
`parse.rs` file contains the code to parse regular expressions and produce a
parse tree as an intermediate representation for generating VM instructions. You
can run this script to print the parse tree of a given regular expression as
follows:

```bash
cargo run --bin parser -- [OPTIONS] <regex>
```

The optional arguments can be found by running `cargo run --bin parser --
--help`.

The `compile.rs` file contains the code (along with `sre.rs`) to generate VM
instructions from the parse tree using either the Thompson or Glushkov
constructions. To see a compiled VM program of a regular expression, you can
run:

```bash
cargo run --bin compiler -- [OPTIONS] <regex>
```

The optional arguments can be found by running `cargo run --bin compiler --
--help`.

Finally, the actual regular expression matcher can be run from the `lib.rs` file
which contains the source code for the actual execution of the VM with compiled
VM instructions. To run the BRU matcher, simply run:

```bash
cargo run -- [OPTIONS] <regex> <input>
```

The optional arguments are the same as that for the `compiler` binary, and can
be found again by running `cargo run -- --help`.

The script compiles the regular expression into VM instructions and tries to
match it over the given input string, and will print whether the regular
expression matched the input string, as well as the capturing information, and
maximum number of threads created during lock-step execution.
