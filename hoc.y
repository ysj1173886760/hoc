%{
#include <string.h>
#include "hoc.h"
#define	code2(c1,c2)	code(c1); code(c2)
#define	code3(c1,c2,c3)	code(c1); code(c2); code(c3)

#define InteractiveEnv 0

Symbol *globalSymbolList = 0;
Symbol *keywordList = 0;
Info *curDefiningFunction = 0;
TypeLookupEntry *globalTypeTable = 0;

// TODO: move reading debuglevel and debugflag from argument
int debugLevel = 0;
int debugFlag = hocExec | hocCompile;

void yyerror(char* s);
%}
%union {
	Symbol	*sym;	/* symbol table pointer */
	Inst	*inst;	/* machine instruction */
	Object *obj;
	long	narg;	/* number of arguments */
}
%token	<sym>	PRINT VAR BLTIN UNDEF FUNCTION
%token	<sym>	RETURN FUNC READ GLOBAL WHILE FOR IF ELSE
%token 	<obj>	NUMBER STRING LIST
%type	<inst>	expr stmt asgn prlist stmtlist
%type	<inst>	cond while for if begin end
%type	<narg>	arglist vflist valuelist
%right	'=' ADDEQ SUBEQ MULEQ DIVEQ MODEQ
%left	OR
%left	AND
%left	GT GE LT LE EQ NE
%left	'+' '-'
%left	'*' '/' '%'
%left	UNARYMINUS NOT
%right	'^'
%left 	'[' ']'
%%
list:	  /* nothing */
	| list '\n'
	| list defn '\n'
	| list asgn '\n'  { code2(xpop, STOP); return 1; }
	| list stmt '\n'  { code(STOP); return 1; } 
	| list expr '\n'  { code2(printtop, STOP); return 1; }
	| list error '\n' { yyerrok; }
	;
asgn:	  VAR '=' expr  { code3(varpush,(Inst)$1,assign); $$=$3; }
	| VAR ADDEQ expr	{ code3(varpush,(Inst)$1,addeq); $$=$3; }
	| VAR SUBEQ expr	{ code3(varpush,(Inst)$1,subeq); $$=$3; }
	| VAR MULEQ expr	{ code3(varpush,(Inst)$1,muleq); $$=$3; }
	| VAR DIVEQ expr	{ code3(varpush,(Inst)$1,diveq); $$=$3; }
	| VAR MODEQ expr	{ code3(varpush,(Inst)$1,modeq); $$=$3; }
	;
stmt:	  expr	{ code(xpop); }
	| RETURN expr
	        { defnonly("return"); $$=$2; code(ret); }
	| PRINT prlist	{ $$ = $2; }
	| while '(' cond ')' stmt end {
		($1)[1] = (Inst)$5;	/* body of loop */
		($1)[2] = (Inst)$6; }	/* end, if cond fails */
	| for '(' cond ';' cond ';' cond ')' stmt end {
		($1)[1] = (Inst)$5;	/* condition */
		($1)[2] = (Inst)$7;	/* post loop */
		($1)[3] = (Inst)$9;	/* body of loop */
		($1)[4] = (Inst)$10; }	/* end, if cond fails */
	| if '(' cond ')' stmt end {	/* else-less if */
		($1)[1] = (Inst)$5;	/* thenpart */
		($1)[3] = (Inst)$6; }	/* end, if cond fails */
	| if '(' cond ')' stmt end ELSE stmt end {	/* if with else */
		($1)[1] = (Inst)$5;	/* thenpart */
		($1)[2] = (Inst)$8;	/* elsepart */
		($1)[3] = (Inst)$9; }	/* end, if cond fails */
	| '{' stmtlist '}'	{ $$ = $2; }
	| GLOBAL VAR { code2(globalBinding, (Inst)$2); }
	| VAR '.' VAR '(' valuelist ')' { code3(oprcall, (Inst)$1, (Inst)$3); code((Inst)$5); }	
	;
cond:	   expr 	{ code(STOP); }
	;
while:	  WHILE	{ $$ = code3(whilecode,STOP,STOP); }
	;
for:	  FOR	{ $$ = code(forcode); code3(STOP,STOP,STOP); code(STOP); }
	;
