/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2008
 *	Tama Communications Corporation
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "gctags.h"
#include "defined.h"
#include "die.h"
#include "strbuf.h"
#include "strlimcpy.h"
#include "token.h"
#include "c_res.h"

static void C_family(const char *, int);
static void process_attribute(int);
static int function_definition(int, char *);
static void condition_macro(int, int);
static int seems_datatype(const char *);

#define IS_TYPE_QUALIFIER(c)	((c) == C_CONST || (c) == C_RESTRICT || (c) == C_VOLATILE)

#define DECLARATIONS    0
#define RULES           1
#define PROGRAMS        2

#define TYPE_C		0
#define TYPE_LEX	1
#define TYPE_YACC	2
/*
 * #ifdef stack.
 */
#define MAXPIFSTACK	100

static struct {
	short start;		/* level when #if block started */
	short end;		/* level when #if block end */
	short if0only;		/* #if 0 or notdef only */
} stack[MAXPIFSTACK], *cur;
static int piflevel;		/* condition macro level */
static int level;		/* brace level */
static int externclevel;	/* extern "C" block level */

/*
 * yacc: read yacc file and pickup tag entries.
 */
void
yacc(const char *file)
{
	C_family(file, TYPE_YACC);
}
/*
 * C: read C file and pickup tag entries.
 */
void
C(const char *file)
{
	C_family(file, TYPE_C);
}
/*
 *	i)	file	source file
 *	i)	type	TYPE_C, TYPE_YACC, TYPE_LEX
 */
