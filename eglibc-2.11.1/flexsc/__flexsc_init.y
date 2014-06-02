%{
#include <stdio.h>
#include "flexsc.h"
#include "assert.h"

extern int yylex(void);

void yyerror(const char *error);

static int cpulist[MAX_CPUINFO_SIZE];
static int cpulist_index;

extern void open_cpuinfo(int list[], int size);

#define YYSTYPE int

%}

%start program

%token INT

%%

program                :   '[' cpugroup ']'
                       ;

cpugroup               :   cpugroup ':' cpulist
                       |   cpulist
                       ;

intlist                :   intlist INT                      {
                                                                if (cpulist_index == MAX_CPUINFO_SIZE) {
                                                                    flexsc_panic("too many cpus");
                                                                }
                                                                cpulist[cpulist_index ++] = (int)$2;
                                                            }
                       |   INT                              {
                                                                cpulist_index = 0;
                                                                cpulist[cpulist_index ++] = (int)$1;
                                                            }
                       ;

cpulist                :   intlist                          {
                                                                open_cpuinfo(cpulist, cpulist_index);
                                                            }
                       |   error                            {
                                                                flexsc_panic("unknown error\n");
                                                            }
                       ;

%%

void
yyerror(const char *error) {
    flexsc_panic("yyerror: `%s'\n", error);
}

void
flexsc_init_parse(const char *filename) {
    extern FILE *yyin;
    if ((yyin = fopen(filename, "rb")) == NULL) {
        flexsc_panic("cannot open file `%s'\n", filename);
    }
    yyparse();
    fclose(yyin);
}

