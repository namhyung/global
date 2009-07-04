/* This file is generated automatically by convert.pl from gtags/manual.in. */
const char *progname = "gtags";
const char *usage_const = "Usage: gtags [-ciIOqvw][-f file][-n number][dbpath]\n";
const char *help_const = "Options:\n\
-c, --compact\n\
       Make GTAGS in compact format.\n\
       This option does not influence GRTAGS and GSYMS,\n\
       because they are always made in compact format.\n\
--config[=name]\n\
       Show the value of config variable name.\n\
       If name is not specified then show whole of config entry.\n\
-f, --file file\n\
       Read from file a list of file names which should be\n\
       considered as the candidate of source files.\n\
       By default, all files under the current directory are\n\
       considered as the candidate.\n\
       If file is -, read from standard input.\n\
       File names must be separated by newline.\n\
--gtagsconf file\n\
       Load user's configuration from file.\n\
--gtagslabel label\n\
       label is used as the label of configuration file.\n\
       The default is default.\n\
-I, --idutils\n\
       Make index files for idutils(1) too.\n\
-i, --incremental\n\
       Update tag files incrementally. You had better use\n\
       global(1) with the -u option.\n\
-n, --max-args number\n\
       Maximum number of arguments for gtags-parser(1).\n\
       By default, gtags invokes gtags-parser with arguments\n\
       as many as possible to decrease the frequency of invoking.\n\
-O, --objdir\n\
       Use objdir as dbpath.\n\
       If $MAKEOBJDIRPREFIX directory exists, gtags creates\n\
       $MAKEOBJDIRPREFIX/<current directory> directory and makes\n\
       tag files in it.\n\
       If dbpath is specified, this options is ignored.\n\
-q, --quiet\n\
       Quiet mode.\n\
-v, --verbose\n\
       Verbose mode.\n\
-w, --warning\n\
       Print warning messages.\n\
dbpath\n\
       The directory in which tag files are generated.\n\
       The default is the current directory.\n\
       It is useful when your source directory is on a read only\n\
       device like CDROM.\n\
See also:\n\
       GNU GLOBAL web site: http://www.gnu.org/software/global/\n\
";
