# ebnfcomp

EBNF Compiler and Parser Generator

This compiler for (a variant of) Niklaus Wirth's Extended Backus-Naur Form translates an EBNF file that specifies a language grammar into a parser written in one of several languages (currently planned are C and/or C++ and JavaScript).

To run it, simply type "./ebnfcomp inputfile".

If you specify the "--tree" command line option, a syntax tree of the grammar definition will be printed instead.
