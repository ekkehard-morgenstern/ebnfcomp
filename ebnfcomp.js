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

var inputFile;  // name of file to be read

function processInput() {


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