if:	  IF	{ $$ = code(ifcode); code3(STOP,STOP,STOP); }
	;
begin:	  /* nothing */		{ $$ = progp; }
	;
end:	  /* nothing */		{ code(STOP); $$ = progp; }
	;
stmtlist: /* nothing */		{ $$ = progp; }
	| stmtlist '\n'
	| stmtlist stmt
	;
expr:	  NUMBER { $$ = code2(objpush, (Inst)$1); }
	| STRING { $$ = code2(objpush, (Inst)$1); }
	| VAR '[' expr ']' { $$ = code2(memberpush, (Inst)$1); }
	| '[' arglist ']' { $$ = code2(listpush, (Inst)$2); }
	| VAR	 { $$ = code2(exprpush, (Inst)$1); }
	| asgn
	| VAR begin '(' arglist ')'
		{ $$ = $2; code3(call,(Inst)$1,(Inst)$4); }
	| READ '(' VAR ')' { $$ = code2(varread, (Inst)$3); }
	| BLTIN '(' expr ')' { $$=$3; code2(bltin, (Inst)$1->u.ptr); }
	| '(' expr ')'	{ $$ = $2; }
	| expr '+' expr	{ code(add); }
	| expr '-' expr	{ code(sub); }
	| expr '*' expr	{ code(mul); }
	| expr '/' expr	{ code(divop); }	/* ansi has a div fcn! */
	| expr '%' expr	{ code(mod); }
	| expr '^' expr	{ code (power); }
	| '-' expr   %prec UNARYMINUS   { $$=$2; code(negate); }
	| expr GT expr	{ code(gt); }
	| expr GE expr	{ code(ge); }
	| expr LT expr	{ code(lt); }
	| expr LE expr	{ code(le); }
	| expr EQ expr	{ code(eq); }
	| expr NE expr	{ code(ne); }
	| expr AND expr	{ code(and); }
	| expr OR expr	{ code(or); }
	| NOT expr	{ $$ = $2; code(not); }
	;
valuelist:	/* nothing */	{ $$ = 0; }	
	/* | NUMBER { code2(constpush, (Inst)$1); $$ = 1; }
	| valuelist ',' NUMBER { code2(constpush, (Inst)$3); $$ = $1 + 1; } */
	| NUMBER { code2(objpush, (Inst)$1); $$ = 1; }
	| valuelist ',' NUMBER { code2(objpush, (Inst)$3); $$ = $1 + 1; }
prlist:	  expr			{ code(prexpr); }	/* printtop and prexpr seem to be same */
	| prlist ',' expr	{ code(prexpr); }
	;
defn:	  FUNC VAR { $2->type=VAR; defineBegin($2); }
	    '(' vflist ')' { setArg($5); } stmt { code(procret); defineEnd($2); curDefiningFunction = 0; }
	;
arglist:  /* nothing */ 	{ $$ = 0; }
	| expr			{ $$ = 1; }
	| arglist ',' expr	{ $$ = $1 + 1; }
	;
vflist : /* nothing */		{ $$ = 0; }
	| VAR { $$ = 1; }
	| vflist ',' VAR { $$ = $1 + 1; }
	;
%%
	/* end of grammar */
#include <stdio.h>
#include <ctype.h>
char	*progname;
int	lineno = 1;
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
jmp_buf	begin;
char	*infile;	/* input file name */
FILE	*fin;		/* input file pointer */
char	**gargv;	/* global argument list */
extern	errno;
int	gargc;

int c = '\n';	/* global for use by warning() */

int	backslash(int), follow(int, int, int);
void defnonly(char*), run(void);
void warning(char*, char*);

