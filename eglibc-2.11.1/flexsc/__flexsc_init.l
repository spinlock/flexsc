%{
#include <stdlib.h>
#include "flexsc.h"
#include "flexsc_init.y.h"

static int lineno = 1;

%}

NEWLINE                 (\r|\n|\r\n)
DEC_DIGIT               ([0-9])
DEC_INTEGER             ({DEC_DIGIT}+)
HEX_DIGIT               ([0-9A-Fa-f])
HEX_INTEGER             (0[xX]{HEX_DIGIT}+)
S_COMMENT               ("//"[^\r\n]*{NEWLINE})
WHITESPACE              ([ \t]+)
OPERATOR                (":"|"["|"]")

%x comment

%%

"/*"                    {   BEGIN(comment);                     }
<comment><<EOF>>        {
                            BEGIN(INITIAL);
                            flexsc_panic("untermiated comments: %d\n", lineno);
                        }
<comment>[^*\n]*        {                                       }
<comment>"*"+[^*/\n]*   {                                       }
<comment>\n             {   lineno ++;                          }
<comment>"*"+"/"        {
                            BEGIN(INITIAL);
                        }

{WHITESPACE}            {                                       }
{NEWLINE}               {   lineno ++;                          }
{S_COMMENT}             {                                       }
{OPERATOR}              {   return yytext[0];                   }
{DEC_INTEGER}           {
                            yylval = strtol(yytext, NULL, 10);
                            return INT;
                        }
{HEX_INTEGER}           {
                            yylval = strtol(yytext, NULL, 16);
                            return INT;
                        }

.                       {
                            flexsc_panic("unknown character `%c', %d\n", yytext[0], lineno);
                        }

%%

int
yywrap(void) {
    return 1;
}

