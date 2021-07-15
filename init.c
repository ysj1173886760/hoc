#include "hoc.h"
#include "y.tab.h"
#include <math.h>
#include <stdio.h>

static struct
{ /* Keywords */
	char *name;
	int kval;
} keywords[] = {
	"func",		FUNC,
	"return",	RETURN,
	"if",		IF,
	"else",		ELSE,
	"while",	WHILE,
	"for",		FOR,
	"print",	PRINT,
	"read",		READ,
	"global",	GLOBAL,
	0,			0,
};

static struct
{ /* Constants */
	char *name;
	double cval;
} consts[] = {
	"PI", 3.14159265358979323846,
	"E", 2.71828182845904523536,
	"GAMMA", 0.57721566490153286060, /* Euler */
	"DEG", 57.29577951308232087680,	 /* deg/radian */
	"PHI", 1.61803398874989484820,	 /* golden ratio */
	"PREC", 15,						 /* output precision */
	0, 0};

static struct
{ /* Built-ins */
	char *name;
	double (*func)(double);
} builtins[] = {
	"sin", sin,
	"cos", cos,
	"tan", tan,
	"atan", atan,
	"asin", Asin, /* checks range */
	"acos", Acos, /* checks range */
	"sinh", Sinh, /* checks range */
	"cosh", Cosh, /* checks range */
	"tanh", tanh,
	"log", Log,		/* checks range */
	"log10", Log10, /* checks range */
	"exp", Exp,		/* checks range */
	"sqrt", Sqrt,	/* checks range */
	"gamma", Gamma, /* checks range */
	"int", integer,
	"abs", fabs,
	"erf", erf,
	"erfc", erfc,
	0, 0};

static struct
{
	char *name;
	Inst inst;
} codeLookupTable[] = {
	{"call", call},
	{"xpop", xpop},
	{"objpush", objpush},
	{"varpush", varpush},
	{"whilecode", whilecode},
	{"forcode", forcode},
	{"ifcode", ifcode},
	{"ret", ret},
	{"bltin", bltin},
	{"add", add},
	{"sub", sub},
	{"mul", mul},
	{"divop", divop},
	{"mod", mod},
	{"negate", negate},
	{"gt", gt},
	{"lt", lt},
	{"ge", ge},
	{"le", le},
	{"eq", eq},
	{"ne", ne},
	{"and", and},
	{"or", or },
	{"not", not },
	{"power", power},
	{"assign", assign},
	{"addeq", addeq},
	{"subeq", subeq},
	{"muleq", muleq},
	{"diveq", diveq},
	{"modeq", modeq},
	{"printop", printtop},
	{"prexpr", prexpr},
	{"varread", varread},
	{0, 0}};

// nargs should be amend to a range of available number of arguments
static struct memberCallBuiltins {
	char *typeName;
	char *callName;
	void (*func)(void);
	int	nargs;
} memberCallTables[] = {
	{"list", "append", append, 1},
	{"list", "change", listchange, 2},
	{"number", "change", numberchange, 1},
	{0, 0, 0, 0}
};

void init(void) /* install constants and built-ins in table */
{
	int i;
	Symbol *s;
	for (i = 0; keywords[i].name; i++)
		keywordList = install(keywordList, keywords[i].name, keywords[i].kval);
	for (i = 0; consts[i].name; i++)
	{
		keywordList = install(keywordList, consts[i].name, VAR);

		Object *newObj = (Object *)emalloc(sizeof(Object));
		newObj->type = NUMBER;
		newObj->u.value = (double *)emalloc(sizeof(Object));
		*(newObj->u.value) = consts[i].cval;
		keywordList->u.objPtr = newObj;
	}
	for (i = 0; builtins[i].name; i++)
	{
		s = install(keywordList, builtins[i].name, BLTIN);
		keywordList = s;
		s->u.ptr = builtins[i].func;
	}

	for (i = 0; memberCallTables[i].typeName; i++) {
		TypeLookupEntry *cur = findTypeTable(memberCallTables[i].typeName);
		if (!cur) {
			cur = (TypeLookupEntry *) emalloc(sizeof(TypeLookupEntry));
			cur->memberTable = 0;
			cur->next = globalTypeTable;
			cur->typename = memberCallTables[i].typeName;
			globalTypeTable = cur;
		}

		MemberCallLookupEntry *newEntry = (MemberCallLookupEntry *) emalloc(sizeof(MemberCallLookupEntry));
		newEntry->name = memberCallTables->callName;
		newEntry->opr.func = memberCallTables[i].func;
		newEntry->opr.nargs = memberCallTables->nargs;

		newEntry->next = cur->memberTable;
		cur->memberTable = newEntry;
	}
}

char *getCodeThoughAddress(Inst inst)
{
	for (int i = 0; codeLookupTable[i].name; i++)
	{
		if (inst == codeLookupTable[i].inst)
			return codeLookupTable[i].name;
	}
	return 0;
}
