/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included that follows the "include" declaration
** in the input grammar file. */
#include <stdio.h>
#line 23 "NCDConfigParser_parse.y"


#include <string.h>
#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDConfig.h>

struct parser_out {
    int out_of_memory;
    int syntax_error;
    struct NCDConfig_processes *ast;
};

#line 23 "NCDConfigParser_parse.c"
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
#define YYNOCODE 24
#define YYACTIONTYPE unsigned char
#define ParseTOKENTYPE void *
typedef union {
  int yyinit;
  ParseTOKENTYPE yy0;
  int yy6;
  struct NCDConfig_strings * yy10;
  struct NCDConfig_list * yy24;
  struct NCDConfig_statements * yy34;
  struct NCDConfig_processes * yy41;
  char * yy45;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 0
#endif
#define ParseARG_SDECL struct parser_out *parser_out;
#define ParseARG_PDECL ,struct parser_out *parser_out
#define ParseARG_FETCH struct parser_out *parser_out = yypParser->parser_out
#define ParseARG_STORE yypParser->parser_out = parser_out
#define YYNSTATE 43
#define YYNRULE 23
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
 /*     0 */    24,   20,   36,   25,   24,   28,   36,   25,   22,    3,
 /*    10 */    38,   16,   22,    3,   31,   32,   23,   34,   17,   67,
 /*    20 */    23,   24,   22,   26,   25,   24,   35,   41,   25,   24,
 /*    30 */    30,   40,   25,   24,   21,   37,   25,   17,   27,   19,
 /*    40 */    15,   33,   15,   42,   15,    1,   43,   29,   13,   18,
 /*    50 */     7,    8,   12,    4,   68,   11,   68,    9,    2,   39,
 /*    60 */    68,   68,    5,    6,   14,   68,   10,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    16,   17,   18,   19,   16,   17,   18,   19,    1,    2,
 /*    10 */     3,   14,    1,    2,   11,   12,    9,   16,   21,   22,
 /*    20 */     9,   16,    1,   18,   19,   16,    1,   18,   19,   16,
 /*    30 */    14,   18,   19,   16,   20,   18,   19,   21,   16,   15,
 /*    40 */    16,   15,   16,   15,   16,    4,    0,   20,    7,    1,
 /*    50 */     3,    2,    8,   10,   23,    5,   23,    6,    4,    3,
 /*    60 */    23,   23,   10,   10,    5,   23,    6,
};
#define YY_SHIFT_USE_DFLT (-1)
#define YY_SHIFT_MAX 29
static const signed char yy_shift_ofst[] = {
 /*     0 */     3,   11,   11,    7,   11,   11,   11,    3,   21,   21,
 /*    10 */    21,   25,   21,   21,   25,   41,   46,   48,   49,   47,
 /*    20 */    50,   51,   44,   43,   52,   53,   56,   54,   59,   60,
};
#define YY_REDUCE_USE_DFLT (-17)
#define YY_REDUCE_MAX 14
static const signed char yy_reduce_ofst[] = {
 /*     0 */    -3,  -16,  -12,    5,    9,   13,   17,   16,   24,   26,
 /*    10 */    28,   14,    1,   22,   27,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */    66,   52,   52,   66,   66,   66,   66,   44,   66,   46,
 /*    10 */    48,   62,   66,   66,   62,   66,   66,   66,   66,   66,
 /*    20 */    66,   66,   50,   54,   55,   56,   66,   66,   66,   66,
 /*    30 */    45,   64,   65,   47,   51,   63,   53,   59,   60,   61,
 /*    40 */    58,   57,   49,
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
  "DOT",           "STRING",        "COMMA",         "PROCESS",     
  "TEMPLATE",      "error",         "processes",     "statements",  
  "statement_names",  "statement_args_maybe",  "list_contents",  "list",        
  "name_maybe",    "process_or_template",  "input",       
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "input ::= processes",
 /*   1 */ "processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE",
 /*   2 */ "processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE processes",
 /*   3 */ "statements ::= statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON",
 /*   4 */ "statements ::= statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON statements",
 /*   5 */ "statements ::= statement_names ARROW statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON",
 /*   6 */ "statements ::= statement_names ARROW statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON statements",
 /*   7 */ "statement_names ::= NAME",
 /*   8 */ "statement_names ::= NAME DOT statement_names",
 /*   9 */ "statement_args_maybe ::=",
 /*  10 */ "statement_args_maybe ::= list_contents",
 /*  11 */ "list_contents ::= STRING",
 /*  12 */ "list_contents ::= statement_names",
 /*  13 */ "list_contents ::= list",
 /*  14 */ "list_contents ::= STRING COMMA list_contents",
 /*  15 */ "list_contents ::= statement_names COMMA list_contents",
 /*  16 */ "list_contents ::= list COMMA list_contents",
 /*  17 */ "list ::= CURLY_OPEN CURLY_CLOSE",
 /*  18 */ "list ::= CURLY_OPEN list_contents CURLY_CLOSE",
 /*  19 */ "name_maybe ::=",
 /*  20 */ "name_maybe ::= NAME",
 /*  21 */ "process_or_template ::= PROCESS",
 /*  22 */ "process_or_template ::= TEMPLATE",
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
    case 9: /* STRING */
    case 10: /* COMMA */
    case 11: /* PROCESS */
    case 12: /* TEMPLATE */
{
#line 43 "NCDConfigParser_parse.y"
 free((yypminor->yy0)); 
#line 424 "NCDConfigParser_parse.c"
}
      break;
    case 14: /* processes */
{
#line 54 "NCDConfigParser_parse.y"
 NCDConfig_free_processes((yypminor->yy41)); 
#line 431 "NCDConfigParser_parse.c"
}
      break;
    case 15: /* statements */
{
#line 55 "NCDConfigParser_parse.y"
 NCDConfig_free_statements((yypminor->yy34)); 
#line 438 "NCDConfigParser_parse.c"
}
      break;
    case 16: /* statement_names */
{
#line 56 "NCDConfigParser_parse.y"
 NCDConfig_free_strings((yypminor->yy10)); 
#line 445 "NCDConfigParser_parse.c"
}
      break;
    case 17: /* statement_args_maybe */
    case 18: /* list_contents */
    case 19: /* list */
{
#line 57 "NCDConfigParser_parse.y"
 NCDConfig_free_list((yypminor->yy24)); 
#line 454 "NCDConfigParser_parse.c"
}
      break;
    case 20: /* name_maybe */
{
#line 60 "NCDConfigParser_parse.y"
 free((yypminor->yy45)); 
#line 461 "NCDConfigParser_parse.c"
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
#line 69 "NCDConfigParser_parse.y"

    if (yypMinor) {
        free(yypMinor->yy0);
    }
#line 639 "NCDConfigParser_parse.c"
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
  { 22, 1 },
  { 14, 5 },
  { 14, 6 },
  { 15, 6 },
  { 15, 7 },
  { 15, 8 },
  { 15, 9 },
  { 16, 1 },
  { 16, 3 },
  { 17, 0 },
  { 17, 1 },
  { 18, 1 },
  { 18, 1 },
  { 18, 1 },
  { 18, 3 },
  { 18, 3 },
  { 18, 3 },
  { 19, 2 },
  { 19, 3 },
  { 20, 0 },
  { 20, 1 },
  { 21, 1 },
  { 21, 1 },
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
#line 75 "NCDConfigParser_parse.y"
{
    parser_out->ast = yymsp[0].minor.yy41;

    if (!yymsp[0].minor.yy41) {
        parser_out->out_of_memory = 1;
    }
}
#line 782 "NCDConfigParser_parse.c"
        break;
      case 1: /* processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE */
#line 83 "NCDConfigParser_parse.y"
{
    yygotominor.yy41 = NCDConfig_make_processes(yymsp[-4].minor.yy6, yymsp[-3].minor.yy0, yymsp[-1].minor.yy34, 0, NULL);
    if (!yygotominor.yy41) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,2,&yymsp[-2].minor);
  yy_destructor(yypParser,3,&yymsp[0].minor);
}
#line 794 "NCDConfigParser_parse.c"
        break;
      case 2: /* processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE processes */
#line 90 "NCDConfigParser_parse.y"
{
    yygotominor.yy41 = NCDConfig_make_processes(yymsp[-5].minor.yy6, yymsp[-4].minor.yy0, yymsp[-2].minor.yy34, 1, yymsp[0].minor.yy41);
    if (!yygotominor.yy41) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,2,&yymsp[-3].minor);
  yy_destructor(yypParser,3,&yymsp[-1].minor);
}
#line 806 "NCDConfigParser_parse.c"
        break;
      case 3: /* statements ::= statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON */
#line 97 "NCDConfigParser_parse.y"
{
    yygotominor.yy34 = NCDConfig_make_statements(NULL, yymsp[-5].minor.yy10, yymsp[-3].minor.yy24, yymsp[-1].minor.yy45, NULL);
    if (!yygotominor.yy34) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,4,&yymsp[-4].minor);
  yy_destructor(yypParser,5,&yymsp[-2].minor);
  yy_destructor(yypParser,6,&yymsp[0].minor);
}
#line 819 "NCDConfigParser_parse.c"
        break;
      case 4: /* statements ::= statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON statements */
#line 104 "NCDConfigParser_parse.y"
{
    yygotominor.yy34 = NCDConfig_make_statements(NULL, yymsp[-6].minor.yy10, yymsp[-4].minor.yy24, yymsp[-2].minor.yy45, yymsp[0].minor.yy34);
    if (!yygotominor.yy34) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,4,&yymsp[-5].minor);
  yy_destructor(yypParser,5,&yymsp[-3].minor);
  yy_destructor(yypParser,6,&yymsp[-1].minor);
}
#line 832 "NCDConfigParser_parse.c"
        break;
      case 5: /* statements ::= statement_names ARROW statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON */
#line 111 "NCDConfigParser_parse.y"
{
    yygotominor.yy34 = NCDConfig_make_statements(yymsp[-7].minor.yy10, yymsp[-5].minor.yy10, yymsp[-3].minor.yy24, yymsp[-1].minor.yy45, NULL);
    if (!yygotominor.yy34) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,7,&yymsp[-6].minor);
  yy_destructor(yypParser,4,&yymsp[-4].minor);
  yy_destructor(yypParser,5,&yymsp[-2].minor);
  yy_destructor(yypParser,6,&yymsp[0].minor);
}
#line 846 "NCDConfigParser_parse.c"
        break;
      case 6: /* statements ::= statement_names ARROW statement_names ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON statements */
