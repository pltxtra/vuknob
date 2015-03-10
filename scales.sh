#!/bin/bash

printf "struct ScaleEntry scales_library[] = {\n" > scales.hh
cat scales.txt | awk 'BEGIN { FS = "," } ; {print "{ 0x" $1 ", \"" $2 "\", {0x" $3 ", 0x" $4 ", 0x" $5 ", 0x" $6 ", 0x" $7 ", 0x" $8 ", 0x" $9 ", 0x" $3 ", 0x" $4 ", 0x" $5 ", 0x" $6 ", 0x" $7 ", 0x" $8 ", 0x" $9 ", 0x" $9 ", 0x" $9 "} }, "}' >> scales.hh
printf "};\n" >> scales.hh
