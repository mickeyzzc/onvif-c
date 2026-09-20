#!/bin/sh
# onvif-c host golden tests — zero dependencies beyond a C compiler.
# Exit 0 = byte-stability contract intact.
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"
$CC -std=c99 -Wall -Wextra -Werror -O2 \
    -o /tmp/onvif_c_tests test_core.c \
    ../core/onvif_xml.c ../core/onvif_probe.c ../core/onvif_events_ring.c
/tmp/onvif_c_tests