int yylex(void)		/* hoc6 */
{
	while ((c=getc(fin)) == ' ' || c == '\t')
		;
	if (c == EOF)
		return 0;
	if (c == '\\') {
		c = getc(fin);
		if (c == '\n') {
			lineno++;
			return yylex();
		}
	}
	if (c == '#') {		/* comment */
		while ((c=getc(fin)) != '\n' && c != EOF)
			;
		if (c == '\n')
			lineno++;
		return c;
	}
	// here have complex with A.opr, so I delete c == '.'
	if (isdigit(c)) {	/* number */
		double d;
		ungetc(c, fin);
		fscanf(fin, "%lf", &d);
		yylval.sym = install(globalSymbolList, "", NUMBER);

		Object *newObj = (Object *)emalloc(sizeof(Object));
		newObj->type = NUMBER;
		newObj->size = 1;
		newObj->u.value = (double *)emalloc(sizeof(Object));
		*(newObj->u.value) = d;
		yylval.obj = newObj;

		return NUMBER;
	}
	if (isalpha(c) || c == '_') {
		Symbol *s;
		char sbuf[100], *p = sbuf;
		do {
			if (p >= sbuf + sizeof(sbuf) - 1) {
				*p = '\0';
				execerror("name too long", sbuf);
			}
			*p++ = c;
		} while ((c=getc(fin)) != EOF && (isalnum(c) || c == '_'));
		ungetc(c, fin);
		*p = '\0';

		// first try the keywords
		if ((s = lookup(keywordList, sbuf)) != 0) {
			yylval.sym = s;
			return s->type;
		}
		// if we are in the definition
		if (curDefiningFunction != 0) {
			Symbol *symlist = curDefiningFunction->paras;
			if ((s=lookup(symlist, sbuf)) == 0) {
				s = install(symlist, sbuf, UNDEF);
				curDefiningFunction->paras = s;
			}
			yylval.sym = s;
			return s->type == UNDEF ? VAR : s->type;
		} else {
			if ((s=lookup(globalSymbolList, sbuf)) == 0) {
				s = install(globalSymbolList, sbuf, UNDEF);
				globalSymbolList = s;
			}
			yylval.sym = s;
			return s->type == UNDEF ? VAR : s->type;
		}
		return UNDEF;
	}
	if (c == '"') {	/* quoted string */
		char sbuf[100], *p;
		for (p = sbuf; (c=getc(fin)) != '"'; p++) {
			if (c == '\n' || c == EOF)
				execerror("missing quote", "");
			if (p >= sbuf + sizeof(sbuf) - 1) {
				*p = '\0';
				execerror("string too long", sbuf);
			}
			*p = backslash(c);
		}
		*p = 0;

		Object *newObj = (Object *)emalloc(sizeof(Object));
		newObj->type = STRING;
		newObj->size = strlen(sbuf);
		newObj->u.str = (char *)emalloc((strlen(sbuf)+1) * sizeof(char));
		strcpy(newObj->u.str, sbuf);

		yylval.obj = newObj;
		return STRING;
	}
	switch (c) {
		case '+':	return follow('=', ADDEQ, '+');
		case '-':	return follow('=', SUBEQ, '-');
		case '*':	return follow('=', MULEQ, '*');
		case '/':	return follow('=', DIVEQ, '/');
		case '%':	return follow('=', MODEQ, '%');
		case '>':	return follow('=', GE, GT);
		case '<':	return follow('=', LE, LT);
		case '=':	return follow('=', EQ, '=');
		case '!':	return follow('=', NE, NOT);
		case '|':	return follow('|', OR, '|');
		case '&':	return follow('&', AND, '&');
		case '\n':	lineno++; return '\n';
		default:	return c;
	}
}

int backslash(int c)	/* get next char with \'s interpreted */
{
	static char transtab[] = "b\bf\fn\nr\rt\t";
	if (c != '\\')
		return c;
	c = getc(fin);
	if (islower(c) && strchr(transtab, c))
		return strchr(transtab, c)[1];
	return c;
}

int follow(int expect, int ifyes, int ifno)	/* look ahead for >=, etc. */
{
	int c = getc(fin);

	if (c == expect)
		return ifyes;
	ungetc(c, fin);
	return ifno;
}

void yyerror(char* s)	/* report compile-time error */
{
/*rob
	warning(s, (char *)0);
	longjmp(begin, 0);
rob*/
	execerror(s, (char *)0);
}

void execerror(char* s, char* t)	/* recover from run-time error */
{
	warning(s, t);
	fseek(fin, 0L, 2);		/* flush rest of file */
	longjmp(begin, 0);
}

