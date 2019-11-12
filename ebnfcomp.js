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

// modules we're dependent on
var fs = require( 'fs' );

// regular expressions
var regexWHITESPACE = /^[ \t\r\n]+/;
var regexCOMMENT    = /^--[^\n]+\n/;
var regexIdentifier = /^[a-z0-9-]+/;
var regexStrLiteral = /^('[^']+'|"[^"]+")/;
var regexReExpr     = /^\/(\\\/|[^\\/]+)+\//;

// token symbols
const T_EOF        = 0;
const T_IDENTIFIER = 1;
const T_STRLITERAL = 2;
const T_REEXPR     = 3;
const T_LPAREN     = 4;
const T_RPAREN     = 5;
const T_LBRACKET   = 6;
const T_RBRACKET   = 7;
const T_LBRACE     = 8;
const T_RBRACE     = 9;
const T_COLUMN     = 10;
const T_DOT        = 11;
const T_FAIL       = 12;

// other global variables
var inputFile;          // name of file to be read
var inputData;          // data of input file
var currentToken;       // the token we're currently at
var currentTokenText;   // the text of that token

function eatChar() {
    inputData = inputData.substr( 1 );
}

function eatMatch( match ) {
    let eaten = match[0];
    inputData = inputData.substr( match[0].length );
    return eaten;
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
        if ( ch == '|' ) { eatChar(); currentToken = T_COLUMN; return; }
        if ( ch == '.' ) { eatChar(); currentToken = T_DOT; return; }
        if ( ch == '!' ) { eatChar(); currentToken = T_FAIL; return; }

        // syntax error
        console.log( '? syntax error at "' + inputData.substr( 0, 16 ) + '"' );
        process.exit( 1 );
    }
}

function processInput() {
    inputData = fs.readFileSync( inputFile, 'utf8' );

    // read first token
    nextToken();

    // ...
}

function help() {
    console.log( 'Usage: ' + process.argv[0] + ' ' + process.argv[1] + ' [options] inputfile' );
    console.log( 'Options:' );
    console.log( '  --help, -h, -?      (this)' );
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

main();
