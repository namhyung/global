%{
/*
 * Copyright (c) 2003, 2006 Tama Communications Corporation
 *
 * This file is part of GNU GLOBAL.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * scanner for PHP source code.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "defined.h"
#include "die.h"
#include "gparam.h"
#include "gctags.h"
#include "linetable.h"
#include "strbuf.h"
#include "php_res.h"

#define lex_symbol_generation_rule(x) php_ ## x
#define LEXLEX lex_symbol_generation_rule(lex)
#define LEXTEXT lex_symbol_generation_rule(text)
#define LEXLENG lex_symbol_generation_rule(leng)
#define LEXRESTART lex_symbol_generation_rule(restart)
#define LEXLINENO lex_symbol_generation_rule(lineno)

#define PHP_TOKEN		1
#define PHP_VARIABLE		2
#define PHP_STRING		3
#define PHP_POINTER		4
#define PHP_DOLLAR		5
#define PHP_LPAREN		'('
#define PHP_RPAREN		')'
#define PHP_LBRACE		'{'
#define PHP_RBRACE		'}'
#define PHP_LBRACK		'['
#define PHP_RBRACK		']'

static void debug_print(const char *, ...);
static int level;			/* block nest level */
static STRBUF *string;			/* string */
static char end_of_here_document[IDENTLEN+1];
static int pre_here_document;

/*
 * For debug.
 */
static void
debug_print(const char *s, ...)
{
	va_list ap;

	if (!debug)
		return;
	va_start(ap, s);
	(void)vfprintf(stderr, s, ap);
	va_end(ap);
}
#ifdef YYLMAX
#undef YYLMAX
#endif
#define YYLMAX 1024
#ifdef ECHO
#undef ECHO
#endif
#define ECHO debug_print("%s", LEXTEXT)
#ifdef PUT
#undef PUT
#endif
#define PUT(tag, lno, file) do {					\
	if (!nflag) {							\
		fprintf(stdout, "%-16s %4d %-16s ",tag, lno, file);	\
		linetable_print(stdout, lno);				\
	}								\
} while (0)

/*
 * IO routine.
 */
#define YY_INPUT(buf,result,max_size) \
	do { \
		if ((result = linetable_read(buf, max_size)) == -1) \
			result = YY_NULL; \
	} while (0)
%}
 /* Definitions */
H		0[Xx][0-9A-Fa-f]+
N		[0-9]+
L		{N}L?
D1		{N}\.{N}([Ee][+-]?{N})?
D2		\.{N}([Ee][+-]?{N})?
NUMBER		-?({L}|{D1}|{D2})
 /* We accept multi-bytes character */
ALPHA		[a-zA-Z_\x80-\xff]
ALPHANUM	[a-zA-Z_\x80-\xff0-9]
WORD		{ALPHA}{ALPHANUM}*
%start	PHP STRING LITERAL HEREDOCUMENT
%option 8bit caseless noyywrap nounput yylineno never-interactive prefix="php_"
%%
\n			{
				ECHO;
				if (pre_here_document == 1) {
					pre_here_document = 0;
					debug_print("[BEGIN HEREDOCUMENT:%s]\n", end_of_here_document);
					BEGIN HEREDOCUMENT;
				}
			}
 /* Start PHP */
<INITIAL>"<?="		ECHO; BEGIN PHP;
<INITIAL>"<?"		ECHO; BEGIN PHP;
<INITIAL>"<?php"	ECHO; BEGIN PHP;
<INITIAL>"<%"		ECHO; BEGIN PHP;
<INITIAL>"<script[ \t]+language=(\")?php(\")?>"	ECHO; BEGIN PHP;
 /* Ignore HTML */
<INITIAL>.		ECHO;
 /* End of PHP */
<PHP>"?>"		ECHO; BEGIN INITIAL;
<PHP>"%>"		ECHO; BEGIN INITIAL;
<PHP>"</script>"	ECHO; BEGIN INITIAL;

 /* Comment */
<PHP>"/*"		{
				int c;

				debug_print("</*");
				while ((c = input()) != EOF) {
					debug_print("%c", c);
					if (c == '*') {
						while ((c = input()) != EOF && c == '*')
							debug_print("%c", c);
						debug_print("%c", c);
						if (c == EOF || c == '/')
							break;
					}
				}
				if (c == EOF)
					die("unexpected end of comment.");
				debug_print(">");
			}
<PHP>"//".*		debug_print("<%s>", LEXTEXT);
<PHP>"#".*		debug_print("<%s>", LEXTEXT);

 /* String */
<PHP>\"			{ strbuf_reset(string); BEGIN STRING; }
<STRING>\"		{
				debug_print("<S:%s>", strbuf_value(string));
				BEGIN PHP;
					
				return PHP_STRING;
			}
<STRING>\\.		strbuf_puts(string, LEXTEXT);
<STRING>.		strbuf_putc(string, LEXTEXT[0]);

 /* Literal */
<PHP>\'			{ strbuf_reset(string); BEGIN LITERAL; }
<LITERAL>\'		{
				debug_print("<L:%s>", strbuf_value(string));
				BEGIN PHP;
					
				return PHP_STRING;
			}
<LITERAL>\\.		strbuf_puts(string, LEXTEXT);
<LITERAL>.		strbuf_putc(string, LEXTEXT[0]);

 /* Here document */
<PHP><<<{WORD}		{
				debug_print("<L:%s>", LEXTEXT);
				/* extract word and save */
				if (LEXLENG - 3 > IDENTLEN)
					die("Too long name '%s'.", LEXTEXT + 3);
				strcpy(end_of_here_document, LEXTEXT + 3);
				/* begin here document from the next line */
				pre_here_document = 1;
			}