void fpecatch(int signum)	/* catch floating point exceptions */
{
	execerror("floating point exception", (char *) 0);
}

void intcatch(int signum)	/* catch interrupts */
{
	execerror("interrupt", (char *) 0);
}

void printProg(Inst *start) {
	if ((debugFlag & hocCompile) != 0 && debugLevel >= 1) {
		printf("\nPrinting the Program\n");
		for (Inst *cur = start; cur != progp; cur++) {
			printf("%ld\t%-16p\t", cur - start, *cur);

			if (*cur == 0) {
				printf("STOP\n");
				continue;
			}

			char *code = getCodeThoughAddress(*cur);
			if (code) {
				printf("%s", code);
				
				// translate the loop and branch code
				if (strcmp(code, "ifcode") == 0) {
					printf("\n");

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("then: %ld\n", (Inst *)*cur - start);

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("else: ");
					if (*cur) {
						printf("%ld\n", (Inst *)*cur - start);
					} else {
						printf("%-16p\n", *cur);
					}

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("end: %ld", (Inst *)*cur - start);

				} else if (strcmp(code, "whilecode") == 0) {
					printf("\n");

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("body of loop: %ld\n", (Inst *)*cur - start);

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("end: %ld", (Inst *)*cur - start);

				} else if (strcmp(code, "forcode") == 0) {
					printf("\n");

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("condition: %ld\n", (Inst *)*cur - start);

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("post loop: %ld\n", (Inst *)*cur - start);

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("body of loop: %ld\n", (Inst *)*cur - start);

					cur++;
					printf("%ld\t%-16p\t", cur - start, *cur);
					printf("end: %ld", (Inst *)*cur - start);

				}
			} else {
				Symbol *sp = lookupThoughAddress(globalSymbolList, (Symbol *)(*cur));
				if (sp) {
					if (strcmp(sp->name, "") == 0) {
						printf("%lf", *(sp->u.objPtr->u.value));
					} else {
						printf("%s", sp->name);
					}
				}
			}

			printf("\n");
		}
	}
}

void run(void)	/* execute until EOF */
{
	Inst *start = progbase;
	setjmp(begin);
	for (initcode(); yyparse(); initcode()) {
		printProg(progbase);
		execute(progbase);
	}
	printProg(start);
}

int main(int argc, char* argv[])	/* hoc6 */
{
#if YYDEBUG
	yydebug=3;
#endif

	progname = argv[0];
	init();

	// normal argument list, argc - 1 means we don't count the proc name, argv + 1 means we are starting from the first argument
	gargv = argv+1;
	gargc = argc-1;

	/* Interactive Env */
	if (argc == 1) {	/* fake an argument list */
		if (!InteractiveEnv) {
			fprintf(stderr, "WARNING::Disabling Interactive Environment\n");
			return 0;
		}
		static char *stdinonly[] = { "-" };

		gargv = stdinonly;
		gargc = 1;
	}
	
	while (moreinput())
		run();
	return 0;
}

int moreinput(void)
{
	if (gargc-- <= 0)
		return 0;
	if (fin && fin != stdin)
		fclose(fin);
	infile = *gargv++;
	lineno = 1;
	if (strcmp(infile, "-") == 0) {
		fin = stdin;
		infile = 0;
	} else if ((fin = fopen(infile, "r")) == NULL) {
		fprintf(stderr, "%s: can't open %s\n", progname, infile);
		return moreinput();
	}
	return 1;
}

void warning(char *s, char *t)	/* print warning message */
{
	fprintf(stderr, "%s: %s", progname, s);
	if (t)
		fprintf(stderr, " %s", t);
	if (infile)
		fprintf(stderr, " in %s", infile);
	fprintf(stderr, " near line %d\n", lineno);
	while (c != '\n' && c != EOF)
		if((c = getc(fin)) == '\n')	/* flush rest of input line */
			lineno++;
		else if (c == EOF && errno == EINTR) {
			clearerr(stdin);	/* ick! */
			errno = 0;
		}
}

void defnonly(char *s)	/* warn if illegal definition */
{
	if (curDefiningFunction == 0)
		execerror(s, "used outside definition");
}
