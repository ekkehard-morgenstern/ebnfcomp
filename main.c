/*
    EBNF Compiler
    Copyright (C) 2019  Ekkehard Morgenstern

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    Contact Info:
    E-Mail: ekkehard@ekkehardmorgenstern.de 
    Mail: Ekkehard Morgenstern, Mozartstr. 1, 76744 Woerth am Rhein, Germany, Europe
*/

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/*
language syntax:

identifier  := /[a-z0-9-]+/ .
str-literal := /'[^']+'/ | /"[^"]+"/ .

-- during parsing of regular expressions, whitespace skipping will be disabled
re-any      := '.' .
re-chr      := '\' /./ | /[^\/.*?[(|]/ .

re-cc-chr   := '\' /./ | /[^\\\]]/ .
re-cc-rng   := re-cc-chr '-' re-cc-chr .
re-cc-item  := re-cc-rng | re-cc-chr .
re-cc-items := re-cc-item { re-cc-item } .
re-cc       := '[' [ '^' ] re-cc-items ']' .

re-base-expr   := re-cc | re-chr | re-any | '(' re-expr ')' .
re-repeat-expr := re-base-expr [ '+' | '*' | '?' ] .
re-and-expr    := re-repeat-expr { re-repeat-expr } .
re-or-expr     := re-and-expr { '|' re-and-expr } .
re-expr        := re-or-expr .
regex          := '/' re-expr '/' .

base-expr   := identifier | str-literal | regex | '(' expr ')' | '[' expr ']' | '{' expr '}' .
and-expr    := base-expr { base-expr } .
or-expr     := and-expr { '|' and-expr } .
expr        := or-expr .

production  := identifier ':=' expr '.' .
prod-list   := production { production } .

*/

typedef enum _token_t {
    T_EOS,
    T_IDENTIFIER,
    T_STR_LITERAL,
    T_REG_EX,
    T_BRACK_EXPR,
    T_BRACE_EXPR,
    T_AND_EXPR,
    T_OR_EXPR,
    T_EXPR,
    T_PRODUCTION,
    T_PROD_LIST
} token_t;

static const char* token2text( token_t token ) {
    switch ( token ) {
        default:            return "?";
        case T_EOS:         return "T_EOS";
        case T_IDENTIFIER:  return "T_IDENTIFIER";
        case T_STR_LITERAL: return "T_STR_LITERAL";
        case T_REG_EX:      return "T_REG_EX";
        case T_BRACK_EXPR:  return "T_BRACK_EXPR";
        case T_BRACE_EXPR:  return "T_BRACE_EXPR";
        case T_AND_EXPR:    return "T_AND_EXPR";
        case T_OR_EXPR:     return "T_OR_EXPR";
        case T_EXPR:        return "T_EXPR";
        case T_PRODUCTION:  return "T_PRODUCTION";
        case T_PROD_LIST:   return "T_PROD_LIST";
    }
}

typedef struct _treenode_t {
    token_t                 token;
    char*                   text;
    struct _treenode_t**    branches;
    size_t                  branchAlloc;
    size_t                  numBranches;
} treenode_t;

static void* xmalloc( size_t size ) {
    size_t reqSize = size ? size : 1U;
    void* blk = malloc( reqSize );
    if ( blk == 0 ) {
        fprintf( stderr, "? out of memory\n" );
        exit( EXIT_FAILURE );
    }
    return blk;
}

static void xrealloc( void** pBlk, size_t newSize ) {
    size_t reqSize = newSize ? newSize : 0U;
    if ( *pBlk == 0 ) {
        *pBlk = xmalloc( reqSize );
    } else {
        void* newBlk = realloc( *pBlk, reqSize );
        if ( newBlk == 0 ) {
            fprintf( stderr, "? out of memory\n" );
            exit( EXIT_FAILURE );
        }
        *pBlk = newBlk;
    }
}

static char* xstrdup( const char* text ) {
    size_t len = strlen( text );
    char* blk = (char*) xmalloc( len + 1U );
    strcpy( blk, text );
    return blk;
}

static void dump_tree_node( treenode_t* node, int indent ) {
    if ( node == 0 ) return;
    if ( node->text == 0 ) {
        printf( "%-*.*s%s\n", indent, indent, "", token2text(node->token) );
    } else {
        printf( "%-*.*s%s '%s'\n", indent, indent, "", token2text(node->token), node->text );
    }
    for ( size_t i=0; i < node->numBranches; ++i ) {
        dump_tree_node( node->branches[i], indent+2 );
    }
}

