/* DEFINITIONS SECTION */
/* Everything between %{ ... %} is copied verbatim to the start of the parser generated C code. */

%{
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "cmpiutil.h"
#include "cmpimacs.h"			/* Contains CMSetProperty() */

#define RC_OK 0
#define RC_EOF EOF

/* DEFINE ANY GLOBAL VARS HERE */
static const CMPIBroker * _BROKER;
static CMPIInstance ** _INSTANCE;	/* The current instance that is being read into */
#ifdef DBG_PARSER
char tmpfilename[L_tmpnam];
FILE *fd;
#endif

extern int Xen_SettingDatayylex(void);
extern void Xen_SettingDatayyerror(char *);

%}

/* List all possible CIM property types that can be returned by the lexer */
/* Note - we override the CIM definition of string to make this data type
   easier to handle in the lexer/parser. Instead implemented as simple text string. */
%union {
   char *		string;
   CMPIBoolean boolean;
   CMPISint64	sint64;
   CMPIReal64  real64;
}

/* DEFINE SIMPLE (UNTYPED) LEXICAL TOKENS */
%token INSTANCE OF ENDOFFILE NULLTOK

/* DEFINE LEXICAL TOKENS THAT RETURN A VALUE AND THEIR RETURN TYPE (OBTAINED FROM THE %union ABOVE) */
%token <string> CLASSNAME
%token <string> PROPERTYNAME
%token <string> STRING
%token <boolean> BOOLEAN
%token <sint64> INTEGER
%token <real64> REAL

/* END OF DEFINITIONS SECTION */
%%

/* RULES SECTION */
/* DESCRIBE THE SYNTAX OF EACH INSTANCE ENTRY IN THE SOURCE FILE */

instance:	/* empty */
	|	INSTANCE OF CLASSNAME '{'
			{
#ifdef DBG_PARSER
			fprintf(fd,"classname = %s\n",$3);
#endif
         CMPIStatus status = {CMPI_RC_OK, NULL};
			*_INSTANCE = _CMNewInstance(_BROKER, "root/cimv2", $3, &status);
			free($3);
         if ((*_INSTANCE == NULL) || (status.rc != CMPI_RC_OK)){
            YYABORT;
         }
			}
		properties '}' ';'
			{
			/* Return after reading in each instance */
#ifdef DBG_PARSER
         fprintf(fd,"end of class\n",$3);
#endif
			return RC_OK;
			}
	|  ENDOFFILE { return RC_EOF; }
	;

properties:	/* empty */
	|	ENDOFFILE
	|	property properties
	;

