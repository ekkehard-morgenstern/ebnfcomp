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
    char*                   exportIdent;
    char*                   nodeTypeEnum;
    int                     id;
    int                     branchesIx;
    int                     refCnt;
    bool                    branchesOutput;
    bool                    implOutput;
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
    node->token        = token;
    node->text         = text ? xstrdup(text) : 0;
    node->branches     = (struct _treenode_t**) xmalloc( sizeof(struct _treenode_t*) * 5U );
    node->branchAlloc  = 5U;
    node->numBranches  = 0U;
    node->exportIdent  = 0;
    node->nodeTypeEnum = 0;
    node->id           = -1;
    node->branchesIx   = -1;
    node->refCnt       = 1;
    node->branchesOutput = false;
    node->implOutput     = false;
    return node;
}

static void delete_node( treenode_t* node ) {
    if ( --node->refCnt > 0 ) return;
    node->branchesIx = -1;
    node->id         = -1;
    if ( node->nodeTypeEnum ) { free( node->nodeTypeEnum ); node->nodeTypeEnum = 0; }
    if ( node->exportIdent  ) { free( node->exportIdent  ); node->exportIdent  = 0; }
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

static void set_export_ident( treenode_t* node, const char* text ) {
    if ( node->exportIdent ) { free( node->exportIdent ); node->exportIdent = 0; }
    node->exportIdent = xstrdup( text );
}

static void set_node_type_enum( treenode_t* node, const char* text ) {
    if ( node->nodeTypeEnum ) { free( node->nodeTypeEnum ); node->nodeTypeEnum = 0; }
    node->nodeTypeEnum = xstrdup( text );
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

static treenode_t* tree = 0;

static treenode_t* find_literal_helper( treenode_t* query, treenode_t* node, const char* text, token_t token ) {
    if ( node == 0 || node == query ) return 0;
    if ( node->token == token && strcmp( node->text, text ) == 0 ) return node;
    for ( size_t i=0; i < node->numBranches; ++i ) {
        treenode_t* result = find_literal_helper( query, node->branches[i], text, token );
        if ( result ) return result;
    }
    return 0;
}

static treenode_t* find_literal( treenode_t* query, const char* text, token_t token ) {
    return find_literal_helper( query, tree, text, token );
}

static void deduplicate_literals( treenode_t** pBranch, treenode_t* node ) {
    if ( node == 0 ) return;
    if ( node->token == T_STR_LITERAL || node->token == T_REG_EX ) {
        treenode_t* found = find_literal( node, node->text, node->token );
        if ( found ) {
            *pBranch = found; found->refCnt++;
            delete_node( node );
            return;
        }
    }
    for ( size_t i=0; i < node->numBranches; ++i ) {
        deduplicate_literals( &node->branches[i], node->branches[i] );
    }
}

static void help( void ) {
    printf( "%s",
        "usage: ebnfcomp [options]\n"
        "options:\n"
        "    --help, -h                 (this)\n"
        "    --tree, -t                 output syntax tree\n"
        "default behavior:\n"
        "    compiles EBNF specified on standard input to internal form,\n"
        "    then outputs C code for a parsing table to standard output.\n"
    );
}

static void name_to_C_enum( char buf[256], const char* name ) {
    snprintf( buf, 256U, "NT_%s", name );
    size_t len = strlen( buf );
    for ( size_t i=0U; i < len; ++i ) {
        if ( buf[i] == '-' ) buf[i] = '_';
        if ( buf[i] >= 'a' && buf[i] <= 'z' ) buf[i] -= 'a'-'A';
    }
}

static bool is_export_node( treenode_t* node ) {
    switch ( node->token ) {
        case T_PRODUCTION:
        case T_STR_LITERAL:
        case T_REG_EX:
        case T_AND_EXPR:
        case T_OR_EXPR:
        case T_BRACK_EXPR:
        case T_BRACE_EXPR:
            return true;
        default: break;
    }
    return false;
}

static int id = 0;

static void output_enums_helper( treenode_t* node ) {
    if ( node == 0 ) return;
    if ( is_export_node( node ) && node->id == -1 ) {       
        char tmp[256]; bool print = true;
        if ( node->token == T_PRODUCTION ) {
            name_to_C_enum( tmp, node->text );
        } else if ( node->token == T_STR_LITERAL || node->token == T_REG_EX ) {
            snprintf( tmp, 256U, "NT_TERMINAL_%d", id );
        } else {
            snprintf( tmp, 256U, "%s", "_NT_GENERIC" );
            print = false;
        }
        set_node_type_enum( node, tmp );
        if ( print ) printf( "    %s,\n", tmp );
        node->id = id++;
    }
    for ( size_t i=0; i < node->numBranches; ++i ) {
        output_enums_helper( node->branches[i] );
    }
}

static void name_to_C_name( char buf[256], const char* name, const char* prefix ) {
    snprintf( buf, 256U, "%s%s", prefix, name );
    size_t len = strlen( buf );
    for ( size_t i=0U; i < len; ++i ) {
        if ( buf[i] == '-' ) buf[i] = '_';
    }
}

static void text_to_C_text( char buf[256], const char* text ) {
    const char* s = text; char* d = &buf[0]; char* e = &buf[255];
    while ( *s != '\0' ) {
        if ( *s == '\"' ) {
            if ( d < e-1 ) {
                *d++ = '\\';
                *d++ = '\"';
            }
        } else if ( *s == '\\' ) {
            if ( d < e-1 ) {
                *d++ = '\\';
                *d++ = '\\';
            }
        } else if ( d < e ) {
            *d++ = *s;
        }
        ++s;
    }
    *d = '\0';
}

static int branches_ix = 0;

static void output_decls_helper( treenode_t* node ) {
    if ( node == 0 ) return;
    if ( node->id >= 0 && node->exportIdent == 0 ) {
        const char* prefix = ""; bool numId = node->token != T_PRODUCTION;
        switch ( node->token ) {
            case T_PRODUCTION:      prefix = "production_"; break;
            case T_STR_LITERAL:     prefix = "string_terminal_"; break;
            case T_REG_EX:          prefix = "regex_terminal_"; break;
            case T_AND_EXPR:        prefix = "mandatory_expr_"; break;
            case T_OR_EXPR:         prefix = "alternative_expr_"; break;
            case T_BRACK_EXPR:      prefix = "optional_expr_"; break;
            case T_BRACE_EXPR:      prefix = "optional_repetitive_expr_"; break;
            default: break;
        }
        char nameText[256];
        if ( numId ) {
            snprintf( nameText, 256U, "%s%d", prefix, node->id );
        } else {
            name_to_C_name( nameText, node->text, prefix );
        }
        set_export_ident( node, nameText );
        if ( node->numBranches != 0U ) {
            node->branchesIx = branches_ix;
            branches_ix += node->numBranches;
        }
    }
    for ( size_t i=0; i < node->numBranches; ++i ) {
        output_decls_helper( node->branches[i] );
    }
}

static int find_prod_id( treenode_t* node, const char* name ) {
    if ( node == 0 ) return -1;
    if ( node->token == T_PRODUCTION && strcmp( node->text, name ) == 0 ) return node->id;
    for ( size_t i=0; i < node->numBranches; ++i ) {
        int id = find_prod_id( node->branches[i], name );
        if ( id >= 0 ) return id;
    }
    return -1;
}

static int output_branches_helper( treenode_t* node, int index ) {
    if ( node == 0 ) return 0;
    if ( node->id >= 0 && node->branchesIx == index ) {
        printf( "    // %d: %s branches\n    ", node->branchesIx, node->exportIdent );
        for ( size_t i=0; i < node->numBranches; ++i ) {
            treenode_t* branch = node->branches[i]; int id;
            if ( branch->id >= 0 ) {
                printf( "%d, ", branch->id );
            } else if ( branch->token == T_IDENTIFIER && ( id = find_prod_id( tree, branch->text ) ) >= 0 ) {
                printf( "%d, ", id );
            } else {
                printf( "-1 /* %s */, ", token2text(branch->token) );
            }
        }
        printf( "\n" );
        return node->numBranches;
    }
    for ( size_t i=0; i < node->numBranches; ++i ) {
        int n = output_branches_helper( node->branches[i], index );
        if ( n ) return n;
    }
    return 0;
}

static void output_branches( void ) {
    for ( int i=0; i < branches_ix; ) {
        i += output_branches_helper( tree, i );
    }
}

static bool output_impls_helper( treenode_t* node, int id ) {
    if ( node == 0 ) return false;
    if ( node->id == id ) {
        bool numId = node->token != T_PRODUCTION;
        char text[256]; const char* termType = "TT_UNDEF"; const char* nodeClass = "???";
        text[0] = '0'; text[1] = '\0';
        if ( numId ) {
            if ( node->token == T_STR_LITERAL || node->token == T_REG_EX ) {
                nodeClass = "NC_TERMINAL";
                if ( node->token == T_STR_LITERAL ) {
                    termType = "TT_STRING";
                } else { // T_REG_EX
                    termType = "TT_REGEX";
                }
            } else {
                switch ( node->token ) {
                    case T_AND_EXPR:    nodeClass = "NC_MANDATORY"; break;
                    case T_OR_EXPR:     nodeClass = "NC_ALTERNATIVE"; break;
                    case T_BRACK_EXPR:  nodeClass = "NC_OPTIONAL"; break;
                    case T_BRACE_EXPR:  nodeClass = "NC_OPTIONAL_REPETITIVE"; break;
                    default: break;
                }
            }
            if ( node->text ) {
                char tmp[256]; text_to_C_text( tmp, node->text );
                snprintf( text, 256U, "\"%s\"", tmp );
            }
        } else {
            nodeClass = "NC_PRODUCTION";
        }
        printf( 
            "    // %d: %s\n"
            "    { %s, %s, %s, %s, %d, %d },\n"
            , node->id, node->exportIdent
            , nodeClass, node->nodeTypeEnum, termType, text
            , (int) node->numBranches, node->branchesIx
        );
        return true;
    }
    for ( size_t i=0; i < node->numBranches; ++i ) {
        if ( output_impls_helper( node->branches[i], id ) ) return true;
    }
    return false;
}

static void output_impls( void ) {
    for ( int i=0; i < id; ++i ) {
        output_impls_helper( tree, i );
    }
}

static void output_code( void ) {
    printf( "%s",
        "// code auto-generated by ebnfcomp; do not modify!\n"
        "// (code might get overwritten during next ebnfcomp invocation)\n\n"
        "#include <stddef.h>\n\n"
        "typedef enum _nodeclass_t {\n"
        "    NC_TERMINAL,\n"
        "    NC_PRODUCTION,\n"
        "    NC_MANDATORY,\n"
        "    NC_ALTERNATIVE,\n"
        "    NC_OPTIONAL,\n"
        "    NC_OPTIONAL_REPETITIVE,\n"
        "} nodeclass_t;\n\n"
        "typedef enum _terminaltype_t {\n"
        "    TT_UNDEF,\n"
        "    TT_STRING,\n"
        "    TT_REGEX,\n"
        "} terminaltype_t;\n\n"
        "typedef enum _nodetype_t {\n"
        "    _NT_GENERIC,\n"
    );
    output_enums_helper( tree );
    printf( "%s",
        "} nodetype_t;\n\n"
        "typedef struct _parsingnode_t {\n"
        "    nodeclass_t        nodeClass;\n"
        "    nodetype_t         nodeType;\n"
        "    terminaltype_t     termType;\n"
        "    const char*        text;\n"
        "    size_t             numBranches;\n"
        "    int                branches;\n"
        "} parsingnode_t;\n\n"
        "// declarations (ONLY works in C!)\n"
        "#ifdef _cplusplus\n"
        "#error \"the following code will not work in C++!\"\n"
        "#endif\n\n"
    );
    output_decls_helper( tree );
    printf(
        "// branches\n\n"
        "static const int branches[%d] = {\n"
        , branches_ix
    );
    output_branches();
    printf( 
        "};\n\n"
        "static const parsingnode_t parsingTable[%d] = {\n"
        , id
    );
    output_impls();
    printf( 
        "};\n\n"
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

    tree = prodlist;
    deduplicate_literals( &tree, tree );
    output_code();

    return EXIT_SUCCESS;
}