static treenode_t* create_node( token_t token, const char* text ) {
    treenode_t* node = (treenode_t*) xmalloc( sizeof(treenode_t) );
    node->token    = token;
    node->text     = text ? xstrdup(text) : 0;
    node->branches = (struct _treenode_t**) xmalloc( sizeof(struct _treenode_t*) * 5U );
    node->branchAlloc = 5U;
    node->numBranches = 0U;
    return node;
}

static void delete_node( treenode_t* node ) {
    while ( node->numBranches > 0U ) {
        treenode_t* branch = node->branches[--node->numBranches];
        if ( branch ) delete_node( branch );
    }
    free( (void*)(node->branches) ); node->branches = 0;
    node->branchAlloc = 0U;
    if ( node->text ) { free( node->text ); node->text = 0; }
    node->token = T_EOS;
    free( node );
}

static void add_branch( treenode_t* node, treenode_t* branch ) {
    if ( node->numBranches >= node->branchAlloc ) {
        size_t newSize = node->branchAlloc * 2U;
        xrealloc( (void**)(&node->branches), sizeof(struct _treenode_t*) * newSize );
        node->branchAlloc = newSize;
    }
    node->branches[ node->numBranches++ ] = branch;
}

static int ch  = EOF;
static int lno = 0;
static int chx = 0;

static char rngbuf[64];
static int  wpos = 0;
static int  rpos = 0;

static char regex[256];
static int  repos = 0;

static void storech() {
    rngbuf[wpos] = (char) ch;
    wpos = ( wpos + 1 ) & 63;
}

static void printrng() {
    while ( rpos != wpos ) {
        int c = (int) rngbuf[rpos];
        fputc( c, stderr );
        rpos = ( rpos + 1 ) & 63;
    }
    fputc( '\n', stderr );
}

static void rdch( void ) {
RETRY:
    ch = fgetc( stdin );
REEVALUATE:
    if ( ch == EOF ) return;
    if ( lno == 0 ) { ++lno; chx = 0; }
    if ( ch == '\r' ) goto RETRY;
    if ( ch == '\n' ) { ++lno; chx = 0; goto RETRY; }
    if ( ch == '-'  ) {
        ch = fgetc( stdin );
        if ( ch != '-' ) {
            ungetc( ch, stdin );
            ch = '-';
        } else {
            // -- comment
            do { ch = fgetc( stdin ); } while ( ch != '\n' && ch != EOF );
            goto REEVALUATE;
        }
    }
    ++chx;
    storech();
}

static void report( const char* text ) {
    fprintf( stderr, "? %s in line %d near position %d\n", text, lno, chx );
    printrng();
    exit( EXIT_FAILURE );
}

static void skip_whitespace( void ) {
    while ( ch == ' ' || ch == '\t' ) rdch();
}

static treenode_t* read_identifier( void ) {
    // identifier := /[a-z0-9-]+/ .
    char tmp[256];
    int  ix = 0;
    do {
        if ( ix < 255 ) tmp[ix++] = (char) ch;
        rdch();
    } while ( ( ch >= '0' && ch <= '9' ) || ( ch >= 'a' && ch <= 'z' ) || ch == '-' );
    tmp[ix] = '\0';
    return create_node( T_IDENTIFIER, tmp );
}

static treenode_t* read_str_literal( void ) {
    // str-literal := /'[^']+'/ | /"[^"]+"/ .
    char tmp[256];
    int  ix = 0;
    int  term = ch;
    do {
        if ( ch != term && ch != EOF && ix < 255 ) tmp[ix++] = (char) ch;
        rdch();
    } while ( ch != term && ch != EOF );
    rdch();
    if ( ix == 0 ) report( "string literal is empty" );
    tmp[ix] = '\0';
    return create_node( T_STR_LITERAL, tmp );
}

static void store_regex_char( char c ) {
    if ( repos < 255 ) regex[repos++] = (char) c;
}

static bool read_re_any( void ) {
    // re-any := '.' .
    if ( ch != '.' ) return false;
    store_regex_char( '.' );
    rdch();
    return true;
}

static bool read_re_chr( void ) {
    // re-chr := '\' /./ | /[^\/.*?[(|]/ .
    if ( ch == '\\' ) {
        rdch();
        if ( ch == EOF ) report( "unexpected end of file" );
        store_regex_char( '\\' );
    } else {
        switch ( ch ) {
            case EOF:
                report( "unexpected end of file" );
            case '/': case '.': case '*': case '?': case '[': case '(': case '|':
                return false;
            default: break;
        }
    }
    store_regex_char( (char) ch );
    rdch();
    return true;
}

