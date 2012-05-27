/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included that follows the "include" declaration
** in the input grammar file. */
#include <stdio.h>
#line 30 "NCDConfigParser_parse.y"


#include <string.h>
#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDConfig.h>

struct parser_minor {
    char *str;
    size_t len;
};

struct parser_out {
    int out_of_memory;
    int syntax_error;
    struct NCDConfig_processes *ast;
};

#line 28 "NCDConfigParser_parse.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    ParseTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is ParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    ParseARG_SDECL     A static variable declaration for the %extra_argument
**    ParseARG_PDECL     A parameter declaration for the %extra_argument
**    ParseARG_STORE     Code to store %extra_argument into yypParser
**    ParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned char
#define YYNOCODE 31
#define YYACTIONTYPE unsigned char
#define ParseTOKENTYPE struct parser_minor
typedef union {
  int yyinit;
  ParseTOKENTYPE yy0;
  char * yy13;
  struct NCDConfig_list * yy14;
  struct NCDConfig_strings * yy44;
  struct NCDConfig_statements * yy50;
  int yy52;
  struct NCDConfig_processes * yy57;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 0
#endif
#define ParseARG_SDECL struct parser_out *parser_out;
#define ParseARG_PDECL ,struct parser_out *parser_out
#define ParseARG_FETCH struct parser_out *parser_out = yypParser->parser_out
#define ParseARG_STORE yypParser->parser_out = parser_out
#define YYNSTATE 50
#define YYNRULE 27
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* The yyzerominor constant is used to initialize instances of
** YYMINORTYPE objects to zero. */
static const YYMINORTYPE yyzerominor = { 0 };

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    43,   20,   37,   44,   22,   45,   23,   43,   29,   37,
 /*    10 */    44,   35,   45,   23,   22,    3,   39,   43,    1,   24,
 /*    20 */    44,   13,   45,   23,    4,   16,   42,   43,   22,    3,
 /*    30 */    44,   27,   45,   25,   32,   33,   17,   78,    4,   46,
 /*    40 */    42,   43,   21,   38,   44,   43,   45,   23,   44,   41,
 /*    50 */    45,   25,   22,    3,   43,   36,   28,   44,   30,   45,
 /*    60 */    26,   31,    4,   50,   42,    9,   19,   15,    9,   49,
 /*    70 */    15,   18,   17,    8,   11,   10,   12,    7,   40,   34,
 /*    80 */    79,   79,    2,    5,   14,    6,   48,   79,   79,   47,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    20,   21,   22,   23,    1,   25,   26,   20,   21,   22,
 /*    10 */    23,    1,   25,   26,    1,    2,    3,   20,    4,   22,
 /*    20 */    23,    7,   25,   26,   11,   17,   13,   20,    1,    2,
 /*    30 */    23,   24,   25,   26,   14,   15,   28,   29,   11,   12,
 /*    40 */    13,   20,   27,   22,   23,   20,   25,   26,   23,   24,
 /*    50 */    25,   26,    1,    2,   20,   20,   20,   23,   27,   25,
 /*    60 */    26,   17,   11,    0,   13,   18,   19,   20,   18,   19,
 /*    70 */    20,    1,   28,    2,    5,    3,    8,   10,    3,    6,
 /*    80 */    30,   30,    4,    9,    5,    9,    6,   30,   30,   12,
};
#define YY_SHIFT_USE_DFLT (-1)
#define YY_SHIFT_MAX 30
static const signed char yy_shift_ofst[] = {
 /*     0 */    20,   51,   51,   13,   27,   51,   51,   51,    3,    3,
 /*    10 */    20,   10,    3,    3,   10,   14,   63,   70,   71,   72,
 /*    20 */    69,   73,   68,   74,   75,   67,   76,   77,   78,   79,
 /*    30 */    80,
};
#define YY_REDUCE_USE_DFLT (-21)
#define YY_REDUCE_MAX 14
static const signed char yy_reduce_ofst[] = {
 /*     0 */     8,  -20,  -13,   -3,    7,   21,   25,   34,   47,   50,
 /*    10 */    44,   15,   35,   36,   31,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */    77,   59,   59,   77,   77,   77,   77,   77,   77,   55,
 /*    10 */    51,   73,   77,   77,   73,   77,   77,   77,   77,   77,
 /*    20 */    77,   77,   57,   61,   77,   77,   65,   77,   77,   77,
 /*    30 */    77,   52,   75,   76,   53,   74,   58,   60,   62,   63,
 /*    40 */    64,   66,   69,   70,   71,   72,   67,   68,   54,   56,
};
#define YY_SZ_ACTTAB (int)(sizeof(yy_action)/sizeof(yy_action[0]))

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyidxMax;                 /* Maximum value of yyidx */
#endif
  int yyerrcnt;                 /* Shifts left before out of the error */
  ParseARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void ParseTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "NAME",          "CURLY_OPEN",    "CURLY_CLOSE", 
  "ROUND_OPEN",    "ROUND_CLOSE",   "SEMICOLON",     "ARROW",       
  "DOT",           "COMMA",         "COLON",         "BRACKET_OPEN",
  "BRACKET_CLOSE",  "STRING",        "PROCESS",       "TEMPLATE",    
  "error",         "processes",     "statement",     "statements",  
  "statement_names",  "statement_args_maybe",  "list_contents",  "list",        
  "map_contents",  "map",           "value",         "name_maybe",  
  "process_or_template",  "input",       
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "input ::= processes",
 /*   1 */ "processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE",
 /*   2 */ "processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE processes",
 /*   3 */ "statement ::= statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON",
 /*   4 */ "statement ::= statement_names ARROW statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON",
 /*   5 */ "statements ::= statement",
 /*   6 */ "statements ::= statement statements",
 /*   7 */ "statement_names ::= NAME",
 /*   8 */ "statement_names ::= NAME DOT statement_names",
 /*   9 */ "statement_args_maybe ::=",
 /*  10 */ "statement_args_maybe ::= list_contents",
 /*  11 */ "list_contents ::= value",
 /*  12 */ "list_contents ::= value COMMA list_contents",
 /*  13 */ "list ::= CURLY_OPEN CURLY_CLOSE",
 /*  14 */ "list ::= CURLY_OPEN list_contents CURLY_CLOSE",
 /*  15 */ "map_contents ::= value COLON value",
 /*  16 */ "map_contents ::= value COLON value COMMA map_contents",
 /*  17 */ "map ::= BRACKET_OPEN BRACKET_CLOSE",
 /*  18 */ "map ::= BRACKET_OPEN map_contents BRACKET_CLOSE",
 /*  19 */ "value ::= STRING",
 /*  20 */ "value ::= statement_names",
 /*  21 */ "value ::= list",
 /*  22 */ "value ::= map",
 /*  23 */ "name_maybe ::=",
 /*  24 */ "name_maybe ::= NAME",
 /*  25 */ "process_or_template ::= PROCESS",
 /*  26 */ "process_or_template ::= TEMPLATE",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.
