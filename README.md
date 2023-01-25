# ebnfcomp

## EBNF Compiler

This compiler for (a variant of) Niklaus Wirth's Extended Backus-Naur Form translates an EBNF file that specifies a
language grammar into a parsing table coded in C or assembly language, and outputs it to the specified files (split into header and implementation).

To compile it, use "make" from the command line. To run it, simply type "./ebnfcomp filestem &lt;inputfile", where "filestem" is the base name of the output files to be generated.

If you specify the "--tree" command line option, a syntax tree of the grammar definition will be printed instead.

If you specify the "--asm" command line option, code for NASM will be generated instead of C code.

## Release Notes

### Enhancements

In release 1.2.1, "speaking" names are attempted to be generated for terminals.
In release 1.3, the EBNF compiler supports assembly language output for NASM.

### Bugfixes

Please note that in release 1.0 and 1.1, the EBNF compiler didn't check your EBNF for validity. If you use an identifier that you haven't declared, output will contain something like:

> `-1 /* T_IDENTIFIER */`

in the branch table. This issue has been fixed in release 1.2.

The following code has been output in releases 1.0 and 1.1, but has been removed in 1.2, for it no longer makes sense. The code was necessary in earlier, unreleased versions of the compiler.

> `// declarations (ONLY works in C!)`
> `#ifdef _cplusplus`
> `#error "the following code will not work in C++!"`
> `#endif`

### Bugs

The regular expression recognition syntax and/or implementation seems to be partially broken at the moment. If the compiler complains about a regular expression, try reformulating it, use multiple smaller regular expressions, or formulate without them.

This issue will be fixed in a future release.