static bool read_re_cc_chr( void ) {
    // re-cc-chr := '\' /./ | /[^\\\]]/ .
    if ( ch == '\\' ) {
        rdch();
        if ( ch == EOF ) report( "unexpected end of file" );
        store_regex_char( '\\' );
    } else {
        switch ( ch ) {
            case EOF:
                report( "unexpected end of file" );
            case '\\': case ']':
                return false;
            default: break;
        }
    }
    store_regex_char( (char) ch );
    rdch();
    return true;
}

static bool read_re_cc_item( void ) {
    // re-cc-rng  := re-cc-chr '-' re-cc-chr .
    // re-cc-item := re-cc-rng | re-cc-chr .
    // -or-
    // re-cc-item := re-cc-chr [ '-' re-cc-chr ] .
    if ( !read_re_cc_chr() ) return false;;
    if ( ch == '-' ) {
        store_regex_char( '-' );
        rdch();
        if ( !read_re_cc_chr() ) report( "bad character class in regular expression" );
    }
    return true;
}

static bool read_re_cc_items( void ) {
    // re-cc-items := re-cc-item { re-cc-item } .
    if ( !read_re_cc_item() ) return false;
    while ( read_re_cc_item() );
    return true;
}

static bool read_re_cc( void ) {
    // re-cc := '[' [ '^' ] re-cc-items ']' .
    if ( ch != '[' ) return false;
    store_regex_char( '[' );
    rdch();
    if ( ch == '^' ) {
        store_regex_char( '^' );
        rdch();
    }
    if ( !read_re_cc_items() || ch != ']' ) report( "bad character class in regular expression" );
    store_regex_char( ']' );
    rdch();
    return true;
}

static bool read_re_expr( void );

static bool read_re_base_expr( void ) {
    // re-base-expr := re-cc | re-chr | re-any | '(' re-expr ')' .
    if ( read_re_cc() || read_re_chr() || read_re_any() ) return true;
    if ( ch != '(' ) return false;
    store_regex_char( '(' );
    rdch();
    if ( !read_re_expr() || ch != ')' ) report( "expression expected in regular expression" );
    store_regex_char( ')' );
    rdch();
    return true;
}

static bool read_re_repeat_expr( void ) {
    // re-repeat-expr := re-base-expr [ '+' | '*' | '?' ] .
    if ( !read_re_base_expr() ) return false;
    if ( ch == '+' || ch == '*' || ch == '?' ) {
        store_regex_char( (char) ch );
        rdch();
    }
    return true;
}

static bool read_re_and_expr( void ) {
    // re-and-expr := re-repeat-expr { re-repeat-expr } .
    if ( !read_re_repeat_expr() ) return false;
    while ( read_re_repeat_expr() );
    return true;
}

static bool read_re_or_expr( void ) {
    // re-or-expr := re-and-expr { '|' re-and-expr } .
    if ( !read_re_and_expr() ) return false;
    do {
        if ( ch != '|' ) break;
        store_regex_char( '|' );
        rdch();
        if ( !read_re_and_expr() ) report( "expression expected in regular expression" );
    } while ( true );
    return true;    
}

static bool read_re_expr( void ) {
    // re-expr := re-or-expr .
    return read_re_or_expr();
}

static treenode_t* read_regex( void ) {
    // regex := '/' re-expr '/' .
    if ( ch != '/' ) return false;
    rdch();
    repos = 0;
    if ( !read_re_expr() ) report( "regular expression expected" );
    if ( ch != '/' ) report( "delimiter '/' expected after regular expression" );
    rdch();
    regex[repos] = '\0';
    return create_node( T_REG_EX, regex );
}

static treenode_t* read_expr( void );

static treenode_t* read_paren_expr( void ) {
    // '(' expr ')' 
    rdch();
    treenode_t* expr = read_expr();
    if ( expr == 0 ) report( "expression expected after '('" );
    if ( ch != ')' ) report( "closing parenthesis ')' expected" );
    rdch();
    return expr;
}

static treenode_t* read_brack_expr( void ) {
    // '[' expr ']' 
    rdch();
    treenode_t* expr = read_expr();
    if ( expr == 0 ) report( "expression expected after '['" );
    if ( ch != ']' ) report( "closing bracket ']' expected" );
    rdch();
    treenode_t* node = create_node( T_BRACK_EXPR, 0 );
    add_branch( node, expr );
    return node;
}