*/
static void yyGrowStack(yyParser *p){
  int newSize;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  if( pNew ){
    p->yystack = pNew;
    p->yystksz = newSize;
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows to %d entries!\n",
              yyTracePrompt, p->yystksz);
    }
#endif
  }
}
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to Parse and ParseFree.
*/
void *ParseAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyidxMax = 0;
#endif
#if YYSTACKDEPTH<=0
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    yyGrowStack(pParser);
#endif
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  ParseARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
      /* TERMINAL Destructor */
    case 1: /* NAME */
    case 2: /* CURLY_OPEN */
    case 3: /* CURLY_CLOSE */
    case 4: /* ROUND_OPEN */
    case 5: /* ROUND_CLOSE */
    case 6: /* SEMICOLON */
    case 7: /* ARROW */
    case 8: /* DOT */
    case 9: /* COMMA */
    case 10: /* COLON */
    case 11: /* BRACKET_OPEN */
    case 12: /* BRACKET_CLOSE */
    case 13: /* STRING */
    case 14: /* PROCESS */
    case 15: /* TEMPLATE */
{
#line 55 "NCDConfigParser_parse.y"
 free((yypminor->yy0).str); 
#line 443 "NCDConfigParser_parse.c"
}
      break;
    case 17: /* processes */
{
#line 70 "NCDConfigParser_parse.y"
 NCDConfig_free_processes((yypminor->yy57)); 
#line 450 "NCDConfigParser_parse.c"
}
      break;
    case 18: /* statement */
    case 19: /* statements */
{
#line 71 "NCDConfigParser_parse.y"
 NCDConfig_free_statements((yypminor->yy50)); 
#line 458 "NCDConfigParser_parse.c"
}
      break;
    case 20: /* statement_names */
{
#line 73 "NCDConfigParser_parse.y"
 NCDConfig_free_strings((yypminor->yy44)); 
#line 465 "NCDConfigParser_parse.c"
}
      break;
    case 21: /* statement_args_maybe */
    case 22: /* list_contents */
    case 23: /* list */
    case 24: /* map_contents */
    case 25: /* map */
    case 26: /* value */
{
#line 74 "NCDConfigParser_parse.y"
 NCDConfig_free_list((yypminor->yy14)); 
#line 477 "NCDConfigParser_parse.c"
}
      break;
    case 27: /* name_maybe */
{
#line 80 "NCDConfigParser_parse.y"
 free((yypminor->yy13)); 
#line 484 "NCDConfigParser_parse.c"
}
      break;
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos = &pParser->yystack[pParser->yyidx];

  if( pParser->yyidx<0 ) return 0;
