# ebnfcomp
EBNF Compiler

This compiler for (a variant of) Niklaus Wirth's Extended Backus-Naur Form translates an EBNF file that specifies a 
language grammar into a parsing table coded in C, and outputs it to the standard output.

To compile it, use "make" from the command line. To run it, simply type "./ebnfcomp <inputfile".

If you specify the "--tree" command line option, a syntax tree of the grammar definition will be printed instead.

