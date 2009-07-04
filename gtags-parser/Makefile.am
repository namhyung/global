## Process this file with automake to create Makefile.in
#
# Copyright (c) 2000, 2001, 2003 Tama Communications Corporation
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
bin_PROGRAMS= gtags-parser

noinst_HEADERS = gctags.h c_res.h cpp_res.h java_res.h php_res.h asm_res.h \
		asm_parse.h

gtags_parser_SOURCES = gctags.c C.c Cpp.c asm_parse.c asm_scan.c java.c php.c

INCLUDES = @INCLUDES@

LDADD = @LDADD@

gtags_parser_DEPENDENCIES = $(LDADD)

man_MANS = gtags-parser.1

EXTRA_DIST = $(man_MANS) const.h manual.in reserved.pl HACKING \
		c_res.in c_res.gpf \
		cpp_res.in cpp_res.gpf \
		java_res.in java_res.gpf \
		php_res.in php_res.gpf php.l \
		asm_res.in asm_res.gpf asm_parse.y asm_scan.l