static treenode_t* read_brace_expr( void ) {
    // '{' expr '}' 
    rdch();
    treenode_t* expr = read_expr();
    if ( expr == 0 ) report( "expression expected after '{'" );
    if ( ch != '}' ) report( "closing brace '}' expected" );
    rdch();
    treenode_t* node = create_node( T_BRACE_EXPR, 0 );
    add_branch( node, expr );
    return node;
}

static treenode_t* read_base_expr( void ) {
    // base-expr := identifier | str-literal | regex | '(' expr ')' | '[' expr ']' | '{' expr '}' .
    skip_whitespace();
    switch ( ch ) {
        case '\'': case '"':    return read_str_literal();
        case '/':               return read_regex();
        case '(':               return read_paren_expr();
        case '[':               return read_brack_expr();
        case '{':               return read_brace_expr();
        default:
            if ( ( ch >= 'a' && ch <= 'z' ) || ( ch >= '0' && ch <= '9' ) ) {
                return read_identifier();
            }
            break;
    }
    return 0;
}

static treenode_t* read_and_expr( void ) {
    // and-expr := base-expr { base-expr } .
    treenode_t* expr = read_base_expr();
    if ( expr == 0 ) return 0;
    treenode_t* node = create_node( T_AND_EXPR, 0 );
    for (;;) {
        add_branch( node, expr );
        expr = read_base_expr();
        if ( expr == 0 ) break;
    }
    if ( node->numBranches == 1 ) {
        expr = node->branches[0]; node->branches[0] = 0;
        delete_node( node );
        return expr;
    }
    return node;
}

static treenode_t* read_or_expr( void ) {
    // or-expr := and-expr { '|' and-expr } .
    treenode_t* expr = read_and_expr();
    if ( expr == 0 ) return 0;
    treenode_t* node = create_node( T_OR_EXPR, 0 );
    for (;;) {
        add_branch( node, expr );
        skip_whitespace();
        if ( ch != '|' ) break;
        rdch();
        expr = read_and_expr();
        if ( expr == 0 ) report( "expression expected after '|'" );
    }
    if ( node->numBranches == 1 ) {
        expr = node->branches[0]; node->branches[0] = 0;
        delete_node( node );
        return expr;
    }
    return node;
}

static treenode_t* read_expr( void ) {
    // expr := or-expr .
    return read_or_expr();
}


static treenode_t* read_production( void ) {
    // production  := identifier ':=' expr '.' .
    skip_whitespace();
    treenode_t* ident;
    if ( ( ch >= '0' && ch <= '9' ) || ( ch >= 'a' && ch <= 'z' ) ) {
        ident = read_identifier();
    } else {
        return 0;
    }
    skip_whitespace();
    if ( ch != ':' ) report( "':' expected" );
    rdch();
    if ( ch != '=' ) report( "'=' expected" );
    rdch();
    treenode_t* expr = read_expr();
    if ( expr == 0 ) report( "expression expected in production" );
    skip_whitespace();
    if ( ch != '.' ) report( "'.' expected" );
    rdch();
    treenode_t* node = create_node( T_PRODUCTION, ident->text );
    delete_node( ident );
    add_branch( node, expr );
    return node;
}

static treenode_t* read_prod_list( void ) {
    // prod-list := production { production } .
    treenode_t* prod = read_production();
    if ( prod == 0 ) return 0;
    treenode_t* node = create_node( T_PROD_LIST, 0 );
    do {
        add_branch( node, prod );
        prod = read_production();
    } while ( prod );
    return node;
}

static void help( void ) {
    printf( "%s",
        "usage: ebnfcomp [options]\n"
        "options:\n"
        "    --help, -h                 (this)\n"
        "    --tree, -t                 output syntax tree\n"
        "default behavior:\n"
        "    compiles EBNF specified on standard input to internal form,\n"
        "    then outputs C code for table-directed parsing to standard \n"
        "    output.\n"
    );
}

int main( int argc, char** argv ) {

    bool printTree = false;

    for ( int i=1; i < argc; ++i ) {
        const char* arg = argv[i];
        if ( strcmp( arg, "--help" ) == 0 || strcmp( arg, "-h" ) == 0 ) {
            help();
            return EXIT_SUCCESS;
        }
        if ( strcmp( arg, "--tree" ) == 0 || strcmp( arg, "-t" ) == 0 ) {
            printTree = true;
        }
    }

    rdch();
    treenode_t* prodlist = read_prod_list();
    if ( prodlist == 0 ) report( "production list expected" );

    if ( printTree ) { dump_tree_node( prodlist, 0 ); return EXIT_SUCCESS; }

    return EXIT_SUCCESS;
}
