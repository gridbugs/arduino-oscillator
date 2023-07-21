#!/bin/sh
set -euo pipefail

# Script that prints a compilation database (typically stored in
# compile_commands.json) which contains additional include paths found by
# querying avr-gcc.

include_paths() {
    # Print custom include paths used by the avr compiler to stdout, one per line.
    CC=avr-gcc
    $CC -E -Wp,-v - < /dev/null 2>&1 \
        | sed -n '/#include <...> search starts here:/,/End of search list./p' \
        | tail -n+2 \
        | head -n-1 \
        | xargs -n1 echo
}

compile_commands() {
    # Print the compilation database that would normally go in
    # compile_commands.json.
    # This would be simpler if bear supported printing to stdout:
    # https://github.com/rizsotto/Bear/issues/525
    TMP="$(mktemp -d)"
    trap "rm -rf $TMP" EXIT
    OUTPUT=$TMP/x.json
    bear --output $OUTPUT -- make --always-make > /dev/null
    cat $OUTPUT
}

# Comma-separated list of quoted include paths with "-I" prepended to each. E.g.
# "-Ifoo","-Ibar"
COMMA_SEPARATED_QUOTED_INCLUDE_ARGS=$(
    include_paths \
        | xargs -I{} echo '"-I{}"' \
        | tr '\n' , \
        | sed 's/,$//'
)

# Add the extra include paths to the compilation database and print the result.
compile_commands | \
    jq "map(.arguments += [$COMMA_SEPARATED_QUOTED_INCLUDE_ARGS])"
