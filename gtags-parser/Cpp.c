/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
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
#include "locatestring.h"
#include "strbuf.h"
#include "strlimcpy.h"
#include "token.h"
#include "cpp_res.h"

static void process_attribute(int);
static int function_definition(int);
static int seems_datatype(const char *);
static void condition_macro(int, int);

#define MAXCOMPLETENAME 1024            /* max size of complete name of class */
#define MAXCLASSSTACK   100             /* max size of class stack */
#define IS_CV_QUALIFIER(c)      ((c) == CPP_CONST || (c) == CPP_VOLATILE)

/*
 * #ifdef stack.
 */
#define MAXPIFSTACK	100

static struct {
	short start;		/* level when #if block started */
	short end;		/* level when #if block end */
	short if0only;		/* #if 0 or notdef only */
} pifstack[MAXPIFSTACK], *cur;
static int piflevel;		/* condition macro level */
static int level;		/* brace level */
static int namespacelevel;	/* namespace block level */

/*
 * Cpp: read C++ file and pickup tag entries.
 */
void
Cpp(const char *file)
{
	int c, cc;
	int savelevel;
	int target;
	int startclass, startthrow, startmacro, startsharp, startequal;
	char classname[MAXTOKEN];
	char completename[MAXCOMPLETENAME];
	int classlevel;
	struct {
		char *classname;
		char *terminate;
		int level;
	} stack[MAXCLASSSTACK];
	const char *interested = "{}=;~";
	STRBUF *sb = strbuf_open(0);

	*classname = *completename = 0;
	stack[0].classname = completename;
	stack[0].terminate = completename;
	stack[0].level = 0;
	level = classlevel = piflevel = namespacelevel = 0;
	savelevel = -1;
	target = (sflag) ? SYM : (rflag) ? REF : DEF;
	startclass = startthrow = startmacro = startsharp = startequal = 0;

	if (!opentoken(file))
		die("'%s' cannot open.", file);
	cmode = 1;			/* allow token like '#xxx' */
	crflag = 1;			/* require '\n' as a token */
	cppmode = 1;			/* treat '::' as a token */

	while ((cc = nexttoken(interested, cpp_reserved_word)) != EOF) {
		if (cc == '~' && level == stack[classlevel].level)
			continue;
		switch (cc) {
		case SYMBOL:		/* symbol	*/
			if (startclass || startthrow) {
				if (target == REF && defined(token))
					PUT(token, lineno, sp);
			} else if (peekc(0) == '('/* ) */) {
				if (isnotfunction(token)) {
					if (target == REF && defined(token))
						PUT(token, lineno, sp);
				} else if (level > stack[classlevel].level || startequal || startmacro) {
					if (target == REF) {
						if (defined(token))
							PUT(token, lineno, sp);
					} else if (target == SYM) {
						if (!defined(token))
							PUT(token, lineno, sp);
					}
				} else if (level == stack[classlevel].level && !startmacro && !startsharp && !startequal) {
					char savetok[MAXTOKEN], *saveline;
					int savelineno = lineno;

					strlimcpy(savetok, token, sizeof(savetok));
					strbuf_reset(sb);
					strbuf_puts(sb, sp);
					saveline = strbuf_value(sb);
					if (function_definition(target)) {
						/* ignore constructor */
						if (target == DEF && strcmp(stack[classlevel].classname, savetok))
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
		case CPP_USING:
			/*
			 * using namespace name;
			 * using ...;
			 */
			if ((c = nexttoken(interested, cpp_reserved_word)) == CPP_NAMESPACE) {
				if ((c = nexttoken(interested, cpp_reserved_word)) == SYMBOL) {
					if (target == REF)
						PUT(token, lineno, sp);
				} else {
					if (wflag)
						warning("missing namespace name. [+%d %s].", lineno, curfile);
					pushbacktoken();
				}
			} else
				pushbacktoken();
			break;
		case CPP_NAMESPACE:
			crflag = 0;
			/*
			 * namespace name = ...;
			 * namespace [name] { ... }
			 */
			if ((c = nexttoken(interested, cpp_reserved_word)) == SYMBOL) {
				if (target == DEF)
					PUT(token, lineno, sp);
				if ((c = nexttoken(interested, cpp_reserved_word)) == '=') {
					crflag = 1;
					break;
				}
			}
			/*
			 * Namespace block doesn't have any influence on level.
			 */
			if (c == '{') /* } */ {
				namespacelevel++;
			} else {
				if (wflag)
					warning("missing namespace block. [+%d %s](0x%x).", lineno, curfile, c);
			}
			crflag = 1;
			break;
		case CPP_EXTERN: /* for 'extern "C"/"C++"' */
			if (peekc(0) != '"') /* " */
				continue; /* If does not start with '"', continue. */
			while ((c = nexttoken(interested, cpp_reserved_word)) == '\n')
				;
			/*
			 * 'extern "C"/"C++"' block is a kind of namespace block.
			 * (It doesn't have any influence on level.)
			 */
			if (c == '{') /* } */
				namespacelevel++;
			else
				pushbacktoken();
			break;
		case CPP_CLASS:
			DBG_PRINT(level, "class");
			if ((c = nexttoken(interested, cpp_reserved_word)) == SYMBOL) {
				strlimcpy(classname, token, sizeof(classname));
				/*
				 * Ignore forward definitions.
				 * "class name;"
				 */
				if (peekc(0) != ';') {
					startclass = 1;
					if (target == DEF)
						PUT(token, lineno, sp);
				}
			}
			break;
		case '{':  /* } */
			DBG_PRINT(level, "{"); /* } */
			++level;
			if (bflag && atfirst) {
				if (wflag && level != 1)
					warning("forced level 1 block start by '{' at column 0 [+%d %s].", lineno, curfile); /* } */
				level = 1;
			}
			if (startclass) {
				char *p = stack[classlevel].terminate;
				char *q = classname;

				if (++classlevel >= MAXCLASSSTACK)
					die("class stack over flow.[%s]", curfile);
				if (classlevel > 1)
					*p++ = '.';
				stack[classlevel].classname = p;
				while (*q)
					*p++ = *q++;
				stack[classlevel].terminate = p;
				stack[classlevel].level = level;
				*p++ = 0;
			}
			startclass = startthrow = 0;
			break;
			/* { */
		case '}':
			if (--level < 0) {
				if (namespacelevel > 0)
					namespacelevel--;
				else if (wflag)
					warning("missing left '{' [+%d %s].", lineno, curfile); /* } */
				level = 0;
			}
			if (eflag && atfirst) {
				if (wflag && level != 0)
					/* { */
					warning("forced level 0 block end by '}' at column 0 [+%d %s].", lineno, curfile);
				level = 0;
			}
			if (level < stack[classlevel].level)
				*(stack[--classlevel].terminate) = 0;
			/* { */
			DBG_PRINT(level, "}");
			break;
		case '=':
			/* dirty hack. Don't mimic this. */
			if (peekc(0) == '=') {
				throwaway_nextchar();
			} else {
				startequal = 1;
			}
			break;
		case ';':
			startthrow = startequal = 0;
			break;
		case '\n':
			if (startmacro && level != savelevel) {
				if (wflag)
					warning("different level before and after #define macro. reseted. [+%d %s].", lineno, curfile);
				level = savelevel;
			}
			startmacro = startsharp = 0;
			break;
		/*
		 * #xxx
		 */
		case SHARP_DEFINE:
		case SHARP_UNDEF:
			startmacro = 1;
			savelevel = level;
			if ((c = nexttoken(interested, cpp_reserved_word)) != SYMBOL) {
				pushbacktoken();
				break;
			}
			if (peekc(1) == '('/* ) */) {
				if (target == DEF)
					PUT(token, lineno, sp);
				while ((c = nexttoken("()", cpp_reserved_word)) != EOF && c != '\n' && c != /* ( */ ')')
					if (c == SYMBOL && target == SYM)
						PUT(token, lineno, sp);
				if (c == '\n')
					pushbacktoken();
			}  else {
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
			while ((c = nexttoken(interested, cpp_reserved_word)) != EOF && c != '\n')
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
			(void)nexttoken(interested, cpp_reserved_word);
			break;
		case CPP_NEW:
			if ((c = nexttoken(interested, cpp_reserved_word)) == SYMBOL)
				if (target == REF && defined(token))
					PUT(token, lineno, sp);
			break;
		case CPP_STRUCT:
		case CPP_ENUM:
		case CPP_UNION:
			c = nexttoken(interested, cpp_reserved_word);
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
				c = nexttoken(interested, cpp_reserved_word);
			}
			if (c == '{' /* } */ && cc == CPP_ENUM) {
				int savelevel = level;

				for (; c != EOF; c = nexttoken(interested, cpp_reserved_word)) {
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
		case CPP_TEMPLATE:
			{
				int level = 0;

				while ((c = nexttoken("<>", cpp_reserved_word)) != EOF) {
					if (c == '<')
						++level;
					else if (c == '>') {
						if (--level == 0)
							break;
					} else if (c == SYMBOL) {
						if (target == SYM)
							PUT(token, lineno, sp);
					}
				}
				if (c == EOF && wflag)
					warning("template <...> isn't closed. [+%d %s].", lineno, curfile);
			}
			break;
		case CPP_OPERATOR:
			while ((c = nexttoken(";{", /* } */ cpp_reserved_word)) != EOF) {
				if (c == '{') /* } */ {
					pushbacktoken();
					break;
				} else if (c == ';') {
					break;
				} else if (c == SYMBOL) {
					if (target == SYM)
						PUT(token, lineno, sp);
				}
			}
			if (c == EOF && wflag)
				warning("'{' doesn't exist after 'operator'. [+%d %s].", lineno, curfile); /* } */
			break;
		/* control statement check */
		case CPP_THROW:
			startthrow = 1;
		case CPP_BREAK:
		case CPP_CASE:
		case CPP_CATCH:
		case CPP_CONTINUE:
		case CPP_DEFAULT:
		case CPP_DELETE:
		case CPP_DO:
		case CPP_ELSE:
		case CPP_FOR:
		case CPP_GOTO:
		case CPP_IF:
		case CPP_RETURN:
		case CPP_SWITCH:
		case CPP_TRY:
		case CPP_WHILE:
			if (wflag && !startmacro && level == 0)
				warning("Out of function. %8s [+%d %s]", token, lineno, curfile);
			break;
		case CPP_TYPEDEF:
			{
				/*
				 * This parser is too complex to maintain.
				 * We should rewrite the whole.
				 */
				char savetok[MAXTOKEN];
				int savelineno = 0;
				int typedef_savelevel = level;

				savetok[0] = 0;

				/* skip CV qualifiers */
				do {
					c = nexttoken("{}(),;", cpp_reserved_word);
				} while (IS_CV_QUALIFIER(c) || c == '\n');

				if (wflag && c == EOF) {
					warning("unexpected eof. [+%d %s]", lineno, curfile);
					break;
				} else if (c == CPP_ENUM || c == CPP_STRUCT || c == CPP_UNION) {
					char *interest_enum = "{},;";
					int c_ = c;

					c = nexttoken(interest_enum, cpp_reserved_word);
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
						c = nexttoken(interest_enum, cpp_reserved_word);
					}
					for (; c != EOF; c = nexttoken(interest_enum, cpp_reserved_word)) {
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
							if (c_ == CPP_ENUM) {
								if (target == DEF && level > typedef_savelevel)
									PUT(token, lineno, sp);
								if (target == SYM && level == typedef_savelevel)
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
				while ((c = nexttoken("(),;", cpp_reserved_word)) != EOF) {
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
		case CPP___ATTRIBUTE__:
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
	while ((c = nexttoken("()", cpp_reserved_word)) != EOF) {
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
 *	r)	target type
 */
static int
function_definition(int target)
{
	int c;
	int brace_level;

	brace_level = 0;
	while ((c = nexttoken("()", cpp_reserved_word)) != EOF) {
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
	if (peekc(0) == ';') {
		(void)nexttoken(";", NULL);
		return 0;
	}
	brace_level = 0;
	while ((c = nexttoken(",;[](){}=", cpp_reserved_word)) != EOF) {
		switch (c) {
		case SHARP_IFDEF:
		case SHARP_IFNDEF:
		case SHARP_IF:
		case SHARP_ELIF:
		case SHARP_ELSE:
		case SHARP_ENDIF:
			condition_macro(c, target);
			continue;
		case CPP___ATTRIBUTE__:
			process_attribute(target);
			continue;
		default:
			break;
		}
		if (c == '('/* ) */ || c == '[')
			brace_level++;
		else if (c == /* ( */')' || c == ']')
			brace_level--;
		else if (brace_level == 0 && (c == ';' || c == ','))
			break;
		else if (c == '{' /* } */) {
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
	cur = &pifstack[piflevel];
	if (cc == SHARP_IFDEF || cc == SHARP_IFNDEF || cc == SHARP_IF) {
		DBG_PRINT(piflevel, "#if");
		if (++piflevel >= MAXPIFSTACK)
			die("#if pifstack over flow. [%s]", curfile);
		++cur;
		cur->start = level;
		cur->end = -1;
		cur->if0only = 0;
		if (peekc(0) == '0')
			cur->if0only = 1;
		else if ((cc = nexttoken(NULL, cpp_reserved_word)) == SYMBOL && !strcmp(token, "notdef"))
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
	while ((cc = nexttoken(NULL, cpp_reserved_word)) != EOF && cc != '\n') {
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