#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor(pParser, yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from ParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void ParseFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int ParseStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyidxMax;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  if( stateno>YY_SHIFT_MAX || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
    if( iLookAhead>0 ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        return yy_find_shift_action(pParser, iFallback);
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( j>=0 && j<YY_SZ_ACTTAB && yy_lookahead[j]==YYWILDCARD ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
    }
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_MAX ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_MAX );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_SZ_ACTTAB );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser, YYMINORTYPE *yypMinor){
   ParseARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
#line 89 "NCDConfigParser_parse.y"

    if (yypMinor) {
        free(yypMinor->yy0.str);
    }
#line 662 "NCDConfigParser_parse.c"
   ParseARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer to the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( yypParser->yyidx>yypParser->yyidxMax ){
    yypParser->yyidxMax = yypParser->yyidx;
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yyidx>=YYSTACKDEPTH ){
    yyStackOverflow(yypParser, yypMinor);
    return;
  }
#else
  if( yypParser->yyidx>=yypParser->yystksz ){
    yyGrowStack(yypParser);
    if( yypParser->yyidx>=yypParser->yystksz ){
      yyStackOverflow(yypParser, yypMinor);
      return;
    }
  }
#endif
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 29, 1 },
  { 17, 5 },
  { 17, 6 },
  { 18, 6 },
  { 18, 8 },
  { 19, 1 },
  { 19, 2 },
  { 20, 1 },
  { 20, 3 },
  { 21, 0 },
  { 21, 1 },
  { 22, 1 },
  { 22, 3 },
  { 23, 2 },
  { 23, 3 },
  { 24, 3 },
  { 24, 5 },
  { 25, 2 },
  { 25, 3 },
  { 26, 1 },
  { 26, 1 },
  { 26, 1 },
  { 26, 1 },
  { 27, 0 },
  { 27, 1 },
  { 28, 1 },
  { 28, 1 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  ParseARG_FETCH;
  yymsp = &yypParser->yystack[yypParser->yyidx];
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno>=0 
        && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
      yyRuleName[yyruleno]);
  }
