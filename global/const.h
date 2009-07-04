/* This file is generated automatically by convert.pl from global/manual.in. */
const char *progname = "global";
const char *usage_const = "Usage: global [-aGilnqrstTvx][-e] pattern\n\
       global -c[qrsv] prefix\n\
       global -f[anqrstvx] files\n\
       global -g[aGilnoOqtvx][-e] pattern\n\
       global -I[ailnqtvx][-e] pattern\n\
       global -P[aGilnoOqtvx][-e] pattern\n\
       global -p[qrv]\n\
       global -u[qv]\n";
const char *help_const = "Commands:\n\
<no command> pattern\n\
       Print object which match to the pattern.\n\
       Extended regular expressions which are the same as those\n\
       accepted by egrep(1) are available.\n\
-c, --completion [prefix]\n\
       Print the candidates of object names which start with the specified\n\
       prefix. Prefix is not specified,\n\
       print all object names.\n\
-f, --file files\n\
       Print all tags in the files.\n\
       This option implies the -x option.\n\
-g, --grep pattern\n\
       Print all lines which match to the pattern.\n\
--help\n\
       Show help.\n\
-I, --idutils pattern\n\
       Print all lines which match to the pattern.\n\
       This function use idutils(1) as a search engine.\n\
       To use this command, you need to install idutils(1)\n\
       in your system and you must execute gtags(1)\n\
       with the -I option.\n\
-P, --path [pattern]\n\
       Print the paths which match to the pattern.\n\
       If no pattern specified, print all paths in the project.\n\
-p, --print-dbpath\n\
       Print the location of GTAGS.\n\
-u, --update\n\
       Locate tag files and update them incrementally.\n\
--version\n\
       Show version number.\n\
Options:\n\
-a, --absolute\n\
       Print absolute path name. By default, print relative path name.\n\
--from-here context\n\
       Decide tag type by the context. The context must be 'lineno:path'.\n\
       If this option is specified then the -s and -r\n\
       are ignored.\n\
       Regular expression is not allowed in the pattern.\n\
       This option assumes use in conversational environments such as\n\
       editors and IDEs.\n\
-e, --regexp pattern\n\
       Use pattern as the pattern; useful to protect  patterns\n\
       beginning with -.\n\
-G, --basic-regexp\n\
       Interpret pattern as a  basic regular expression.\n\
       The default is extended regular expression.\n\
-i, --ignore-case\n\
       ignore case distinctions in pattern.\n\
-l, --local\n\
       Print just objects which exist under the current directory.\n\
-n, --nofilter\n\
       Suppress sort filter and path conversion filter.\n\
-O, --only-other\n\
       Search pattern only in other than source files like README.\n\
       This option is valid only with -g or -P command.\n\
       This option override the -o option.\n\
-o, --other\n\
       Search pattern in not only source files but also other files\n\
       like README.\n\
       This option is valid only with -g or -P command.\n\
-q, --quiet\n\
       Quiet mode.\n\
-r, --reference, --rootdir\n\
       Print the locations of object references.\n\
       By default, print object definitions.\n\
       With the -p option, print the root directory of source tree.\n\
--result format\n\
       format may be 'path', `ctags', `ctags-x', `grep' or 'cscope'.\n\
       The --result=ctags and --result=ctags-x are\n\
       equivalent to the -t and -x respectively.\n\
       The --result option is given to priority more than the -t and -x option.\n\
-s, --symbol\n\
       Print the locations of specified symbol other than definitions.\n\
-T, --through\n\
       Go through all the tag files listed in GTAGSLIBPATH.\n\
       By default, stop searching when tag is found.\n\
       This option is ignored when either -s, -r\n\
       or -l option is specified.\n\
-t, --tags\n\
       Print with standard ctags format.\n\
-v, --verbose\n\
       Verbose mode.\n\
-x, --cxref\n\
       In addition to the default output, produce the line number and\n\
       the line contents.\n\
See also:\n\
       GNU GLOBAL web site: http://www.gnu.org/software/global/\n\
";
