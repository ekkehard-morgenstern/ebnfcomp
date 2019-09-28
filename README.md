# ebnfcomp
EBNF Compiler

This compiler for (a variant of) Niklaus Wirth's Extended Backus-Naur Form translates an EBNF file that specifies a 
language grammar into a parsing table coded in C, and outputs it to the standard output.

To compile it, use "make" from the command line. To run it, simply type "./ebnfcomp <inputfile".

If you specify the "--tree" command line option, a syntax tree of the grammar definition will be printed instead.

Please note that at the moment, the EBNF compiler does not check your EBNF for validity. If you use an identifier that you haven't declared, output will contain something like:
  
> `-1 /* T_IDENTIFIER */`

in the branch table.

The regular expression recognition syntax and/or implementation seems to be partially broken at the moment. If the compiler complains about a regular expression, try reformulating it, use multiple smaller regular expressions, or formulate without them.

Both issues will be fixed in a future release.