#endif /* NDEBUG */

  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.  
  **
  ** 2007-01-16:  The wireshark project (www.wireshark.org) reports that
  ** without this code, their parser segfaults.  I'm not sure what there
  ** parser is doing to make this happen.  This is the second bug report
  ** from wireshark this week.  Clearly they are stressing Lemon in ways
  ** that it has not been previously stressed...  (SQLite ticket #2172)
  */
  /*memset(&yygotominor, 0, sizeof(yygotominor));*/
  yygotominor = yyzerominor;


  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 0: /* input ::= processes */
#line 95 "NCDConfigParser_parse.y"
{
    parser_out->ast = yymsp[0].minor.yy57;

    if (!yymsp[0].minor.yy57) {
        parser_out->out_of_memory = 1;
    }
}
#line 809 "NCDConfigParser_parse.c"
        break;
      case 1: /* processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE */
#line 103 "NCDConfigParser_parse.y"
{
    yygotominor.yy57 = NCDConfig_make_processes(yymsp[-4].minor.yy52, yymsp[-3].minor.yy0.str, yymsp[-1].minor.yy50, NULL);
    if (!yygotominor.yy57) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,2,&yymsp[-2].minor);
  yy_destructor(yypParser,3,&yymsp[0].minor);
}
#line 821 "NCDConfigParser_parse.c"
        break;
      case 2: /* processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE processes */
#line 110 "NCDConfigParser_parse.y"
{
    yygotominor.yy57 = NCDConfig_make_processes(yymsp[-5].minor.yy52, yymsp[-4].minor.yy0.str, yymsp[-2].minor.yy50, yymsp[0].minor.yy57);
    if (!yygotominor.yy57) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,2,&yymsp[-3].minor);
  yy_destructor(yypParser,3,&yymsp[-1].minor);
}
#line 833 "NCDConfigParser_parse.c"
        break;
      case 3: /* statement ::= statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON */
#line 117 "NCDConfigParser_parse.y"
{
    yygotominor.yy50 = NCDConfig_make_statements(NULL, yymsp[-5].minor.yy44, yymsp[-3].minor.yy14, yymsp[-1].minor.yy13, NULL);
    if (!yygotominor.yy50) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,4,&yymsp[-4].minor);
  yy_destructor(yypParser,5,&yymsp[-2].minor);
  yy_destructor(yypParser,6,&yymsp[0].minor);
}
#line 846 "NCDConfigParser_parse.c"
        break;
      case 4: /* statement ::= statement_names ARROW statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON */
#line 124 "NCDConfigParser_parse.y"
{
    yygotominor.yy50 = NCDConfig_make_statements(yymsp[-7].minor.yy44, yymsp[-5].minor.yy44, yymsp[-3].minor.yy14, yymsp[-1].minor.yy13, NULL);
    if (!yygotominor.yy50) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,7,&yymsp[-6].minor);
  yy_destructor(yypParser,4,&yymsp[-4].minor);
  yy_destructor(yypParser,5,&yymsp[-2].minor);
  yy_destructor(yypParser,6,&yymsp[0].minor);
}
#line 860 "NCDConfigParser_parse.c"
        break;
      case 5: /* statements ::= statement */
#line 131 "NCDConfigParser_parse.y"
{
    yygotominor.yy50 = yymsp[0].minor.yy50;
}
#line 867 "NCDConfigParser_parse.c"
        break;
      case 6: /* statements ::= statement statements */
#line 135 "NCDConfigParser_parse.y"
{
    if (!yymsp[-1].minor.yy50) {
        NCDConfig_free_statements(yymsp[0].minor.yy50);
    } else {
        ASSERT(!yymsp[-1].minor.yy50->next)
        yymsp[-1].minor.yy50->next = yymsp[0].minor.yy50;
    }
    yygotominor.yy50 = yymsp[-1].minor.yy50;
}
#line 880 "NCDConfigParser_parse.c"
        break;
      case 7: /* statement_names ::= NAME */
#line 145 "NCDConfigParser_parse.y"
{
    yygotominor.yy44 = NCDConfig_make_strings(yymsp[0].minor.yy0.str, 0, NULL);
    if (!yygotominor.yy44) {
        parser_out->out_of_memory = 1;
    }
}
#line 890 "NCDConfigParser_parse.c"
        break;
      case 8: /* statement_names ::= NAME DOT statement_names */