property:	PROPERTYNAME '=' STRING ';'
		{
#ifdef DBG_PARSER
			fprintf(fd,"propertyname = %s\n",$1);
			fprintf(fd,"\ttype = CMPI_chars\n");
			fprintf(fd,"\tvalue = --%s--\n",$3);
#endif
         CMPIData tmpprop = CMGetProperty(*_INSTANCE, $1, 0);
         /* check we are setting the right type for the property */
         if((tmpprop.type & CMPI_string) || (tmpprop.type & CMPI_chars))
         {
            if(CMIsArray(tmpprop))
            {
               /* Handle string array properties */
               tmpprop.value.array = CMNewArray(_BROKER, 1, CMPI_chars, 0);
               CMSetArrayElementAt(tmpprop.value.array, 0, $3, CMPI_chars);
               CMSetProperty( *_INSTANCE, $1, &tmpprop.value, CMPI_charsA);
            }
            else
               CMSetProperty( *_INSTANCE, $1, $3, CMPI_chars);
         }
			free($1); free($3);
      }
	|	PROPERTYNAME '=' INTEGER ';'
		{
#ifdef DBG_PARSER
			fprintf(fd,"propertyname = %s\n",$1);
			fprintf(fd,"\ttype = CMPI_sint64\n");
			fprintf(fd,"\tvalue = %lld\n",$3);
#endif
         CMPIData tmpprop = CMGetProperty(*_INSTANCE, $1, 0);

         /* A real value could be masquareding as an integer */
         if(tmpprop.type & CMPI_REAL)
         {
            double value = (double) $3;
            if(CMIsArray(tmpprop))
            {
               /* Handle string array properties */
               tmpprop.value.array = CMNewArray(_BROKER, 1, CMPI_real64, 0);
               CMSetArrayElementAt(tmpprop.value.array, 0, &value, CMPI_real64);
               CMSetProperty( *_INSTANCE, $1, &tmpprop.value, CMPI_real64A);
            }
            else
               CMSetProperty( *_INSTANCE, $1, &(value), CMPI_real64 );
         }
         else if(tmpprop.type & CMPI_INTEGER){
            unsigned long long value = $3;
            if(CMIsArray(tmpprop))
            {
               /* Handle string array properties */
               tmpprop.value.array = CMNewArray(_BROKER, 1, CMPI_uint64, 0);
               CMSetArrayElementAt(tmpprop.value.array, 0, &value, CMPI_uint64);
               CMSetProperty( *_INSTANCE, $1, &tmpprop.value, CMPI_uint64A);
            }
            else
               CMSetProperty( *_INSTANCE, $1, &(value), CMPI_uint64 );
         }
			free($1);
		}
	|	PROPERTYNAME '=' BOOLEAN ';'
      {
#ifdef DBG_PARSER
			fprintf(fd,"propertyname = %s\n",$1);
			fprintf(fd,"\ttype = CMPI_boolean\n");
			fprintf(fd,"\tvalue = %d\n",$3);
#endif
         CMPIData tmpprop = CMGetProperty(*_INSTANCE, $1, 0);
         bool value = $3;
         if(tmpprop.type & CMPI_boolean) {
            if(CMIsArray(tmpprop))
            {
               /* Handle string array properties */
               tmpprop.value.array = CMNewArray(_BROKER, 1, CMPI_boolean, 0);
               CMSetArrayElementAt(tmpprop.value.array, 0, &value, CMPI_boolean);
               CMSetProperty( *_INSTANCE, $1, &tmpprop.value, CMPI_booleanA);
            }
            else
               CMSetProperty( *_INSTANCE, $1, &(value), CMPI_boolean );
         }
			free($1);
      }
   |	PROPERTYNAME '=' REAL ';'
	   {
#ifdef DBG_PARSER
			fprintf(fd,"propertyname = %s\n",$1);
			fprintf(fd,"\ttype = CMPI_real64\n");
			fprintf(fd,"\tvalue = %f\n",$3);
#endif
         CMPIData tmpprop = CMGetProperty(*_INSTANCE, $1, 0);
         double value = $3;
         if(tmpprop.type & CMPI_REAL)
         {
            if(CMIsArray(tmpprop))
            {
               tmpprop.value.array = CMNewArray(_BROKER, 1, CMPI_real64, 0);
               CMSetArrayElementAt(tmpprop.value.array, 0, &value, CMPI_real64);
               CMSetProperty( *_INSTANCE, $1, &tmpprop.value, CMPI_real64A);
            }
            else
               CMSetProperty( *_INSTANCE, $1, &(value), CMPI_real64 );
         }
			free($1);
     }
   | PROPERTYNAME '=' '{' STRING '}' ';'
     {
#ifdef DBG_PARSER
			fprintf(fd,"propertyname = %s\n",$1);
			fprintf(fd,"\ttype = CMPI_stringA\n");
			fprintf(fd,"\tvalue = ----%s----\n", $4);
#endif
         CMPIData tmpprop = CMGetProperty(*_INSTANCE, $1, 0);
         if((tmpprop.type & CMPI_string)||(tmpprop.type & CMPI_chars))
         {
            if(CMIsArray(tmpprop))
            {
               /* Handle string array properties */
               tmpprop.value.array = CMNewArray(_BROKER, 1, CMPI_chars, 0);
               CMSetArrayElementAt(tmpprop.value.array, 0, $4, CMPI_chars);
               CMSetProperty( *_INSTANCE, $1, &tmpprop.value, CMPI_charsA);
            }
            else
               CMSetProperty( *_INSTANCE, $1, $4, CMPI_chars);
         }
			free($1); free($4);
     }
   | PROPERTYNAME '=' NULLTOK ';'
     {

     }
	;

/* END OF RULES SECTION */
%%

/* USER SUBROUTINE SECTION */
int Xen_SettingDatayyparseinstance( const CMPIBroker * broker, CMPIInstance ** instance )
{
   _BROKER = broker;
   _INSTANCE = instance;

#ifdef DBG_PARSER
   tmpnam(tmpfilename);
   fd = fopen(tmpfilename, "w");
#endif
   /* Parse the next instance */
   int val = Xen_SettingDatayyparse();
#ifdef DBG_PARSER
   fclose(fd);
#endif
   return val;
}

