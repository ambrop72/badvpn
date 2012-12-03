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
#include <misc/concat_strings.h>
#include <ncd/NCDAst.h>

struct parser_out {
    int out_of_memory;
    int syntax_error;
    int have_ast;
    NCDProgram ast;
};

struct token {
    char *str;
    size_t len;
};

struct program {
    int have;
    NCDProgram v;
};

struct block {
    int have;
    NCDBlock v;
};

struct statement {
    int have;
    NCDStatement v;
};

struct ifblock {
    int have;
    NCDIfBlock v;
};

struct value {
    int have;
    NCDValue v;
};

static void free_token (struct token o) { free(o.str); }
static void free_program (struct program o) { if (o.have) NCDProgram_Free(&o.v); }
static void free_block (struct block o) { if (o.have) NCDBlock_Free(&o.v); }
static void free_statement (struct statement o) { if (o.have) NCDStatement_Free(&o.v); }
static void free_ifblock (struct ifblock o) { if (o.have) NCDIfBlock_Free(&o.v); }
static void free_value (struct value o) { if (o.have) NCDValue_Free(&o.v); }

#line 62 "NCDConfigParser_parse.c"
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
#define YYNOCODE 40
#define YYACTIONTYPE unsigned char
#define ParseTOKENTYPE  struct token 
typedef union {
  int yyinit;
  ParseTOKENTYPE yy0;
  struct statement yy5;
  char * yy7;
  struct value yy13;
  struct program yy18;
  struct block yy21;
  int yy46;
  struct ifblock yy74;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 0
#endif
#define ParseARG_SDECL  struct parser_out *parser_out ;
#define ParseARG_PDECL , struct parser_out *parser_out 
#define ParseARG_FETCH  struct parser_out *parser_out  = yypParser->parser_out 
#define ParseARG_STORE yypParser->parser_out  = parser_out 
#define YYNSTATE 96
#define YYNRULE 37
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
 /*     0 */    83,   37,   77,   84,   38,   85,   40,   83,   46,   77,
 /*    10 */    84,   66,   85,   40,   75,   82,   39,    3,   79,   83,
 /*    20 */    33,   41,   84,   32,   85,   40,   39,   82,   39,    3,
 /*    30 */    83,    4,   70,   84,   44,   85,   42,   34,  134,   72,
 /*    40 */    73,   76,   47,    4,   86,   83,   34,   78,   84,   83,
 /*    50 */    85,   40,   84,   81,   85,   42,   82,   39,    3,   83,
 /*    60 */    25,   94,   84,   83,   85,   43,   84,   39,   85,   49,
 /*    70 */    45,   53,    4,   83,   48,   54,   84,   83,   85,   55,
 /*    80 */    84,   15,   85,   67,   15,   36,   30,   15,   51,   30,
 /*    90 */    15,   65,   30,   15,   58,   30,   71,   91,   30,   15,
 /*   100 */    95,   15,   35,   63,   30,   69,   30,   57,    1,   26,
 /*   110 */    34,   23,   52,   60,   59,   64,   96,   18,   11,   21,
 /*   120 */    22,   74,   19,    5,   80,    6,   87,    2,   88,   24,
 /*   130 */     8,    7,   50,   12,   89,   20,   13,    9,   31,   56,
 /*   140 */    14,   90,   27,   61,   28,   62,   16,  135,   93,   92,
 /*   150 */    10,   17,   68,  135,   29,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    29,   30,   31,   32,   36,   34,   35,   29,   30,   31,
 /*    10 */    32,   14,   34,   35,    3,    2,    3,    4,    5,   29,
 /*    20 */     1,   31,   32,   23,   34,   35,    3,    2,    3,    4,
 /*    30 */    29,   18,   23,   32,   33,   34,   35,   37,   38,   20,
 /*    40 */    21,   29,   36,   18,   19,   29,   37,   31,   32,   29,
 /*    50 */    34,   35,   32,   33,   34,   35,    2,    3,    4,   29,
 /*    60 */    25,   26,   32,   29,   34,   35,   32,    3,   34,   35,
 /*    70 */    29,   15,   18,   29,   10,   11,   32,   29,   34,   35,
 /*    80 */    32,   24,   34,   35,   24,   28,   29,   24,   28,   29,
 /*    90 */    24,   28,   29,   24,   28,   29,   23,   28,   29,   24,
 /*   100 */    26,   24,    3,   28,   29,   28,   29,    7,    6,   27,
 /*   110 */    37,    9,   36,   13,   36,   36,    0,    2,    4,    7,
 /*   120 */    16,    8,    5,   17,    5,   17,   19,    6,    8,    7,
 /*   130 */     6,   13,    7,    4,    8,    5,    4,    6,    3,   12,
 /*   140 */     4,    8,    5,    3,    5,    7,    4,   39,    5,    8,
 /*   150 */     6,    4,    7,   39,    5,
};
#define YY_SHIFT_USE_DFLT (-4)
#define YY_SHIFT_MAX 69
static const short yy_shift_ofst[] = {
 /*     0 */    19,   54,   54,   13,   25,   54,   54,   54,   54,   54,
 /*    10 */    54,   64,   64,   64,   64,   64,   64,   64,   19,   19,
 /*    20 */    -3,   11,   23,   23,   11,   56,   11,   11,   11,   -3,
 /*    30 */   102,  100,  116,  115,   99,  114,  117,  112,  113,  104,
 /*    40 */   106,  119,  118,  108,  107,  121,  122,  120,  124,  125,
 /*    50 */   129,  130,  126,  132,  131,  127,  135,  136,  137,  133,
 /*    60 */   140,  138,  142,  139,  141,  143,  144,  145,  147,  149,
};
#define YY_REDUCE_USE_DFLT (-33)
#define YY_REDUCE_MAX 29
static const signed char yy_reduce_ofst[] = {
 /*     0 */     0,  -29,  -22,  -10,    1,   16,   20,   30,   34,   44,
 /*    10 */    48,   57,   60,   63,   66,   69,   75,   77,    9,   73,
 /*    20 */    35,  -32,   12,   41,    6,   82,   76,   78,   79,   74,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */    97,  115,  115,  133,  133,  133,  133,  133,  133,  133,
 /*    10 */   133,  133,  133,  133,  133,  111,  133,  133,   97,   97,
 /*    20 */   105,  129,  133,  133,  129,  109,  129,  129,  129,  107,
 /*    30 */   133,  133,  133,  133,  133,  133,  133,  133,  133,  113,
 /*    40 */   117,  133,  133,  121,  133,  133,  133,  133,  133,  133,
 /*    50 */   133,  133,  133,  133,  133,  133,  133,  133,  133,  133,
 /*    60 */   133,  133,  133,  133,  133,  133,  133,  133,  133,  133,
 /*    70 */    98,   99,  131,  132,  100,  130,  114,  116,  118,  119,
 /*    80 */   120,  122,  125,  126,  127,  128,  123,  124,  101,  102,
 /*    90 */   103,  112,  104,  110,  106,  108,
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
  "$",             "INCLUDE",       "STRING",        "NAME",        
  "CURLY_OPEN",    "CURLY_CLOSE",   "ROUND_OPEN",    "ROUND_CLOSE", 
  "SEMICOLON",     "ARROW",         "IF",            "FOREACH",     
  "AS",            "COLON",         "ELIF",          "ELSE",        
  "DOT",           "COMMA",         "BRACKET_OPEN",  "BRACKET_CLOSE",
  "PROCESS",       "TEMPLATE",      "error",         "processes",   
  "statement",     "elif_maybe",    "elif",          "else_maybe",  
  "statements",    "dotted_name",   "statement_args_maybe",  "list_contents",
  "list",          "map_contents",  "map",           "value",       
  "name_maybe",    "process_or_template",  "input",       
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "input ::= processes",
 /*   1 */ "processes ::=",
 /*   2 */ "processes ::= INCLUDE STRING processes",
 /*   3 */ "processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE processes",
 /*   4 */ "statement ::= dotted_name ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON",
 /*   5 */ "statement ::= dotted_name ARROW dotted_name ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON",
 /*   6 */ "statement ::= IF ROUND_OPEN value ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE elif_maybe else_maybe name_maybe SEMICOLON",
 /*   7 */ "statement ::= FOREACH ROUND_OPEN value AS NAME ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE name_maybe SEMICOLON",
 /*   8 */ "statement ::= FOREACH ROUND_OPEN value AS NAME COLON NAME ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE name_maybe SEMICOLON",
 /*   9 */ "elif_maybe ::=",
 /*  10 */ "elif_maybe ::= elif",
 /*  11 */ "elif ::= ELIF ROUND_OPEN value ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE",
 /*  12 */ "elif ::= ELIF ROUND_OPEN value ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE elif",
 /*  13 */ "else_maybe ::=",
 /*  14 */ "else_maybe ::= ELSE CURLY_OPEN statements CURLY_CLOSE",
 /*  15 */ "statements ::= statement",
 /*  16 */ "statements ::= statement statements",
 /*  17 */ "dotted_name ::= NAME",
 /*  18 */ "dotted_name ::= NAME DOT dotted_name",
 /*  19 */ "statement_args_maybe ::=",
 /*  20 */ "statement_args_maybe ::= list_contents",
 /*  21 */ "list_contents ::= value",
 /*  22 */ "list_contents ::= value COMMA list_contents",
 /*  23 */ "list ::= CURLY_OPEN CURLY_CLOSE",
 /*  24 */ "list ::= CURLY_OPEN list_contents CURLY_CLOSE",
 /*  25 */ "map_contents ::= value COLON value",
 /*  26 */ "map_contents ::= value COLON value COMMA map_contents",
 /*  27 */ "map ::= BRACKET_OPEN BRACKET_CLOSE",
 /*  28 */ "map ::= BRACKET_OPEN map_contents BRACKET_CLOSE",
 /*  29 */ "value ::= STRING",
 /*  30 */ "value ::= dotted_name",
 /*  31 */ "value ::= list",
 /*  32 */ "value ::= map",
 /*  33 */ "name_maybe ::=",
 /*  34 */ "name_maybe ::= NAME",
 /*  35 */ "process_or_template ::= PROCESS",
 /*  36 */ "process_or_template ::= TEMPLATE",
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
    case 1: /* INCLUDE */
    case 2: /* STRING */
    case 3: /* NAME */
    case 4: /* CURLY_OPEN */
    case 5: /* CURLY_CLOSE */
    case 6: /* ROUND_OPEN */
    case 7: /* ROUND_CLOSE */
    case 8: /* SEMICOLON */
    case 9: /* ARROW */
    case 10: /* IF */
    case 11: /* FOREACH */
    case 12: /* AS */
    case 13: /* COLON */
    case 14: /* ELIF */
    case 15: /* ELSE */
    case 16: /* DOT */
    case 17: /* COMMA */
    case 18: /* BRACKET_OPEN */
    case 19: /* BRACKET_CLOSE */
    case 20: /* PROCESS */
    case 21: /* TEMPLATE */
{
#line 89 "NCDConfigParser_parse.y"
 free_token((yypminor->yy0)); 
#line 519 "NCDConfigParser_parse.c"
}
      break;
    case 23: /* processes */
{
#line 108 "NCDConfigParser_parse.y"
 (void)parser_out; free_program((yypminor->yy18)); 
#line 526 "NCDConfigParser_parse.c"
}
      break;
    case 24: /* statement */
{
#line 109 "NCDConfigParser_parse.y"
 free_statement((yypminor->yy5)); 
#line 533 "NCDConfigParser_parse.c"
}
      break;
    case 25: /* elif_maybe */
    case 26: /* elif */
{
#line 110 "NCDConfigParser_parse.y"
 free_ifblock((yypminor->yy74)); 
#line 541 "NCDConfigParser_parse.c"
}
      break;
    case 27: /* else_maybe */
    case 28: /* statements */
{
#line 112 "NCDConfigParser_parse.y"
 free_block((yypminor->yy21)); 
#line 549 "NCDConfigParser_parse.c"
}
      break;
    case 29: /* dotted_name */
    case 36: /* name_maybe */
{
#line 114 "NCDConfigParser_parse.y"
 free((yypminor->yy7)); 
#line 557 "NCDConfigParser_parse.c"
}
      break;
    case 30: /* statement_args_maybe */
    case 31: /* list_contents */
    case 32: /* list */
    case 33: /* map_contents */
    case 34: /* map */
    case 35: /* value */
{
#line 115 "NCDConfigParser_parse.y"
 free_value((yypminor->yy13)); 
#line 569 "NCDConfigParser_parse.c"
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
#line 130 "NCDConfigParser_parse.y"

    if (yypMinor) {
        free_token(yypMinor->yy0);
    }
#line 747 "NCDConfigParser_parse.c"
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
  { 38, 1 },
  { 23, 0 },
  { 23, 3 },
  { 23, 6 },
  { 24, 6 },
  { 24, 8 },
  { 24, 11 },
  { 24, 11 },
  { 24, 13 },
  { 25, 0 },
  { 25, 1 },
  { 26, 7 },
  { 26, 8 },
  { 27, 0 },
  { 27, 4 },
  { 28, 1 },
  { 28, 2 },
  { 29, 1 },
  { 29, 3 },
  { 30, 0 },
  { 30, 1 },
  { 31, 1 },
  { 31, 3 },
  { 32, 2 },
  { 32, 3 },
  { 33, 3 },
  { 33, 5 },
  { 34, 2 },
  { 34, 3 },
  { 35, 1 },
  { 35, 1 },
  { 35, 1 },
  { 35, 1 },
  { 36, 0 },
  { 36, 1 },
  { 37, 1 },
  { 37, 1 },
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
#line 136 "NCDConfigParser_parse.y"
{
    ASSERT(!parser_out->have_ast)

    if (yymsp[0].minor.yy18.have) {
        parser_out->have_ast = 1;
        parser_out->ast = yymsp[0].minor.yy18.v;
    }
}
#line 905 "NCDConfigParser_parse.c"
        break;
      case 1: /* processes ::= */
#line 145 "NCDConfigParser_parse.y"
{
    NCDProgram prog;
    NCDProgram_Init(&prog);
    
    yygotominor.yy18.have = 1;
    yygotominor.yy18.v = prog;
}
#line 916 "NCDConfigParser_parse.c"
        break;
      case 2: /* processes ::= INCLUDE STRING processes */
#line 153 "NCDConfigParser_parse.y"
{
    ASSERT(yymsp[-1].minor.yy0.str)
    if (!yymsp[0].minor.yy18.have) {
        goto failA0;
    }
    
    NCDProgramElem elem;
    if (!NCDProgramElem_InitInclude(&elem, yymsp[-1].minor.yy0.str, yymsp[-1].minor.yy0.len)) {
        goto failA0;
    }
    
    if (!NCDProgram_PrependElem(&yymsp[0].minor.yy18.v, elem)) {
        goto failA1;
    }
    
    yygotominor.yy18.have = 1;
    yygotominor.yy18.v = yymsp[0].minor.yy18.v;
    yymsp[0].minor.yy18.have = 0;
    goto doneA;

failA1:
    NCDProgramElem_Free(&elem);
failA0:
    yygotominor.yy18.have = 0;
    parser_out->out_of_memory = 1;
doneA:
    free_token(yymsp[-1].minor.yy0);
    free_program(yymsp[0].minor.yy18);
  yy_destructor(yypParser,1,&yymsp[-2].minor);
}
#line 950 "NCDConfigParser_parse.c"
        break;
      case 3: /* processes ::= process_or_template NAME CURLY_OPEN statements CURLY_CLOSE processes */
#line 183 "NCDConfigParser_parse.y"
{
    ASSERT(yymsp[-4].minor.yy0.str)
    if (!yymsp[-2].minor.yy21.have || !yymsp[0].minor.yy18.have) {
        goto failB0;
    }

    NCDProcess proc;
    if (!NCDProcess_Init(&proc, yymsp[-5].minor.yy46, yymsp[-4].minor.yy0.str, yymsp[-2].minor.yy21.v)) {
        goto failB0;
    }
    yymsp[-2].minor.yy21.have = 0;
    
    NCDProgramElem elem;
    NCDProgramElem_InitProcess(&elem, proc);

    if (!NCDProgram_PrependElem(&yymsp[0].minor.yy18.v, elem)) {
        goto failB1;
    }

    yygotominor.yy18.have = 1;
    yygotominor.yy18.v = yymsp[0].minor.yy18.v;
    yymsp[0].minor.yy18.have = 0;
    goto doneB;

failB1:
    NCDProgramElem_Free(&elem);
failB0:
    yygotominor.yy18.have = 0;
    parser_out->out_of_memory = 1;
doneB:
    free_token(yymsp[-4].minor.yy0);
    free_block(yymsp[-2].minor.yy21);
    free_program(yymsp[0].minor.yy18);
  yy_destructor(yypParser,4,&yymsp[-3].minor);
  yy_destructor(yypParser,5,&yymsp[-1].minor);
}
#line 990 "NCDConfigParser_parse.c"
        break;
      case 4: /* statement ::= dotted_name ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON */
#line 218 "NCDConfigParser_parse.y"
{
    if (!yymsp[-5].minor.yy7 || !yymsp[-3].minor.yy13.have) {
        goto failC0;
    }

    if (!NCDStatement_InitReg(&yygotominor.yy5.v, yymsp[-1].minor.yy7, NULL, yymsp[-5].minor.yy7, yymsp[-3].minor.yy13.v)) {
        goto failC0;
    }
    yymsp[-3].minor.yy13.have = 0;

    yygotominor.yy5.have = 1;
    goto doneC;

failC0:
    yygotominor.yy5.have = 0;
    parser_out->out_of_memory = 1;
doneC:
    free(yymsp[-5].minor.yy7);
    free_value(yymsp[-3].minor.yy13);
    free(yymsp[-1].minor.yy7);
  yy_destructor(yypParser,6,&yymsp[-4].minor);
  yy_destructor(yypParser,7,&yymsp[-2].minor);
  yy_destructor(yypParser,8,&yymsp[0].minor);
}
#line 1018 "NCDConfigParser_parse.c"
        break;
      case 5: /* statement ::= dotted_name ARROW dotted_name ROUND_OPEN statement_args_maybe ROUND_CLOSE name_maybe SEMICOLON */
#line 240 "NCDConfigParser_parse.y"
{
    if (!yymsp[-7].minor.yy7 || !yymsp[-5].minor.yy7 || !yymsp[-3].minor.yy13.have) {
        goto failD0;
    }

    if (!NCDStatement_InitReg(&yygotominor.yy5.v, yymsp[-1].minor.yy7, yymsp[-7].minor.yy7, yymsp[-5].minor.yy7, yymsp[-3].minor.yy13.v)) {
        goto failD0;
    }
    yymsp[-3].minor.yy13.have = 0;

    yygotominor.yy5.have = 1;
    goto doneD;

failD0:
    yygotominor.yy5.have = 0;
    parser_out->out_of_memory = 1;
doneD:
    free(yymsp[-7].minor.yy7);
    free(yymsp[-5].minor.yy7);
    free_value(yymsp[-3].minor.yy13);
    free(yymsp[-1].minor.yy7);
  yy_destructor(yypParser,9,&yymsp[-6].minor);
  yy_destructor(yypParser,6,&yymsp[-4].minor);
  yy_destructor(yypParser,7,&yymsp[-2].minor);
  yy_destructor(yypParser,8,&yymsp[0].minor);
}
#line 1048 "NCDConfigParser_parse.c"
        break;
      case 6: /* statement ::= IF ROUND_OPEN value ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE elif_maybe else_maybe name_maybe SEMICOLON */
#line 263 "NCDConfigParser_parse.y"
{
    if (!yymsp[-8].minor.yy13.have || !yymsp[-5].minor.yy21.have || !yymsp[-3].minor.yy74.have) {
        goto failE0;
    }

    NCDIf ifc;
    NCDIf_Init(&ifc, yymsp[-8].minor.yy13.v, yymsp[-5].minor.yy21.v);
    yymsp[-8].minor.yy13.have = 0;
    yymsp[-5].minor.yy21.have = 0;

    if (!NCDIfBlock_PrependIf(&yymsp[-3].minor.yy74.v, ifc)) {
        NCDIf_Free(&ifc);
        goto failE0;
    }

    if (!NCDStatement_InitIf(&yygotominor.yy5.v, yymsp[-1].minor.yy7, yymsp[-3].minor.yy74.v)) {
        goto failE0;
    }
    yymsp[-3].minor.yy74.have = 0;

    if (yymsp[-2].minor.yy21.have) {
        NCDStatement_IfAddElse(&yygotominor.yy5.v, yymsp[-2].minor.yy21.v);
        yymsp[-2].minor.yy21.have = 0;
    }

    yygotominor.yy5.have = 1;
    goto doneE;

failE0:
    yygotominor.yy5.have = 0;
    parser_out->out_of_memory = 1;
doneE:
    free_value(yymsp[-8].minor.yy13);
    free_block(yymsp[-5].minor.yy21);
    free_ifblock(yymsp[-3].minor.yy74);
    free_block(yymsp[-2].minor.yy21);
    free(yymsp[-1].minor.yy7);
  yy_destructor(yypParser,10,&yymsp[-10].minor);
  yy_destructor(yypParser,6,&yymsp[-9].minor);
  yy_destructor(yypParser,7,&yymsp[-7].minor);
  yy_destructor(yypParser,4,&yymsp[-6].minor);
  yy_destructor(yypParser,5,&yymsp[-4].minor);
  yy_destructor(yypParser,8,&yymsp[0].minor);
}
#line 1096 "NCDConfigParser_parse.c"
        break;
      case 7: /* statement ::= FOREACH ROUND_OPEN value AS NAME ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE name_maybe SEMICOLON */
#line 302 "NCDConfigParser_parse.y"
{
    if (!yymsp[-8].minor.yy13.have || !yymsp[-6].minor.yy0.str || !yymsp[-3].minor.yy21.have) {
        goto failEA0;
    }
    
    if (!NCDStatement_InitForeach(&yygotominor.yy5.v, yymsp[-1].minor.yy7, yymsp[-8].minor.yy13.v, yymsp[-6].minor.yy0.str, NULL, yymsp[-3].minor.yy21.v)) {
        goto failEA0;
    }
    yymsp[-8].minor.yy13.have = 0;
    yymsp[-3].minor.yy21.have = 0;
    
    yygotominor.yy5.have = 1;
    goto doneEA0;
    
failEA0:
    yygotominor.yy5.have = 0;
    parser_out->out_of_memory = 1;
doneEA0:
    free_value(yymsp[-8].minor.yy13);
    free_token(yymsp[-6].minor.yy0);
    free_block(yymsp[-3].minor.yy21);
    free(yymsp[-1].minor.yy7);
  yy_destructor(yypParser,11,&yymsp[-10].minor);
  yy_destructor(yypParser,6,&yymsp[-9].minor);
  yy_destructor(yypParser,12,&yymsp[-7].minor);
  yy_destructor(yypParser,7,&yymsp[-5].minor);
  yy_destructor(yypParser,4,&yymsp[-4].minor);
  yy_destructor(yypParser,5,&yymsp[-2].minor);
  yy_destructor(yypParser,8,&yymsp[0].minor);
}
#line 1130 "NCDConfigParser_parse.c"
        break;
      case 8: /* statement ::= FOREACH ROUND_OPEN value AS NAME COLON NAME ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE name_maybe SEMICOLON */
#line 326 "NCDConfigParser_parse.y"
{
    if (!yymsp[-10].minor.yy13.have || !yymsp[-8].minor.yy0.str || !yymsp[-6].minor.yy0.str || !yymsp[-3].minor.yy21.have) {
        goto failEB0;
    }
    
    if (!NCDStatement_InitForeach(&yygotominor.yy5.v, yymsp[-1].minor.yy7, yymsp[-10].minor.yy13.v, yymsp[-8].minor.yy0.str, yymsp[-6].minor.yy0.str, yymsp[-3].minor.yy21.v)) {
        goto failEB0;
    }
    yymsp[-10].minor.yy13.have = 0;
    yymsp[-3].minor.yy21.have = 0;
    
    yygotominor.yy5.have = 1;
    goto doneEB0;
    
failEB0:
    yygotominor.yy5.have = 0;
    parser_out->out_of_memory = 1;
doneEB0:
    free_value(yymsp[-10].minor.yy13);
    free_token(yymsp[-8].minor.yy0);
    free_token(yymsp[-6].minor.yy0);
    free_block(yymsp[-3].minor.yy21);
    free(yymsp[-1].minor.yy7);
  yy_destructor(yypParser,11,&yymsp[-12].minor);
  yy_destructor(yypParser,6,&yymsp[-11].minor);
  yy_destructor(yypParser,12,&yymsp[-9].minor);
  yy_destructor(yypParser,13,&yymsp[-7].minor);
  yy_destructor(yypParser,7,&yymsp[-5].minor);
  yy_destructor(yypParser,4,&yymsp[-4].minor);
  yy_destructor(yypParser,5,&yymsp[-2].minor);
  yy_destructor(yypParser,8,&yymsp[0].minor);
}
#line 1166 "NCDConfigParser_parse.c"
        break;
      case 9: /* elif_maybe ::= */
#line 351 "NCDConfigParser_parse.y"
{
    NCDIfBlock_Init(&yygotominor.yy74.v);
    yygotominor.yy74.have = 1;
}
#line 1174 "NCDConfigParser_parse.c"
        break;
      case 10: /* elif_maybe ::= elif */
#line 356 "NCDConfigParser_parse.y"
{
    yygotominor.yy74 = yymsp[0].minor.yy74;
}
#line 1181 "NCDConfigParser_parse.c"
        break;
      case 11: /* elif ::= ELIF ROUND_OPEN value ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE */
#line 360 "NCDConfigParser_parse.y"
{
    if (!yymsp[-4].minor.yy13.have || !yymsp[-1].minor.yy21.have) {
        goto failF0;
    }

    NCDIfBlock_Init(&yygotominor.yy74.v);

    NCDIf ifc;
    NCDIf_Init(&ifc, yymsp[-4].minor.yy13.v, yymsp[-1].minor.yy21.v);
    yymsp[-4].minor.yy13.have = 0;
    yymsp[-1].minor.yy21.have = 0;

    if (!NCDIfBlock_PrependIf(&yygotominor.yy74.v, ifc)) {
        goto failF1;
    }

    yygotominor.yy74.have = 1;
    goto doneF0;

failF1:
    NCDIf_Free(&ifc);
    NCDIfBlock_Free(&yygotominor.yy74.v);
failF0:
    yygotominor.yy74.have = 0;
    parser_out->out_of_memory = 1;
doneF0:
    free_value(yymsp[-4].minor.yy13);
    free_block(yymsp[-1].minor.yy21);
  yy_destructor(yypParser,14,&yymsp[-6].minor);
  yy_destructor(yypParser,6,&yymsp[-5].minor);
  yy_destructor(yypParser,7,&yymsp[-3].minor);
  yy_destructor(yypParser,4,&yymsp[-2].minor);
  yy_destructor(yypParser,5,&yymsp[0].minor);
}
#line 1219 "NCDConfigParser_parse.c"
        break;
      case 12: /* elif ::= ELIF ROUND_OPEN value ROUND_CLOSE CURLY_OPEN statements CURLY_CLOSE elif */
#line 390 "NCDConfigParser_parse.y"
{
    if (!yymsp[-5].minor.yy13.have || !yymsp[-2].minor.yy21.have || !yymsp[0].minor.yy74.have) {
        goto failG0;
    }

    NCDIf ifc;
    NCDIf_Init(&ifc, yymsp[-5].minor.yy13.v, yymsp[-2].minor.yy21.v);
    yymsp[-5].minor.yy13.have = 0;
    yymsp[-2].minor.yy21.have = 0;

    if (!NCDIfBlock_PrependIf(&yymsp[0].minor.yy74.v, ifc)) {
        goto failG1;
    }

    yygotominor.yy74.have = 1;
    yygotominor.yy74.v = yymsp[0].minor.yy74.v;
    yymsp[0].minor.yy74.have = 0;
    goto doneG0;

failG1:
    NCDIf_Free(&ifc);
failG0:
    yygotominor.yy74.have = 0;
    parser_out->out_of_memory = 1;
doneG0:
    free_value(yymsp[-5].minor.yy13);
    free_block(yymsp[-2].minor.yy21);
    free_ifblock(yymsp[0].minor.yy74);
  yy_destructor(yypParser,14,&yymsp[-7].minor);
  yy_destructor(yypParser,6,&yymsp[-6].minor);
  yy_destructor(yypParser,7,&yymsp[-4].minor);
  yy_destructor(yypParser,4,&yymsp[-3].minor);
  yy_destructor(yypParser,5,&yymsp[-1].minor);
}
#line 1257 "NCDConfigParser_parse.c"
        break;
      case 13: /* else_maybe ::= */
#line 420 "NCDConfigParser_parse.y"
{
    yygotominor.yy21.have = 0;
}
#line 1264 "NCDConfigParser_parse.c"
        break;
      case 14: /* else_maybe ::= ELSE CURLY_OPEN statements CURLY_CLOSE */
#line 424 "NCDConfigParser_parse.y"
{
    yygotominor.yy21 = yymsp[-1].minor.yy21;
  yy_destructor(yypParser,15,&yymsp[-3].minor);
  yy_destructor(yypParser,4,&yymsp[-2].minor);
  yy_destructor(yypParser,5,&yymsp[0].minor);
}
#line 1274 "NCDConfigParser_parse.c"
        break;
      case 15: /* statements ::= statement */
#line 428 "NCDConfigParser_parse.y"
{
    if (!yymsp[0].minor.yy5.have) {
        goto failH0;
    }

    NCDBlock_Init(&yygotominor.yy21.v);

    if (!NCDBlock_PrependStatement(&yygotominor.yy21.v, yymsp[0].minor.yy5.v)) {
        goto failH1;
    }
    yymsp[0].minor.yy5.have = 0;

    yygotominor.yy21.have = 1;
    goto doneH;

failH1:
    NCDBlock_Free(&yygotominor.yy21.v);
failH0:
    yygotominor.yy21.have = 0;
    parser_out->out_of_memory = 1;
doneH:
    free_statement(yymsp[0].minor.yy5);
}
#line 1301 "NCDConfigParser_parse.c"
        break;
      case 16: /* statements ::= statement statements */
#line 452 "NCDConfigParser_parse.y"
{
    if (!yymsp[-1].minor.yy5.have || !yymsp[0].minor.yy21.have) {
        goto failI0;
    }

    if (!NCDBlock_PrependStatement(&yymsp[0].minor.yy21.v, yymsp[-1].minor.yy5.v)) {
        goto failI1;
    }
    yymsp[-1].minor.yy5.have = 0;

    yygotominor.yy21.have = 1;
    yygotominor.yy21.v = yymsp[0].minor.yy21.v;
    yymsp[0].minor.yy21.have = 0;
    goto doneI;

failI1:
    NCDBlock_Free(&yygotominor.yy21.v);
failI0:
    yygotominor.yy21.have = 0;
    parser_out->out_of_memory = 1;
doneI:
    free_statement(yymsp[-1].minor.yy5);
    free_block(yymsp[0].minor.yy21);
}
#line 1329 "NCDConfigParser_parse.c"
        break;
      case 17: /* dotted_name ::= NAME */
      case 34: /* name_maybe ::= NAME */ yytestcase(yyruleno==34);
#line 477 "NCDConfigParser_parse.y"
{
    ASSERT(yymsp[0].minor.yy0.str)

    yygotominor.yy7 = yymsp[0].minor.yy0.str;
}
#line 1339 "NCDConfigParser_parse.c"
        break;
      case 18: /* dotted_name ::= NAME DOT dotted_name */
#line 483 "NCDConfigParser_parse.y"
{
    ASSERT(yymsp[-2].minor.yy0.str)
    if (!yymsp[0].minor.yy7) {
        goto failJ0;
    }

    if (!(yygotominor.yy7 = concat_strings(3, yymsp[-2].minor.yy0.str, ".", yymsp[0].minor.yy7))) {
        goto failJ0;
    }

    goto doneJ;

failJ0:
    yygotominor.yy7 = NULL;
    parser_out->out_of_memory = 1;
doneJ:
    free_token(yymsp[-2].minor.yy0);
    free(yymsp[0].minor.yy7);
  yy_destructor(yypParser,16,&yymsp[-1].minor);
}
#line 1363 "NCDConfigParser_parse.c"
        break;
      case 19: /* statement_args_maybe ::= */
#line 503 "NCDConfigParser_parse.y"
{
    yygotominor.yy13.have = 1;
    NCDValue_InitList(&yygotominor.yy13.v);
}
#line 1371 "NCDConfigParser_parse.c"
        break;
      case 20: /* statement_args_maybe ::= list_contents */
      case 31: /* value ::= list */ yytestcase(yyruleno==31);
      case 32: /* value ::= map */ yytestcase(yyruleno==32);
#line 508 "NCDConfigParser_parse.y"
{
    yygotominor.yy13 = yymsp[0].minor.yy13;
}
#line 1380 "NCDConfigParser_parse.c"
        break;
      case 21: /* list_contents ::= value */
#line 512 "NCDConfigParser_parse.y"
{
    if (!yymsp[0].minor.yy13.have) {
        goto failL0;
    }

    NCDValue_InitList(&yygotominor.yy13.v);

    if (!NCDValue_ListPrepend(&yygotominor.yy13.v, yymsp[0].minor.yy13.v)) {
        goto failL1;
    }
    yymsp[0].minor.yy13.have = 0;

    yygotominor.yy13.have = 1;
    goto doneL;

failL1:
    NCDValue_Free(&yygotominor.yy13.v);
failL0:
    yygotominor.yy13.have = 0;
    parser_out->out_of_memory = 1;
doneL:
    free_value(yymsp[0].minor.yy13);
}
#line 1407 "NCDConfigParser_parse.c"
        break;
      case 22: /* list_contents ::= value COMMA list_contents */
#line 536 "NCDConfigParser_parse.y"
{
    if (!yymsp[-2].minor.yy13.have || !yymsp[0].minor.yy13.have) {
        goto failM0;
    }

    if (!NCDValue_ListPrepend(&yymsp[0].minor.yy13.v, yymsp[-2].minor.yy13.v)) {
        goto failM0;
    }
    yymsp[-2].minor.yy13.have = 0;

    yygotominor.yy13.have = 1;
    yygotominor.yy13.v = yymsp[0].minor.yy13.v;
    yymsp[0].minor.yy13.have = 0;
    goto doneM;

failM0:
    yygotominor.yy13.have = 0;
    parser_out->out_of_memory = 1;
doneM:
    free_value(yymsp[-2].minor.yy13);
    free_value(yymsp[0].minor.yy13);
  yy_destructor(yypParser,17,&yymsp[-1].minor);
}
#line 1434 "NCDConfigParser_parse.c"
        break;
      case 23: /* list ::= CURLY_OPEN CURLY_CLOSE */
#line 559 "NCDConfigParser_parse.y"
{
    yygotominor.yy13.have = 1;
    NCDValue_InitList(&yygotominor.yy13.v);
  yy_destructor(yypParser,4,&yymsp[-1].minor);
  yy_destructor(yypParser,5,&yymsp[0].minor);
}
#line 1444 "NCDConfigParser_parse.c"
        break;
      case 24: /* list ::= CURLY_OPEN list_contents CURLY_CLOSE */
#line 564 "NCDConfigParser_parse.y"
{
    yygotominor.yy13 = yymsp[-1].minor.yy13;
  yy_destructor(yypParser,4,&yymsp[-2].minor);
  yy_destructor(yypParser,5,&yymsp[0].minor);
}
#line 1453 "NCDConfigParser_parse.c"
        break;
      case 25: /* map_contents ::= value COLON value */
#line 568 "NCDConfigParser_parse.y"
{
    if (!yymsp[-2].minor.yy13.have || !yymsp[0].minor.yy13.have) {
        goto failS0;
    }

    NCDValue_InitMap(&yygotominor.yy13.v);

    if (!NCDValue_MapPrepend(&yygotominor.yy13.v, yymsp[-2].minor.yy13.v, yymsp[0].minor.yy13.v)) {
        goto failS1;
    }
    yymsp[-2].minor.yy13.have = 0;
    yymsp[0].minor.yy13.have = 0;

    yygotominor.yy13.have = 1;
    goto doneS;

failS1:
    NCDValue_Free(&yygotominor.yy13.v);
failS0:
    yygotominor.yy13.have = 0;
    parser_out->out_of_memory = 1;
doneS:
    free_value(yymsp[-2].minor.yy13);
    free_value(yymsp[0].minor.yy13);
  yy_destructor(yypParser,13,&yymsp[-1].minor);
}
#line 1483 "NCDConfigParser_parse.c"
        break;
      case 26: /* map_contents ::= value COLON value COMMA map_contents */
#line 594 "NCDConfigParser_parse.y"
{
    if (!yymsp[-4].minor.yy13.have || !yymsp[-2].minor.yy13.have || !yymsp[0].minor.yy13.have) {
        goto failT0;
    }

    if (!NCDValue_MapPrepend(&yymsp[0].minor.yy13.v, yymsp[-4].minor.yy13.v, yymsp[-2].minor.yy13.v)) {
        goto failT0;
    }
    yymsp[-4].minor.yy13.have = 0;
    yymsp[-2].minor.yy13.have = 0;

    yygotominor.yy13.have = 1;
    yygotominor.yy13.v = yymsp[0].minor.yy13.v;
    yymsp[0].minor.yy13.have = 0;
    goto doneT;

failT0:
    yygotominor.yy13.have = 0;
    parser_out->out_of_memory = 1;
doneT:
    free_value(yymsp[-4].minor.yy13);
    free_value(yymsp[-2].minor.yy13);
    free_value(yymsp[0].minor.yy13);
  yy_destructor(yypParser,13,&yymsp[-3].minor);
  yy_destructor(yypParser,17,&yymsp[-1].minor);
}
#line 1513 "NCDConfigParser_parse.c"
        break;
      case 27: /* map ::= BRACKET_OPEN BRACKET_CLOSE */
#line 619 "NCDConfigParser_parse.y"
{
    yygotominor.yy13.have = 1;
    NCDValue_InitMap(&yygotominor.yy13.v);
  yy_destructor(yypParser,18,&yymsp[-1].minor);
  yy_destructor(yypParser,19,&yymsp[0].minor);
}
#line 1523 "NCDConfigParser_parse.c"
        break;
      case 28: /* map ::= BRACKET_OPEN map_contents BRACKET_CLOSE */
#line 624 "NCDConfigParser_parse.y"
{
    yygotominor.yy13 = yymsp[-1].minor.yy13;
  yy_destructor(yypParser,18,&yymsp[-2].minor);
  yy_destructor(yypParser,19,&yymsp[0].minor);
}
#line 1532 "NCDConfigParser_parse.c"
        break;
      case 29: /* value ::= STRING */
#line 628 "NCDConfigParser_parse.y"
{
    ASSERT(yymsp[0].minor.yy0.str)

    if (!NCDValue_InitStringBin(&yygotominor.yy13.v, (uint8_t *)yymsp[0].minor.yy0.str, yymsp[0].minor.yy0.len)) {
        goto failU0;
    }

    yygotominor.yy13.have = 1;
    goto doneU;

failU0:
    yygotominor.yy13.have = 0;
    parser_out->out_of_memory = 1;
doneU:
    free_token(yymsp[0].minor.yy0);
}
#line 1552 "NCDConfigParser_parse.c"
        break;
      case 30: /* value ::= dotted_name */
#line 645 "NCDConfigParser_parse.y"
{
    if (!yymsp[0].minor.yy7) {
        goto failV0;
    }

    if (!NCDValue_InitVar(&yygotominor.yy13.v, yymsp[0].minor.yy7)) {
        goto failV0;
    }

    yygotominor.yy13.have = 1;
    goto doneV;

failV0:
    yygotominor.yy13.have = 0;
    parser_out->out_of_memory = 1;
doneV:
    free(yymsp[0].minor.yy7);
}
#line 1574 "NCDConfigParser_parse.c"
        break;
      case 33: /* name_maybe ::= */
#line 672 "NCDConfigParser_parse.y"
{
    yygotominor.yy7 = NULL;
}
#line 1581 "NCDConfigParser_parse.c"
        break;
      case 35: /* process_or_template ::= PROCESS */
#line 682 "NCDConfigParser_parse.y"
{
    yygotominor.yy46 = 0;
  yy_destructor(yypParser,20,&yymsp[0].minor);
}
#line 1589 "NCDConfigParser_parse.c"
        break;
      case 36: /* process_or_template ::= TEMPLATE */
#line 686 "NCDConfigParser_parse.y"
{
    yygotominor.yy46 = 1;
  yy_destructor(yypParser,21,&yymsp[0].minor);
}
#line 1597 "NCDConfigParser_parse.c"
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
#line 125 "NCDConfigParser_parse.y"

    parser_out->syntax_error = 1;
#line 1662 "NCDConfigParser_parse.c"
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