static void
C_family(const char *file, int type)
{
	int c, cc;
	int savelevel;
	int target;
	int startmacro, startsharp;
	const char *interested = "{}=;";
	STRBUF *sb = strbuf_open(0);
	/*
	 * yacc file format is like the following.
	 *
	 * declarations
	 * %%
	 * rules
	 * %%
	 * programs
	 *
	 */
	int yaccstatus = (type == TYPE_YACC) ? DECLARATIONS : PROGRAMS;
	int inC = (type == TYPE_YACC) ? 0 : 1;	/* 1 while C source */

	level = piflevel = externclevel = 0;
	savelevel = -1;
	target = (sflag) ? SYM : (rflag) ? REF : DEF;
	startmacro = startsharp = 0;

	if (!opentoken(file))
		die("'%s' cannot open.", file);
	cmode = 1;			/* allow token like '#xxx' */
	crflag = 1;			/* require '\n' as a token */
	if (type == TYPE_YACC)
		ymode = 1;		/* allow token like '%xxx' */

	while ((cc = nexttoken(interested, c_reserved_word)) != EOF) {
		switch (cc) {
		case SYMBOL:		/* symbol	*/
			if (inC && peekc(0) == '('/* ) */) {
				if (isnotfunction(token)) {
					if (target == REF && defined(token))
						PUT(token, lineno, sp);
				} else if (level > 0 || startmacro) {
					if (target == REF) {
						if (defined(token))
							PUT(token, lineno, sp);
					} else if (target == SYM) {
						if (!defined(token))
							PUT(token, lineno, sp);
					}
				} else if (level == 0 && !startmacro && !startsharp) {
					char arg1[MAXTOKEN], savetok[MAXTOKEN], *saveline;
					int savelineno = lineno;

					strlimcpy(savetok, token, sizeof(savetok));
					strbuf_reset(sb);
					strbuf_puts(sb, sp);
					saveline = strbuf_value(sb);
					arg1[0] = '\0';
					/*
					 * Guile function entry using guile-snarf is like follows:
					 *
					 * SCM_DEFINE (scm_list, "list", 0, 0, 1, 
					 *           (SCM objs),
					 *            "Return a list containing OBJS, the arguments to `list'.")
					 * #define FUNC_NAME s_scm_list
					 * {
					 *   return objs;
					 * }
					 * #undef FUNC_NAME
					 *
					 * We should assume the first argument as a function name instead of 'SCM_DEFINE'.
					 */
					if (function_definition(target, arg1)) {
						if (!strcmp(savetok, "SCM_DEFINE") && *arg1)
							strlimcpy(savetok, arg1, sizeof(savetok));
						if (target == DEF)
							PUT(savetok, savelineno, saveline);
					} else {
						if (target == REF && defined(savetok))
							PUT(savetok, savelineno, saveline);
					}
				}
			} else {
				if (target == REF) {
					if (defined(token))
						PUT(token, lineno, sp);
				} else if (target == SYM) {
					if (!defined(token))
						PUT(token, lineno, sp);
				}
			}
			break;
		case '{':  /* } */
			DBG_PRINT(level, "{"); /* } */
			if (yaccstatus == RULES && level == 0)
				inC = 1;
			++level;
			if (bflag && atfirst) {
				if (wflag && level != 1)
					warning("forced level 1 block start by '{' at column 0 [+%d %s].", lineno, curfile); /* } */
				level = 1;
			}
			break;
			/* { */
		case '}':
			if (--level < 0) {
				if (externclevel > 0)
					externclevel--;
				else if (wflag)
					warning("missing left '{' [+%d %s].", lineno, curfile); /* } */
				level = 0;
			}
			if (eflag && atfirst) {
				if (wflag && level != 0) /* { */
					warning("forced level 0 block end by '}' at column 0 [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (yaccstatus == RULES && level == 0)
				inC = 0;
			/* { */
			DBG_PRINT(level, "}");
			break;
		case '\n':
			if (startmacro && level != savelevel) {
				if (wflag)
					warning("different level before and after #define macro. reseted. [+%d %s].", lineno, curfile);
				level = savelevel;
			}
			startmacro = startsharp = 0;
			break;
		case YACC_SEP:		/* %% */
			if (level != 0) {
				if (wflag)
					warning("forced level 0 block end by '%%' [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (yaccstatus == DECLARATIONS) {
				if (target == DEF)
					PUT("yyparse", lineno, sp);
				yaccstatus = RULES;
			} else if (yaccstatus == RULES)
				yaccstatus = PROGRAMS;
			inC = (yaccstatus == PROGRAMS) ? 1 : 0;
			break;
		case YACC_BEGIN:	/* %{ */
			if (level != 0) {
				if (wflag)
					warning("forced level 0 block end by '%%{' [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (inC == 1 && wflag)
				warning("'%%{' appeared in C mode. [+%d %s].", lineno, curfile);
			inC = 1;
			break;
		case YACC_END:		/* %} */
			if (level != 0) {
				if (wflag)
					warning("forced level 0 block end by '%%}' [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (inC == 0 && wflag)
				warning("'%%}' appeared in Yacc mode. [+%d %s].", lineno, curfile);
			inC = 0;
			break;
		case YACC_UNION:	/* %union {...} */
			if (yaccstatus == DECLARATIONS && target == DEF)
				PUT("YYSTYPE", lineno, sp);
			break;
		/*
		 * #xxx
		 */
		case SHARP_DEFINE:
		case SHARP_UNDEF:
			startmacro = 1;
			savelevel = level;
			if ((c = nexttoken(interested, c_reserved_word)) != SYMBOL) {
				pushbacktoken();
				break;
			}
			if (peekc(1) == '('/* ) */) {
				if (target == DEF)
					PUT(token, lineno, sp);
				while ((c = nexttoken("()", c_reserved_word)) != EOF && c != '\n' && c != /* ( */ ')')
					if (c == SYMBOL && target == SYM)
						PUT(token, lineno, sp);
				if (c == '\n')
					pushbacktoken();
			} else {
				if (target == DEF)
					PUT(token, lineno, sp);
			}
			break;
		case SHARP_IMPORT:
		case SHARP_INCLUDE:
		case SHARP_INCLUDE_NEXT:
		case SHARP_ERROR:
		case SHARP_LINE:
		case SHARP_PRAGMA:
		case SHARP_WARNING:
		case SHARP_IDENT:
		case SHARP_SCCS:
			while ((c = nexttoken(interested, c_reserved_word)) != EOF && c != '\n')
				;
			break;
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(cc, target);
			break;
		case SHARP_SHARP:		/* ## */
			(void)nexttoken(interested, c_reserved_word);
			break;
		case C_EXTERN: /* for 'extern "C"/"C++"' */
			if (peekc(0) != '"') /* " */
				continue; /* If does not start with '"', continue. */
			while ((c = nexttoken(interested, c_reserved_word)) == '\n')
				;
			/*
			 * 'extern "C"/"C++"' block is a kind of namespace block.
			 * (It doesn't have any influence on level.)
			 */
			if (c == '{') /* } */
				externclevel++;
			else
				pushbacktoken();
			break;
		case C_STRUCT:
		case C_ENUM:
		case C_UNION:
			c = nexttoken(interested, c_reserved_word);
			if (c == SYMBOL) {
				if (peekc(0) == '{') /* } */ {
					if (target == DEF)
						PUT(token, lineno, sp);
				} else if (target == REF) {
					if (defined(token))
						PUT(token, lineno, sp);
				} else if (target == SYM) {
					if (!defined(token))
						PUT(token, lineno, sp);
				}
				c = nexttoken(interested, c_reserved_word);
			}
			if (c == '{' /* } */ && cc == C_ENUM) {
				int savelevel = level;

				for (; c != EOF; c = nexttoken(interested, c_reserved_word)) {
					switch (c) {
					case SHARP_IFDEF:
					case SHARP_IFNDEF:
					case SHARP_IF:
					case SHARP_ELIF:
					case SHARP_ELSE:
					case SHARP_ENDIF:
						condition_macro(c, target);
						continue;
					default:
						break;
					}
					if (c == '{')
						level++;
					else if (c == '}') {
						if (--level == savelevel)
							break;
					} else if (c == SYMBOL) {
						if (target == DEF)
							PUT(token, lineno, sp);
					}
				}
			} else {
				pushbacktoken();
			}
			break;
		/* control statement check */
		case C_BREAK:
		case C_CASE:
		case C_CONTINUE:
		case C_DEFAULT:
		case C_DO:
		case C_ELSE:
		case C_FOR:
		case C_GOTO:
		case C_IF:
		case C_RETURN:
		case C_SWITCH:
		case C_WHILE:
			if (wflag && !startmacro && level == 0)
				warning("Out of function. %8s [+%d %s]", token, lineno, curfile);
			break;
		case C_TYPEDEF:
			{
				/*
				 * This parser is too complex to maintain.
				 * We should rewrite the whole.
				 */
				char savetok[MAXTOKEN];
				int savelineno = 0;
				int typedef_savelevel = level;

				savetok[0] = 0;

				/* skip type qualifiers */
				do {
					c = nexttoken("{}(),;", c_reserved_word);
				} while (IS_TYPE_QUALIFIER(c) || c == '\n');

				if (wflag && c == EOF) {
					warning("unexpected eof. [+%d %s]", lineno, curfile);
					break;
				} else if (c == C_ENUM || c == C_STRUCT || c == C_UNION) {
					char *interest_enum = "{},;";
					int c_ = c;

					c = nexttoken(interest_enum, c_reserved_word);
					/* read enum name if exist */
					if (c == SYMBOL) {
						if (peekc(0) == '{') /* } */ {
							if (target == DEF)
								PUT(token, lineno, sp);
						} else if (target == REF) {
							if (defined(token))
								PUT(token, lineno, sp);
						} else if (target == SYM) {
							if (!defined(token))
								PUT(token, lineno, sp);
						}
						c = nexttoken(interest_enum, c_reserved_word);
					}
					for (; c != EOF; c = nexttoken(interest_enum, c_reserved_word)) {
						switch (c) {
						case SHARP_IFDEF:
						case SHARP_IFNDEF:
						case SHARP_IF:
						case SHARP_ELIF:
						case SHARP_ELSE:
						case SHARP_ENDIF:
							condition_macro(c, target);
							continue;
						default:
							break;
						}
						if (c == ';' && level == typedef_savelevel) {
							if (savetok[0] && target == DEF)
								PUT(savetok, savelineno, sp);
							break;
						} else if (c == '{')
							level++;
						else if (c == '}') {
							if (--level == typedef_savelevel)
								break;
						} else if (c == SYMBOL) {
							if (c_ == C_ENUM) {
								if (target == DEF && level > typedef_savelevel)
									PUT(token, lineno, sp);
								if (target == SYM && level == typedef_savelevel && !defined(token))
									PUT(token, lineno, sp);
							} else {
								if (target == REF) {
									if (level > typedef_savelevel && defined(token))
										PUT(token, lineno, sp);
								} else if (target == SYM) {
									if (!defined(token))
										PUT(token, lineno, sp);
								} else if (target == DEF) {
									/* save lastest token */
									strlimcpy(savetok, token, sizeof(savetok));
									savelineno = lineno;
								}
							}
						}
					}
					if (c == ';')
						break;
					if (wflag && c == EOF) {
						warning("unexpected eof. [+%d %s]", lineno, curfile);
						break;
					}
				} else if (c == SYMBOL) {
					if (target == REF && defined(token))
						PUT(token, lineno, sp);
					if (target == SYM && !defined(token))
						PUT(token, lineno, sp);
				}
				savetok[0] = 0;
				while ((c = nexttoken("(),;", c_reserved_word)) != EOF) {
					switch (c) {
					case SHARP_IFDEF:
					case SHARP_IFNDEF:
					case SHARP_IF:
					case SHARP_ELIF:
					case SHARP_ELSE:
					case SHARP_ENDIF:
						condition_macro(c, target);
						continue;
					default:
						break;
					}
					if (c == '(')
						level++;
					else if (c == ')')
						level--;
					else if (c == SYMBOL) {
						if (level > typedef_savelevel) {
							if (target == SYM)
								PUT(token, lineno, sp);
						} else {
							/* put latest token if any */
							if (savetok[0]) {
								if (target == SYM)
									PUT(savetok, savelineno, sp);
							}
							/* save lastest token */
							strlimcpy(savetok, token, sizeof(savetok));
							savelineno = lineno;
						}
					} else if (c == ',' || c == ';') {
						if (savetok[0]) {
							if (target == DEF)
								PUT(savetok, lineno, sp);
							savetok[0] = 0;
						}
					}
					if (level == typedef_savelevel && c == ';')
						break;
				}
				if (wflag) {
					if (c == EOF)
						warning("unexpected eof. [+%d %s]", lineno, curfile);
					else if (level != typedef_savelevel)
						warning("() block unmatched. (last at level %d.)[+%d %s]", level, lineno, curfile);
				}
			}
			break;
		case C___ATTRIBUTE__:
			process_attribute(target);
			break;
		default:
			break;
		}
	}
	strbuf_close(sb);
	if (wflag) {
		if (level != 0)
			warning("{} block unmatched. (last at level %d.)[+%d %s]", level, lineno, curfile);
		if (piflevel != 0)
			warning("#if block unmatched. (last at level %d.)[+%d %s]", piflevel, lineno, curfile);
	}
	closetoken();
}
/*
 * process_attribute: skip attributes in __attribute__((...)).
 *
 *	r)	target type
 */
static void
process_attribute(int target)
{
	int brace = 0;
	int c;
	/*
	 * Skip '...' in __attribute__((...))
	 * but pick up symbols in it.
	 */
	while ((c = nexttoken("()", c_reserved_word)) != EOF) {
		if (c == '(')
			brace++;
		else if (c == ')')
			brace--;
		else if (c == SYMBOL) {
			if (target == REF) {
				if (defined(token))
					PUT(token, lineno, sp);
			} else if (target == SYM) {
				if (!defined(token))
					PUT(token, lineno, sp);
			}
		}
		if (brace == 0)
			break;
	}
}
/*
 * function_definition: return if function definition or not.
 *
 *	i)	target	DEF, REF, SYMBOL
 *	o)	arg1	the first argument
 *	r)	target type
 */
static int
function_definition(int target, char arg1[MAXTOKEN])
{
	int c;
	int brace_level, isdefine;
	int accept_arg1 = 0;

	brace_level = isdefine = 0;
	while ((c = nexttoken("()", c_reserved_word)) != EOF) {
		switch (c) {
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(c, target);
			continue;
		default:
			break;
		}
		if (c == '('/* ) */)
			brace_level++;
		else if (c == /* ( */')') {
			if (--brace_level == 0)
				break;
		}
		/* pick up symbol */
		if (c == SYMBOL) {
			if (accept_arg1 == 0) {
				accept_arg1 = 1;
				strlimcpy(arg1, token, MAXTOKEN);
			}
			if (target == REF) {
				if (seems_datatype(token) && defined(token))
					PUT(token, lineno, sp);
			} else if (target == SYM) {
				if (!seems_datatype(token) || !defined(token))
					PUT(token, lineno, sp);
			}
		}
	}
	if (c == EOF)
		return 0;
	brace_level = 0;
	while ((c = nexttoken(",;[](){}=", c_reserved_word)) != EOF) {
		switch (c) {
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(c, target);
			continue;
		case C___ATTRIBUTE__:
			process_attribute(target);
			continue;
		default:
			break;
		}
		if (c == '('/* ) */ || c == '[')
			brace_level++;
		else if (c == /* ( */')' || c == ']')
			brace_level--;
		else if (brace_level == 0
		    && ((c == SYMBOL && strcmp(token, "__THROW")) || IS_RESERVED_WORD(c)))
			isdefine = 1;
		else if (c == ';' || c == ',') {
			if (!isdefine)
				break;
		} else if (c == '{' /* } */) {
			pushbacktoken();
			return 1;
		} else if (c == /* { */'}')
			break;
		else if (c == '=')
			break;

		/* pick up symbol */
		if (c == SYMBOL) {
			if (target == REF) {
				if (seems_datatype(token) && defined(token))
					PUT(token, lineno, sp);
			} else if (target == SYM) {
				if (!seems_datatype(token) || !defined(token))
					PUT(token, lineno, sp);
			}
		}
	}
	return 0;
}

/*
 * condition_macro: 
 *
 *	i)	cc	token
 *	i)	target	current target
 */
static void
condition_macro(int cc, int target)
{
	cur = &stack[piflevel];
	if (cc == SHARP_IFDEF || cc == SHARP_IFNDEF || cc == SHARP_IF) {
		DBG_PRINT(piflevel, "#if");
		if (++piflevel >= MAXPIFSTACK)
			die("#if stack over flow. [%s]", curfile);
		++cur;
		cur->start = level;
		cur->end = -1;
		cur->if0only = 0;
		if (peekc(0) == '0')
			cur->if0only = 1;
		else if ((cc = nexttoken(NULL, c_reserved_word)) == SYMBOL && !strcmp(token, "notdef"))
			cur->if0only = 1;
		else
			pushbacktoken();
	} else if (cc == SHARP_ELIF || cc == SHARP_ELSE) {
		DBG_PRINT(piflevel - 1, "#else");
		if (cur->end == -1)
			cur->end = level;
		else if (cur->end != level && wflag)
			warning("uneven level. [+%d %s]", lineno, curfile);
		level = cur->start;
		cur->if0only = 0;
	} else if (cc == SHARP_ENDIF) {
		int minus = 0;

		--piflevel;
		if (piflevel < 0) {
			minus = 1;
			piflevel = 0;
		}
		DBG_PRINT(piflevel, "#endif");
		if (minus) {
			if (wflag)
				warning("#if block unmatched. reseted. [+%d %s]", lineno, curfile);
		} else {
			if (cur->if0only)
				level = cur->start;
			else if (cur->end != -1) {
				if (cur->end != level && wflag)
					warning("uneven level. [+%d %s]", lineno, curfile);
				level = cur->end;
			}
		}
	}
	while ((cc = nexttoken(NULL, c_reserved_word)) != EOF && cc != '\n') {
                if (cc == SYMBOL && strcmp(token, "defined") != 0) {
			if (target == REF) {
				if (defined(token))
		                        PUT(token, lineno, sp);
			} else if (target == SYM) {
				if (!defined(token))
		                	PUT(token, lineno, sp);
			}
		}
	}
}
/*
 * seems_datatype: decide whether or not it is a data type.
 *
 *	i)	token	token
 *	r)		0: not data type, 1: data type
 */
static int
seems_datatype(const char *token)
{
	int length = strlen(token);
	const char *p = token + length;

	if (length > 2 && strcmp(p - 2, "_t") == 0)
		return 1;
	for (p = token; *p; p++)
		if (islower(*p))
			return 0;
	return 1;
}