<HEREDOCUMENT>^[ \t]*{WORD} {
				const char *keyword = strtrim((const char *)LEXTEXT, TRIM_HEAD, NULL);
				ECHO;
				if (!strcmp(end_of_here_document, keyword)) {
					debug_print("[END HEREDOCUMENT]");
					end_of_here_document[0] = '\0';
					BEGIN PHP;
				}
			}

 /* Cast */
<PHP>\([ \t]*(bool|boolean|int|integer|real|double|float|string|array|object)[ \t]*\)
			;
<PHP,STRING,HEREDOCUMENT>$\{{WORD}\} {
				/*
				 * 0123456	yyleng = 6
				 * ${abc}\0
				 */
				if (YY_START == STRING)
					strbuf_puts(string, LEXTEXT);
				memcpy(LEXTEXT, &LEXTEXT[2], LEXLENG - 3);
				LEXTEXT[LEXLENG - 3] = '\0';
				LEXLENG = LEXLENG - 3;
				debug_print("<V:%s>", LEXTEXT);

				return PHP_VARIABLE;
			}
<PHP,STRING,HEREDOCUMENT>${WORD} {
				/*
				 * 01234	yyleng = 4
				 * $abc\0
				 */
				if (YY_START == STRING)
					strbuf_puts(string, LEXTEXT);
				memcpy(LEXTEXT, &LEXTEXT[1], LEXLENG - 1);
				LEXTEXT[LEXLENG - 1] = '\0';
				LEXLENG = LEXLENG - 1;
				debug_print("<V:%s>", LEXTEXT);

				return PHP_VARIABLE;
			}
<HEREDOCUMENT>.		ECHO;

<PHP>{NUMBER}		debug_print("<N:%s>", LEXTEXT);
<PHP>{WORD}		{
				int id = php_reserved_word(LEXTEXT, LEXLENG);
				if (id) {
					debug_print("<Reserved:%s>", LEXTEXT);
					return id;
				} else {
					debug_print("<T:%s>", LEXTEXT);
					return PHP_TOKEN;
				}
			}
 /* Operator */
<PHP>[{}]		{
				int c = LEXTEXT[0];
				if (c == PHP_LBRACE)
					level++;
				else
					level--;
				debug_print("%c[%d]", c, level);

				return c;
			}
<PHP>[][()]		{
				return LEXTEXT[0];
			}
<PHP>[-+*/%&~^]=?	ECHO;
<PHP>[=><!]=		ECHO;
<PHP>[-+&|<>]{2}=?	ECHO;
<PHP>"<>"		ECHO;
<PHP>"$"		{ ECHO; return PHP_DOLLAR; }
<PHP>.			ECHO;

%%
/*
 * php: read PHP file and pickup tag entries.
 */
void
php(const char *file)
{
	int token;
	int target = (sflag) ? SYM : (rflag) ? REF : DEF;

	level = 0;
	string = strbuf_open(0);
	if (linetable_open(file) == -1)
		die("'%s' cannot open.", file);

	LEXRESTART(NULL);
	LEXLINENO = 1;
	while ((token = LEXLEX()) != 0) {
		switch (token) {
		case PHP_DEFINE:
			if (LEXLEX() != PHP_LPAREN)
				break;
			if (LEXLEX() != PHP_STRING)
				break;
			if (target == DEF)
				PUT(strbuf_value(string), LEXLINENO, file);
			break;
		case PHP_CLASS:
			if (LEXLEX() != PHP_TOKEN)
				break;
			if (target == DEF)
				PUT(LEXTEXT, LEXLINENO, file);
			break;
		case PHP_FUNCTION:
		case PHP_CFUNCTION:
		case PHP_OLD_FUNCTION:
			if (LEXLEX() != PHP_TOKEN)
				break;
			if (target == DEF)
				PUT(LEXTEXT, LEXLINENO, file);
			break;
		case PHP_VARIABLE:
			if (php_reserved_variable(LEXTEXT, LEXLENG)) {
				if (target == SYM)
					PUT(LEXTEXT, LEXLINENO, file);
				if (LEXLEX() == PHP_LBRACK && LEXLEX() == PHP_STRING && LEXLEX() == PHP_RBRACK) {
					char *str = strbuf_value(string);

					if (strchr(str, '$') == 0)
						if (target == SYM)
							PUT(str, LEXLINENO, file);
				}
			} else if (!strcmp(LEXTEXT, "this")) {
				;
			} else {
				if (target == SYM)
					PUT(LEXTEXT, LEXLINENO, file);
			}
			break;
		case PHP_POINTER:
			if (LEXLEX() != PHP_TOKEN)
				break;
			/* FALLTHROUGH */
		case PHP_TOKEN:
			if (target == REF) {
				if (defined(LEXTEXT))
					PUT(LEXTEXT, LEXLINENO, file);
			} else if (target == SYM) {
				if (!defined(LEXTEXT))
					PUT(LEXTEXT, LEXLINENO, file);
			}
			break;
		case PHP_NEW:
			if (LEXLEX() != PHP_TOKEN)
				break;
			if (target == REF)
				PUT(LEXTEXT, LEXLINENO, file);
			break;
		/*
		 * ${x->y}
		 */
		case PHP_DOLLAR:
			if (LEXLEX() != PHP_LBRACE)
				break;
			while ((token = LEXLEX()) != PHP_RBRACE) {
				if (token == PHP_TOKEN) {
					if (target == REF) {
						if (defined(LEXTEXT))
							PUT(LEXTEXT, LEXLINENO, file);
					} else if (target == SYM) {
						if (!defined(LEXTEXT))
							PUT(LEXTEXT, LEXLINENO, file);
					}
				}
			}
			break;
		default:
			break;
		}
	}
	linetable_close();
	strbuf_close(string);
}
