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

var inputFile;

function processCmdLine() {
    process.argv.forEach( ( val, index ) => {
        let valOrig = val;
        if ( index >= 2 ) {
            let opt = false;
            if ( val[0] == '-' && val[1] == '-' ) {
                val = val.substr( 2 );
                opt = true;
            } else if ( val[0] == '-' ) {
                val = val.substr( 1 );
                opt = true;
            }
            if ( opt ) {
                console.error( '? unknown option: ' + valOrig );
                process.exit( 1 );
            } else if ( inputFile === undefined ) {
                inputFile = val;
            } else {
                console.error( '? too many args: ' + val );
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
}

main();
