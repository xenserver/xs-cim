	/*** WARNING - COMMENTS IN LEX MUST BE TAB INDENTED ***/

	/* DEFINITIONS SECTION */
	/* Everything between %{ ... %} is copied verbatim to the start of the lex generated C code. */

%{
#ifdef DBG_PARSER
extern FILE* fd;
#endif

#include "cmpidt.h"
#include "cmpitrace.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "Xen_SettingDataParser.h"

#ifndef YY_FLEX_LEX_COMPAT

#if YY_FLEX_SUBMINOR_VERSION < 35
int yylineno = 1;
#endif

#endif

%}

		/* Some useful regular expressions to use in the RULES section */
DIGIT		([0-9])
ALPHA		([A-Za-z])
ALPHANUMERIC	([A-Za-z0-9])
PUNCTUATION	([\,\.\:\(\)\-\!\"\'\`\^])
ARITHMETIC	([\+\-\*\/\<\>\=])
WHITESPACE	([\t ]+)
EOLN		(\n)
NUMBER		([0-9]+)
INTEGER		(-?[0-9]+)
REAL		(-?[0-9]+\.[0-9]+)
WORD		([A-Za-z]+)
QUOTEDTEXT	      (\"[^"\\]*(\\.[^"\\]*)*\")
SINGLEQUOTEDTEXT	(\'[^'\\]*(\\.[^'\\]*)*\')
FILENAME	([A-Za-z0-9\.][A-Za-z0-9\.\-\+\_]*)
PATHNAME	(\/?[A-Za-z0-9\.][A-Za-z0-9\.\-\+\_]*(\/[A-Za-z0-9\.][A-Za-z0-9\.\-\+\_]*)*\/?) 
HOSTNAME	([A-Za-z][A-Za-z0-9\-\_]*(\.[A-Za-z][A-Za-z0-9\-\_]*)*) 
IPADDR		([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)
BOOLTRUE (true|True|TRUE)
BOOLFALSE (false|False|False)
NULLTOK (null|Null|NULL)

	/* Special mode to read CLASSNAME after reading INSTANCE OF, otherwise cannot distinguish this token from a PROPERTYNAME */
%x READCLASSNAME

	/* Use the default yywrap() behavior */
%option  noyywrap

	/* END OF DEFINITIONS SECTION */
%%
	/* RULES SECTION */
	/* DESCRIBE EVERY LEXICAL TOKEN/PATTERN FOLLOWED BY THE APPROPRIATE ACTION.
	   TYPICALLY THE ACTION RETURNS THE TOKEN IDENTIFIER AND ITS VALUE (IN YYLVAL) */

	/* The following simple lexical tokens only return the token identifier */

instance |
Instance |
INSTANCE {
	return(INSTANCE);
	}

of |
Of |
OF 	{
   BEGIN READCLASSNAME; /* Read in classname as next token */
	return(OF);
	}

   /* property values */
   /* the following rules apply only after a property name has been read */
{NULLTOK} {
   return (NULLTOK);
   }

{BOOLTRUE} {
	yylval.boolean = 1;
#ifdef DBG_PARSER
   fprintf(fd, "found boolean %s\n", yytext);
#endif
	return(BOOLEAN);
	}

{BOOLFALSE} {
	yylval.boolean = 0;
#ifdef DBG_PARSER
   fprintf(fd, "found boolean %s\n", yytext);
#endif
	return(BOOLEAN);
	}

{INTEGER} {
	yylval.sint64 = atoi(yytext);
#ifdef DBG_PARSER
   fprintf(fd, "found integer %s\n", yytext);
#endif
	return(INTEGER);
	}

{REAL} {
   yylval.real64 = atof(yytext);
#ifdef DBG_PARSER
   fprintf(fd, "found real %s\n", yytext);
#endif
   return(REAL);
   }

{QUOTEDTEXT} |
{SINGLEQUOTEDTEXT} {
	yylval.string = (char *)strdup(yytext+1);
	yylval.string[strlen(yylval.string)-1] = '\0';
#ifdef DBG_PARSER
   fprintf(fd, "found string --%s--, yytext=%s\n", yylval.string, yytext);
#endif
	return(STRING);
	}

	/* Classname */
	/* NOTE - this rule only applies after a 'INSTANCE OF' has been read in */
<READCLASSNAME>[A-Za-z][A-Za-z0-9_]* {
	BEGIN INITIAL; /* Go back to normal parsing rules now */
	yylval.string = (char *)strdup(yytext);
#ifdef DBG_PARSER
   fprintf(fd, "found class %s\n", yytext);
#endif
	return(CLASSNAME);
	}

	/* Propertyname */
[A-Za-z][A-Za-z0-9_]* {
#ifdef DBG_PARSER
   fprintf(fd, "found property %s\n", yytext);
#endif
   yylval.string = (char *)strdup(yytext);
   return(PROPERTYNAME);
   }

	/* Ignore end-of-line */
{EOLN}	{
#ifdef DBG_PARSER
   fprintf(fd, "found eoln\n");
#endif
	yylineno++;
	}

<<EOF>> {
#ifdef DBG_PARSER
   fprintf(fd, "found EOF\n");
#endif
	return(ENDOFFILE);
	}
                                                                                                       
	/* Ignore whitespace, one for the readproperty start condition and the other for initial */
{WHITESPACE} {
#ifdef DBG_PARSER
   fprintf(fd, "found Whitespace\n");
#else
   ;
#endif
   }

	/* Return single character punctuation characters as-is, one for the readproperty start condition and the other for initial */
. {
#ifdef DBG_PARSER
   fprintf(fd, "found punctuation character %s\n", yytext);
#endif
	return(yytext[0]);
	}

	/* END OF RULES SECTION */
%%
	/* USER SUBROUTINE SECTION */
	/* Everything below is copied verbatim to the end of the lex generated C code. */

void yyerror(char *errmsg)
{
#ifdef DBG_PARSER
   fprintf(fd, "error line %d: %s in '%s'\n", yylineno, errmsg, yytext);
#endif
   fprintf(stderr, "error line %d: %s in '%s'\n", yylineno, errmsg, yytext);
}