#line 152 "NCDConfigParser_parse.y"
{
    yygotominor.yy44 = NCDConfig_make_strings(yymsp[-2].minor.yy0.str, 1, yymsp[0].minor.yy44);
    if (!yygotominor.yy44) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,8,&yymsp[-1].minor);
}
#line 901 "NCDConfigParser_parse.c"
        break;
      case 9: /* statement_args_maybe ::= */
#line 159 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = NULL;
}
#line 908 "NCDConfigParser_parse.c"
        break;
      case 10: /* statement_args_maybe ::= list_contents */
      case 11: /* list_contents ::= value */ yytestcase(yyruleno==11);
#line 163 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = yymsp[0].minor.yy14;
}
#line 916 "NCDConfigParser_parse.c"
        break;
      case 12: /* list_contents ::= value COMMA list_contents */
#line 171 "NCDConfigParser_parse.y"
{
    if (!yymsp[-2].minor.yy14) {
        NCDConfig_free_list(yymsp[0].minor.yy14);
    } else {
        ASSERT(!yymsp[-2].minor.yy14->next)
        yymsp[-2].minor.yy14->next = yymsp[0].minor.yy14;
    }
    yygotominor.yy14 = yymsp[-2].minor.yy14;
  yy_destructor(yypParser,9,&yymsp[-1].minor);
}
#line 930 "NCDConfigParser_parse.c"
        break;
      case 13: /* list ::= CURLY_OPEN CURLY_CLOSE */
#line 181 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = NULL;
  yy_destructor(yypParser,2,&yymsp[-1].minor);
  yy_destructor(yypParser,3,&yymsp[0].minor);
}
#line 939 "NCDConfigParser_parse.c"
        break;
      case 14: /* list ::= CURLY_OPEN list_contents CURLY_CLOSE */
#line 185 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = yymsp[-1].minor.yy14;
  yy_destructor(yypParser,2,&yymsp[-2].minor);
  yy_destructor(yypParser,3,&yymsp[0].minor);
}
#line 948 "NCDConfigParser_parse.c"
        break;
      case 15: /* map_contents ::= value COLON value */
#line 189 "NCDConfigParser_parse.y"
{
    if (!yymsp[-2].minor.yy14 || !yymsp[0].minor.yy14) {
        NCDConfig_free_list(yymsp[-2].minor.yy14);
        NCDConfig_free_list(yymsp[0].minor.yy14);
        yygotominor.yy14 = NULL;
    } else {
        ASSERT(!yymsp[-2].minor.yy14->next)
        ASSERT(!yymsp[0].minor.yy14->next)
        yymsp[-2].minor.yy14->next = yymsp[0].minor.yy14;
        yygotominor.yy14 = yymsp[-2].minor.yy14;
    }
  yy_destructor(yypParser,10,&yymsp[-1].minor);
}
#line 965 "NCDConfigParser_parse.c"
        break;
      case 16: /* map_contents ::= value COLON value COMMA map_contents */
#line 202 "NCDConfigParser_parse.y"
{
    if (!yymsp[-4].minor.yy14 || !yymsp[-2].minor.yy14) {
        NCDConfig_free_list(yymsp[-4].minor.yy14);
        NCDConfig_free_list(yymsp[-2].minor.yy14);
        NCDConfig_free_list(yymsp[0].minor.yy14);
        yygotominor.yy14 = NULL;
    } else {
        ASSERT(!yymsp[-4].minor.yy14->next)
        ASSERT(!yymsp[-2].minor.yy14->next)
        yymsp[-4].minor.yy14->next = yymsp[-2].minor.yy14;
        yymsp[-2].minor.yy14->next = yymsp[0].minor.yy14;
        yygotominor.yy14 = yymsp[-4].minor.yy14;
    }
  yy_destructor(yypParser,10,&yymsp[-3].minor);
  yy_destructor(yypParser,9,&yymsp[-1].minor);
}
#line 985 "NCDConfigParser_parse.c"
        break;
      case 17: /* map ::= BRACKET_OPEN BRACKET_CLOSE */
