#!/usr/bin/tclsh

# This script will find all files that include certain regular expression.
# If the files are not in the list provided, the script will return error.

set ProgName [ lindex [ split $argv0 "/" ] end ]

if { $argc < 2 } {
    puts "Usage: $ProgName <expr1> <expr2> \[ <file> ... \]"
    puts "\texpr1 - file list expression (vtk*.h)"
    puts "\texpr2 - search string expression (vtkSet.*Macro)"
    puts "\tfile  - files that should be ignore"

    puts ""
    puts "You provided:"
    foreach { a } $argv {
	puts "$a"
    }

    exit 1
}

# Parse command line arguments
set FileExpression [ lindex $argv 0 ]
set SearchMessage  [ lindex $argv 1 ]
set IgnoreFileList [ lrange $argv 2 end ]

#puts "Searching for $SearchMessage in $FileExpression"
#puts "Ignore list: $IgnoreFileList"

# Find regular expression in the string
proc FindString { InFile SearchString } {
    if [ catch { open $InFile r } inchan ] {
	puts stderr "Cannot open $InFile"
	return 0
    }
    set res 0
    set lcount 1
    while { ! [eof $inchan] } {
	gets $inchan line
	if [ regexp $SearchString $line matches ] {
	    puts "$InFile: Found $SearchString on line $lcount"
	    puts "$line"
	    set res 1
	    break
	}
	set lcount [ expr $lcount + 1 ]
    }
    close $inchan
    return $res
}

# Get all files that match expression
set files [ glob $FileExpression ]

set count 0
foreach { a } $files {
    if { [ lsearch $IgnoreFileList $a ] >= 0 } {
	puts "Ignoring: $a"
    } else {
	set count [ expr $count + [ FindString $a $SearchMessage ] ]
    }
}

if { $count > 0 } {
    puts "" 
    puts "Found \"$SearchMessage\" $count times"
    exit 1
}