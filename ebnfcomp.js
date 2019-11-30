#!/usr/bin/js

/*
    EBNF Compiler and Parser Generator
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

// TBD

// -- Dependencies ----------------------------------------------------------------------------------

// modules we're dependent on
var fs = require( 'fs' );

// -- Regular Expressions ---------------------------------------------------------------------------

// regular expressions
var regexWHITESPACE = /^[ \t\r\n]+/;
var regexCOMMENT    = /^--[^\n]+\n/;
var regexIdentifier = /^[a-z0-9-]+/;
var regexStrLiteral = /^('[^']+'|"[^"]+")/;
var regexReExpr     = /^\/(\\.|[^\\\/]+)+\//;
var regexKeyword    = /^[A-Z-]+/;

// -- Constants -------------------------------------------------------------------------------------

// token symbols
const T_EOF           = 0;
const T_IDENTIFIER    = 1;
const T_STRLITERAL    = 2;
const T_REEXPR        = 3;
const T_LPAREN        = 4;
const T_RPAREN        = 5;
const T_LBRACKET      = 6;
const T_RBRACKET      = 7;
const T_LBRACE        = 8;
const T_RBRACE        = 9;
const T_ALTERNATIVE   = 10;
const T_DOT           = 11;
const T_FAIL_IF_UNMATCHED = 12;
const T_WHITESPACE    = 13;
const T_COMMENT       = 14;
const T_TOKEN_ELEMENT = 15;
const T_NAMED_TOKEN   = 16;
const T_TOKEN         = 17;
const T_ROOT          = 18;
const T_SEQUENCE      = 19;
const T_ASSIGN        = 20;
const T_REGULAR       = 21;
const T_PROD_LIST     = 22;
const T_PLUS          = 23;

// -- Global Variables ------------------------------------------------------------------------------

// other global variables
var inputFile;          // name of file to be read
var inputData;          // data of input file
var currentToken;       // the token we're currently at
var currentTokenText;   // the text of that token
var currentLine;        // current line number
var prodList;           // production list (syntax analysis result)
var treeOnly = false;   // just output syntax tree

// -- Classes ---------------------------------------------------------------------------------------

// classes

class TreeNode {

    constructor( nodeType, nodeText = null ) {
        this.nodeType = nodeType; 
        this.nodeText = nodeText;
        this.branches = new Array();
    }

    get nodeTypeAsInt() {
        return this.nodeType;
    }

    get nodeTypeAsString() {
        let s = '';
        switch ( this.nodeType ) {
            case T_EOF           : s = 'T_EOF'          ; break;
            case T_IDENTIFIER    : s = 'T_IDENTIFIER'   ; break;
            case T_STRLITERAL    : s = 'T_STRLITERAL'   ; break;
            case T_REEXPR        : s = 'T_REEXPR'       ; break;
            case T_LPAREN        : s = 'T_LPAREN'       ; break;
            case T_RPAREN        : s = 'T_RPAREN'       ; break;
            case T_LBRACKET      : s = 'T_LBRACKET'     ; break;
            case T_RBRACKET      : s = 'T_RBRACKET'     ; break;
            case T_LBRACE        : s = 'T_LBRACE'       ; break;
            case T_RBRACE        : s = 'T_RBRACE'       ; break;
            case T_ALTERNATIVE   : s = 'T_ALTERNATIVE'  ; break;
            case T_DOT           : s = 'T_DOT'          ; break;
            case T_FAIL_IF_UNMATCHED: s = 'T_FAIL_IF_UNMATCHED'; break;
            case T_WHITESPACE    : s = 'T_WHITESPACE'   ; break;
            case T_COMMENT       : s = 'T_COMMENT'      ; break;
            case T_TOKEN_ELEMENT : s = 'T_TOKEN_ELEMENT'; break;
            case T_NAMED_TOKEN   : s = 'T_NAMED_TOKEN'  ; break;
            case T_TOKEN         : s = 'T_TOKEN'        ; break;
            case T_ROOT          : s = 'T_ROOT'         ; break;
            case T_SEQUENCE      : s = 'T_SEQUENCE'     ; break;
            case T_ASSIGN        : s = 'T_ASSIGN'       ; break;
            case T_REGULAR       : s = 'T_REGULAR'      ; break;
            case T_PROD_LIST     : s = 'T_PROD_LIST'    ; break;
            case T_PLUS          : s = 'T_PLUS'         ; break;
        }
        return s;
    }

    get numBranches() {
        return this.branches.length;
    }

    addBranch( branch ) {
        this.branches.push( branch );
    }

    recurseWith( fn, depth = 0, before = false ) {
        let cont = true;
        if ( before ) {
            this.branches.forEach( element => {
                cont = element.recurseWith( fn, depth+1, before );
                if ( !cont ) return false;
            })    
        }
        cont = fn( this, depth );
        if ( !cont ) return false;
        if ( !before ) {
            this.branches.forEach( element => {
                cont = element.recurseWith( fn, depth+1, before );
                if ( !cont ) return false;
            })    
        }
        return cont;
    }

    print( indent = 0 ) {
        this.recurseWith( ( node, depth ) => {

            let nSpace = indent + depth * 2;
            let sSpace = ''.padEnd( nSpace, ' ' );
            let sArg   = node.nodeText !== null ? ' "' + node.nodeText + '"' : '';

            console.log( sSpace + node.nodeTypeAsString + sArg );

            return true;
        });
    }

}

// -- Functions -------------------------------------------------------------------------------------

// main functions

function eatChar() {
    inputData = inputData.substr( 1 );
}

function eatMatch( match ) {
    let eaten = match[0];
    for ( let pos=0; pos >= 0; ) {
        pos = eaten.indexOf( "\n", pos );
        if ( pos >= 0 ) { ++pos; ++currentLine; }
    }
    inputData = inputData.substr( match[0].length );
    return eaten;
}

function syntaxError() {
    console.log( '? syntax error in line ' + currentLine + ' at "' + 
        inputData.substr( 0, 16 ) + '"' );
    process.exit( 1 );
}

function nextToken() {
    currentTokenText = undefined;
    for (;;) {
        
        // at end of file
        if ( inputData.length == 0 ) { currentToken = T_EOF; return; }
        
        // eat whitespace
        let match = regexWHITESPACE.exec( inputData );
        if ( match !== null ) { eatMatch( match ); continue; }
        
        // eat comment
        match = regexCOMMENT.exec( inputData );
        if ( match !== null ) { eatMatch( match ); continue; }

        // eat identifier
        match = regexIdentifier.exec( inputData );
        if ( match !== null ) { 
            currentTokenText = eatMatch( match );
            currentToken     = T_IDENTIFIER;
            return;
        }

        // eat keyword
        match = regexKeyword.exec( inputData );
        if ( match !== null ) {
            let keyword = match[0];
            let newtok  = -1;
            switch ( keyword ) {
                case 'WHITESPACE'   : newtok = T_WHITESPACE   ; break;
                case 'COMMENT'      : newtok = T_COMMENT      ; break;
                case 'TOKEN-ELEMENT': newtok = T_TOKEN_ELEMENT; break;
                case 'NAMED-TOKEN'  : newtok = T_NAMED_TOKEN  ; break;
                case 'TOKEN'        : newtok = T_TOKEN        ; break;
                case 'ROOT'         : newtok = T_ROOT         ; break;
            }
            if ( newtok >= 0 ) { eatMatch( match ); currentToken = newtok; return; }
        }

        // eat string literal
        match = regexStrLiteral.exec( inputData );
        if ( match !== null ) { 
            currentTokenText = eatMatch( match );
            currentToken     = T_STRLITERAL;
            return;
        }

        // eat regular expression definition
        match = regexReExpr.exec( inputData );
        if ( match !== null ) { 
            currentTokenText = eatMatch( match );
            currentToken     = T_REEXPR;
            return;
        }

        // examine the current character
        let ch = inputData[0];
        if ( ch == '(' ) { eatChar(); currentToken = T_LPAREN; return; }
        if ( ch == ')' ) { eatChar(); currentToken = T_RPAREN; return; }
        if ( ch == '[' ) { eatChar(); currentToken = T_LBRACKET; return; }
        if ( ch == ']' ) { eatChar(); currentToken = T_RBRACKET; return; }
        if ( ch == '{' ) { eatChar(); currentToken = T_LBRACE; return; }
        if ( ch == '}' ) { eatChar(); currentToken = T_RBRACE; return; }
        if ( ch == '|' ) { eatChar(); currentToken = T_ALTERNATIVE; return; }
        if ( ch == '.' ) { eatChar(); currentToken = T_DOT; return; }
        if ( ch == '!' ) { eatChar(); currentToken = T_FAIL_IF_UNMATCHED; return; }
        if ( ch == '+' ) { eatChar(); currentToken = T_PLUS; return; }

        // special cases
        if ( inputData.startsWith( ':=' ) ) {
            eatChar(); eatChar(); currentToken = T_ASSIGN;
            return;
        }

        // syntax error
        syntaxError();
    }
}

function eatBaseExpr() {
    // base-expr := identifier | str-literal | re-expr | '(' expr ')' [ '+' ]
    //              | '[' expr ']' | '{' expr '}' .
    let node = null; let introducer = null; let expr = null;
    switch ( currentToken ) {
        case T_IDENTIFIER: case T_STRLITERAL: case T_REEXPR:
            node = new TreeNode( currentToken, currentTokenText );
            nextToken();
            break;
        case T_LPAREN: case T_LBRACKET: case T_LBRACE:
            introducer = currentToken;
            nextToken();
            expr = eatExpr();
            if ( expr === null ) syntaxError();
            if ( currentToken != introducer + 1 ) syntaxError();
            nextToken();
            node = new TreeNode( introducer );
            node.addBranch( expr );
            if ( introducer == T_LPAREN && currentToken == T_PLUS ) {
                nextToken();
                node.addBranch( new TreeNode( T_PLUS ) );
            }
            break;
    }
    return node;
}

function eatFailExpr() {
    // fail-expr := base-expr [ '!' ] .
    let expr = eatBaseExpr();
    if ( expr === null ) return null;
    if ( currentToken == T_FAIL_IF_UNMATCHED ) {
        nextToken();
        let fail = new TreeNode( T_FAIL_IF_UNMATCHED );
        fail.addBranch( expr );
        return fail;
    }
    return expr;
}

function eatAndExpr() {
    // and-expr := ( fail-expr )+ .
    let node = null;
    for (;;) {
        let expr = eatFailExpr();
        if ( expr === null ) {
            if ( node === null ) return null;
            if ( node.numBranches == 1 ) return node.branches[0];
            return node;
        }
        if ( node === null ) node = new TreeNode( T_SEQUENCE );
        node.addBranch( expr );
    }
}

function eatOrExpr() {
    // or-expr := and-expr { '|' and-expr } .
    let node = null;
    for (;;) {
        if ( node !== null ) {
            if ( currentToken != T_ALTERNATIVE ) {
                if ( node.numBranches == 1 ) return node.branches[0];
                return node;
            }
            nextToken();
        }
        let expr = eatAndExpr();
        if ( expr === null ) {
            if ( node === null ) return null;
            syntaxError();
        }
        if ( node === null ) node = new TreeNode( T_ALTERNATIVE );
        node.addBranch( expr );
    }
}

function eatExpr() {
    // expr := or-expr .
    return eatOrExpr();
}

function eatProdSpecifier() {
    // prod-specifier := ':=' expr '.' .
    if ( currentToken != T_ASSIGN ) return null;
    nextToken();
    let expr = eatExpr();
    if ( expr === null ) syntaxError();
    if ( currentToken != T_DOT ) syntaxError();
    nextToken();
    return expr;
}

function eatWhitespaceProd() {
    // whitespace-prod  := ( "WHITESPACE" | "COMMENT" ) prod-specifier! .
    if ( currentToken != T_WHITESPACE && currentToken != T_COMMENT ) {
        return null;
    }
    let introducer = currentToken;
    nextToken();
    let specifier = eatProdSpecifier();
    if ( specifier === null ) syntaxError();
    let node = new TreeNode( introducer );
    node.addBranch( specifier );
    return node;
}

function eatProdQualifier() {
    // prod-qualifier := "TOKEN-ELEMENT" | "NAMED-TOKEN" | "TOKEN" | "ROOT" .
    switch ( currentToken ) {
        case T_TOKEN_ELEMENT: case T_NAMED_TOKEN: case T_TOKEN: case T_ROOT:
            let node = new TreeNode( currentToken );
            nextToken();
            return node;
    }
    return null;
}

function eatQualifiedProd() {
    // qualified-prod := prod-qualifier ( identifier prod-specifier )! .
    let qualifier = eatProdQualifier();
    if ( qualifier === null ) return null;
    if ( currentToken != T_IDENTIFIER ) syntaxError();
    let identifier = currentTokenText;
    nextToken();
    let specifier = eatProdSpecifier();
    if ( specifier === null ) syntaxError();
    let node = qualifier;
    node.nodeText = identifier;
    node.addBranch( specifier );
    return node;
}

function eatRegularProd() {
    // regular-prod := identifier prod-specifier! .
    if ( currentToken != T_IDENTIFIER ) return null;
    let identifier = currentTokenText;
    nextToken();
    let specifier = eatProdSpecifier();
    if ( specifier === null ) syntaxError();
    let node = new TreeNode( T_REGULAR, identifier );
    node.addBranch( specifier );
    return node;
}

function eatProduction() {
    // production := whitespace-prod | qualified-prod | regular-prod .
    let prod = eatWhitespaceProd(); if ( prod ) return prod;
    prod = eatQualifiedProd();      if ( prod ) return prod;
    return eatRegularProd();
}

function eatProdList() {
    // ROOT prod-list := ( production )+ .
    let node = null;
    for (;;) {
        let expr = eatProduction();
        if ( expr === null ) {
            if ( node === null ) return null;
            if ( node.numBranches == 1 ) return node.branches[0];
            return node;
        }
        if ( node === null ) node = new TreeNode( T_PROD_LIST );
        node.addBranch( expr );
    }
}

function processInput() {
    inputData   = fs.readFileSync( inputFile, 'utf8' );
    currentLine = 1;

    // read first token
    nextToken();

    // read production list
    prodList = eatProdList();
    if ( currentToken != T_EOF ) syntaxError();

    if ( prodList === null ) {
        console.log( "? input empty, nothing to do" );
        return;
    }

    if ( treeOnly ) {
        prodList.print();
        process.exit( 0 );
    }

    

}

function help() {
    console.log( 'Usage: ' + process.argv[0] + ' ' + process.argv[1] + ' [options] inputfile' );
    console.log( 'Options:' );
    console.log( '  --help, -h, -?      (this)' );
    console.log( '  --tree, -t          output syntax tree only' );
}

function processCmdLine() {

    process.argv.forEach( ( value, index ) => {        
    
        let originalValue = value;
    
        if ( index >= 2 ) { // parameters start at index 2
    
            let option = false;

            if ( value[0] == '-' && value[1] == '-' ) { // double-dash option
                value = value.substr( 2 );
                option = true;
            } else if ( value[0] == '-' ) { // single-dash option
                value = value.substr( 1 );
                option = true;
            }

            if ( option ) { // command-line option
                if ( value == 'help' || value == 'h' || value == '?' ) {
                    help();
                    process.exit( 1 );                   
                } else if ( value == 'tree' || value == 't' ) {
                    treeOnly = true;
                } else {
                    console.error( '? unknown option: ' + originalValue );
                    process.exit( 1 );                                  
                }
            } else if ( inputFile === undefined ) { // first non-option argument: input file
                inputFile = value;
            } else {    // too many arguments
                console.error( '? too many args: ' + value );
                process.exit( 1 );
            }

        }
    });
}

function main() {

    processCmdLine();
    if ( inputFile === undefined ) {
        console.error( '? missing input file' );
        process.exit( 1 );
    }

    console.log( 'input file = ' + inputFile );

    processInput();
}

// -- Main Program ----------------------------------------------------------------------------------

main();