#line 118 "NCDConfigParser_parse.y"
{
    yygotominor.yy34 = NCDConfig_make_statements(yymsp[-8].minor.yy10, yymsp[-6].minor.yy10, yymsp[-4].minor.yy24, yymsp[-2].minor.yy45, yymsp[0].minor.yy34);
    if (!yygotominor.yy34) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,7,&yymsp[-7].minor);
  yy_destructor(yypParser,4,&yymsp[-5].minor);
  yy_destructor(yypParser,5,&yymsp[-3].minor);
  yy_destructor(yypParser,6,&yymsp[-1].minor);
}
#line 860 "NCDConfigParser_parse.c"
        break;
      case 7: /* statement_names ::= NAME */
#line 125 "NCDConfigParser_parse.y"
{
    yygotominor.yy10 = NCDConfig_make_strings(yymsp[0].minor.yy0, 0, NULL);
    if (!yygotominor.yy10) {
        parser_out->out_of_memory = 1;
    }
}
#line 870 "NCDConfigParser_parse.c"
        break;
      case 8: /* statement_names ::= NAME DOT statement_names */
#line 132 "NCDConfigParser_parse.y"
{
    yygotominor.yy10 = NCDConfig_make_strings(yymsp[-2].minor.yy0, 1, yymsp[0].minor.yy10);
    if (!yygotominor.yy10) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,8,&yymsp[-1].minor);
}
#line 881 "NCDConfigParser_parse.c"
        break;
      case 9: /* statement_args_maybe ::= */
#line 139 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NULL;
}
#line 888 "NCDConfigParser_parse.c"
        break;
      case 10: /* statement_args_maybe ::= list_contents */
