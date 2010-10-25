<?php
/*
    BStruct generator
    Copyright (C) Ambroz Bizjak, 2010

    This file is part of BadVPN.

    BadVPN is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    BadVPN is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

require_once "lime/parse_engine.php";
require_once "StructParser.php";
require_once "bstruct_functions.php";

function assert_failure ($script, $line, $message)
{
    if ($message == "") {
        fatal_error("Assertion failure at {$script}:{$line}");
    } else {
        fatal_error("Assertion failure at {$script}:{$line}: {$message}");
    }
}

assert_options(ASSERT_CALLBACK, "assert_failure");

function print_help ($name)
{
    echo <<<EOD
Usage: {$name}
    --input-file <file>         Message file to generate source for.
    --output-dir <dir>          Destination directory for generated files.
    [--file-prefix <string>]    Name prefix for generated files. Default: "struct_".

EOD;
}

$name = "";
$input_file = "";
$output_dir = "";

for ($i = 1; $i < $argc;) {
    $arg = $argv[$i++];
    switch ($arg) {
        case "--name":
            $name = $argv[$i++];
            break;
        case "--input-file":
            $input_file = $argv[$i++];
            break;
        case "--output-dir":
            $output_dir = $argv[$i++];
            break;
        case "--help":
            print_help($argv[0]);
            exit(0);
        default:
            fatal_error("Unknown option: {$arg}");
    }
}

if ($name == "") {
    fatal_error("--name missing");
}

if ($input_file == "") {
    fatal_error("--input-file missing");
}

if ($output_dir == "") {
    fatal_error("--output-dir missing");
}

if (($data = file_get_contents($input_file)) === FALSE) {
    fatal_error("Failed to read input file");
}

if (!tokenize($data, $tokens)) {
    fatal_error("Failed to tokenize");
}

$parser = new parse_engine(new StructParser());

try {
    foreach ($tokens as $token) {
        $parser->eat($token[0], $token[1]);
    }
    $parser->eat_eof();
} catch (parse_error $e) {
    fatal_error("$input_file: Parse error: ".$e->getMessage());
}

$data = generate_header($name, $parser->semantic["directives"], $parser->semantic["structures"]);
if (file_put_contents("{$output_dir}/{$name}.h", $data) === NULL) {
    fatal_error("{$input_file}: Failed to write .h file");
}
