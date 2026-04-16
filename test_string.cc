/* Masstree
 * Eddie Kohler, Yandong Mao, Robert Morris
 * Copyright (c) 2012-2013 President and Fellows of Harvard College
 * Copyright (c) 2012-2013 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Masstree LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Masstree LICENSE file; the license in that file
 * is legally binding.
 */
#include "string.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "straccum.hh"

using namespace lcdf;

// NOTE: this file used to contain additional `check_straccum_utf8<T>` /
// `check_straccum2_utf8<T>` helpers that exercised `Encoding::UTF8Encoder<T>`
// with `Encoding::UTF8`, `Encoding::UTF8NoNul`, and `Encoding::Windows1252`
// tag types. Those streaming-encoder classes were never ported to
// masstree-cpp (only the member `String::to_utf8()` family survived), so the
// helpers could not compile. They were removed rather than stubbed so this
// binary once again exercises what the port actually provides.

int
main(int argc, char *argv[])
{
    assert(String("abc").to_utf8() == "abc");
    assert(String("").to_utf8() == "");
    assert(String("ab\000cd", 5).to_utf8() == "abcd");
    assert(String("\xc3\x9dHi!").to_utf8() == "\xc3\x9dHi!");
    assert(String("\xddHi!").to_utf8() == "\xc3\x9dHi!");
    assert(String("\xc3\x9dHi!\x9c").to_utf8() == "\xc3\x9dHi!\xc5\x93");
    assert(String("ab\000c\x9c", 5).to_utf8() == "abc\xc5\x93");
    assert(String("\xc3\x9dXY\000c\x9c", 7).to_utf8() == "\xc3\x9dXYc\xc5\x93");

    if (argc == 2) {
        FILE *f;
        if (strcmp(argv[1], "-") == 0)
            f = stdin;
        else if (!(f = fopen(argv[1], "rb"))) {
            perror("test_string");
            exit(1);
        }
        StringAccum sa;
        while (!feof(f)) {
            size_t x = fread(sa.reserve(1024), 1, 1024, f);
            sa.adjust_length(x);
        }
        String s = sa.take_string().to_utf8(String::utf_strip_bom);
        fwrite(s.data(), 1, s.length(), stdout);
    }
    fprintf(stderr, "test_string: all assertions passed\n");
    return 0;
}