#line 143 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = yymsp[0].minor.yy24;
}
#line 895 "NCDConfigParser_parse.c"
        break;
      case 11: /* list_contents ::= STRING */
#line 147 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NCDConfig_make_list_string(yymsp[0].minor.yy0, NULL);
    if (!yygotominor.yy24) {
        parser_out->out_of_memory = 1;
    }
}
#line 905 "NCDConfigParser_parse.c"
        break;
      case 12: /* list_contents ::= statement_names */
#line 154 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NCDConfig_make_list_var(yymsp[0].minor.yy10, NULL);
    if (!yygotominor.yy24) {
        parser_out->out_of_memory = 1;
    }
}
#line 915 "NCDConfigParser_parse.c"
        break;
      case 13: /* list_contents ::= list */
#line 161 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NCDConfig_make_list_list(yymsp[0].minor.yy24, NULL);
    if (!yygotominor.yy24) {
        parser_out->out_of_memory = 1;
    }
}
#line 925 "NCDConfigParser_parse.c"
        break;
      case 14: /* list_contents ::= STRING COMMA list_contents */
#line 168 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NCDConfig_make_list_string(yymsp[-2].minor.yy0, yymsp[0].minor.yy24);
    if (!yygotominor.yy24) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,10,&yymsp[-1].minor);
}
#line 936 "NCDConfigParser_parse.c"
        break;
      case 15: /* list_contents ::= statement_names COMMA list_contents */
#line 175 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NCDConfig_make_list_var(yymsp[-2].minor.yy10, yymsp[0].minor.yy24);
    if (!yygotominor.yy24) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,10,&yymsp[-1].minor);
}
#line 947 "NCDConfigParser_parse.c"
        break;
      case 16: /* list_contents ::= list COMMA list_contents */
#line 182 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NCDConfig_make_list_list(yymsp[-2].minor.yy24, yymsp[0].minor.yy24);
    if (!yygotominor.yy24) {
        parser_out->out_of_memory = 1;
    }
  yy_destructor(yypParser,10,&yymsp[-1].minor);
}
#line 958 "NCDConfigParser_parse.c"
        break;
      case 17: /* list ::= CURLY_OPEN CURLY_CLOSE */
#line 189 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = NULL;
  yy_destructor(yypParser,2,&yymsp[-1].minor);
  yy_destructor(yypParser,3,&yymsp[0].minor);
}
#line 967 "NCDConfigParser_parse.c"
        break;
      case 18: /* list ::= CURLY_OPEN list_contents CURLY_CLOSE */
#line 193 "NCDConfigParser_parse.y"
{
    yygotominor.yy24 = yymsp[-1].minor.yy24;
  yy_destructor(yypParser,2,&yymsp[-2].minor);
  yy_destructor(yypParser,3,&yymsp[0].minor);
}
#line 976 "NCDConfigParser_parse.c"
        break;
      case 19: /* name_maybe ::= */
#line 197 "NCDConfigParser_parse.y"
{
    yygotominor.yy45 = NULL;
}
#line 983 "NCDConfigParser_parse.c"
        break;
      case 20: /* name_maybe ::= NAME */
#line 201 "NCDConfigParser_parse.y"
{
    yygotominor.yy45 = yymsp[0].minor.yy0;
}
#line 990 "NCDConfigParser_parse.c"
        break;
      case 21: /* process_or_template ::= PROCESS */
#line 205 "NCDConfigParser_parse.y"
{
    yygotominor.yy6 = 0;
  yy_destructor(yypParser,11,&yymsp[0].minor);
}
#line 998 "NCDConfigParser_parse.c"
        break;
      case 22: /* process_or_template ::= TEMPLATE */
#line 209 "NCDConfigParser_parse.y"
{
    yygotominor.yy6 = 1;
  yy_destructor(yypParser,12,&yymsp[0].minor);
}
#line 1006 "NCDConfigParser_parse.c"
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
#line 64 "NCDConfigParser_parse.y"

    parser_out->syntax_error = 1;
#line 1071 "NCDConfigParser_parse.c"
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