#line 217 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = NULL;
  yy_destructor(yypParser,11,&yymsp[-1].minor);
  yy_destructor(yypParser,12,&yymsp[0].minor);
}
#line 994 "NCDConfigParser_parse.c"
        break;
      case 18: /* map ::= BRACKET_OPEN map_contents BRACKET_CLOSE */
#line 221 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = yymsp[-1].minor.yy14;
  yy_destructor(yypParser,11,&yymsp[-2].minor);
  yy_destructor(yypParser,12,&yymsp[0].minor);
}
#line 1003 "NCDConfigParser_parse.c"
        break;
      case 19: /* value ::= STRING */
#line 225 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = NCDConfig_make_list_string(yymsp[0].minor.yy0.str, yymsp[0].minor.yy0.len, NULL);
    if (!yygotominor.yy14) {
        parser_out->out_of_memory = 1;
    }
}
#line 1013 "NCDConfigParser_parse.c"
        break;
      case 20: /* value ::= statement_names */
#line 232 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = NCDConfig_make_list_var(yymsp[0].minor.yy44, NULL);
    if (!yygotominor.yy14) {
        parser_out->out_of_memory = 1;
    }
}
#line 1023 "NCDConfigParser_parse.c"
        break;
      case 21: /* value ::= list */
#line 239 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = NCDConfig_make_list_list(yymsp[0].minor.yy14, NULL);
    if (!yygotominor.yy14) {
        parser_out->out_of_memory = 1;
    }
}
#line 1033 "NCDConfigParser_parse.c"
        break;
      case 22: /* value ::= map */
#line 246 "NCDConfigParser_parse.y"
{
    yygotominor.yy14 = NCDConfig_make_list_maplist(yymsp[0].minor.yy14, NULL);
    if (!yygotominor.yy14) {
        parser_out->out_of_memory = 1;
    }
}
#line 1043 "NCDConfigParser_parse.c"
        break;
      case 23: /* name_maybe ::= */
#line 253 "NCDConfigParser_parse.y"
{
    yygotominor.yy13 = NULL;
}
#line 1050 "NCDConfigParser_parse.c"
        break;
      case 24: /* name_maybe ::= NAME */
#line 257 "NCDConfigParser_parse.y"
{
    yygotominor.yy13 = yymsp[0].minor.yy0.str;
}
#line 1057 "NCDConfigParser_parse.c"
        break;
      case 25: /* process_or_template ::= PROCESS */
#line 261 "NCDConfigParser_parse.y"
{
    yygotominor.yy52 = 0;
  yy_destructor(yypParser,14,&yymsp[0].minor);
}
#line 1065 "NCDConfigParser_parse.c"
        break;
      case 26: /* process_or_template ::= TEMPLATE */
#line 265 "NCDConfigParser_parse.y"
{
    yygotominor.yy52 = 1;
  yy_destructor(yypParser,15,&yymsp[0].minor);
}
#line 1073 "NCDConfigParser_parse.c"
        break;
      default:
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = (YYACTIONTYPE)yyact;
      yymsp->major = (YYCODETYPE)yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else{
    assert( yyact == YYNSTATE + YYNRULE + 1 );
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  ParseARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 84 "NCDConfigParser_parse.y"

    parser_out->syntax_error = 1;
#line 1138 "NCDConfigParser_parse.c"
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "ParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void Parse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  ParseTOKENTYPE yyminor       /* The value for the token */
  ParseARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
#if YYSTACKDEPTH<=0
    if( yypParser->yystksz <=0 ){
      /*memset(&yyminorunion, 0, sizeof(yyminorunion));*/
      yyminorunion = yyzerominor;
      yyStackOverflow(yypParser, &yyminorunion);
      return;
    }
#endif
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  ParseARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact<YYNSTATE ){
      assert( !yyendofinput );  /* Impossible to shift the $ token */
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      yymajor = YYNOCODE;
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_reduce_action(
                        yypParser->yystack[yypParser->yyidx].stateno,
                        YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor,yyminorunion);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
