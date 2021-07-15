typedef void (*Inst)(void);
#define STOP (Inst)0

typedef struct Symbol Symbol;

typedef struct Info
{
	Symbol *paras; //局部变量起始位置
	int nargs;	   //传入参数的个数
	Symbol *argBegin;
	Inst *defn; // code指令起始位置
} Info;

typedef struct Object
{
	long type;
	int size;
	union
	{
		double *valuelist;
		char *str;
		Info *funcInfo;
	} u;
} Object;

typedef struct Symbol
{ /* symbol table entry */
	char *name;
	long type;
	union
	{
		Object *objPtr;		   /* VAR */
		double (*ptr)(double); /* BLTIN */
		// char *str;			   /* STRING */
	} u;
	struct Symbol *next; /* to link to another */
} Symbol;

Symbol *install(Symbol *, char *, int);
Symbol *lookup(Symbol *, char *);

typedef struct Datum
{ /* interpreter stack type */
	// double val;
	int setflag; // obj = 0, sym = 1
	union
	{
		Object *obj;
		Symbol *sym;
	} u;
} Datum;

enum DebugFlag
{
	hocCompile = 1 << 0,
	hocExec = 1 << 1
};

extern Symbol *globalSymbolList;
extern Info *curDefiningFunction;
extern Symbol *keywordList;

extern char *getCodeThoughAddress(Inst inst);
extern Symbol *lookupThoughAddress(Symbol *symList, Symbol *p);

extern double Fgetd(int);
extern int moreinput(void);
extern void execerror(char *, char *);
extern void defineBegin(Symbol *), verify(Symbol *);
extern void defineEnd(Symbol *);
extern Datum pop(void);
extern void initcode(void), push(Datum), xpop(void), objpush(void), listpush(void), memberpush(void);

extern void strpush(void), varpush(void);
extern void add(void), sub(void), mul(void), divop(void), mod(void);
extern void negate(void), power(void);
extern void addeq(void), subeq(void), muleq(void), diveq(void), modeq(void);

extern void globalBinding();
extern void setArg(int);
extern Inst *progp, *progbase, prog[], *code(Inst);
extern void assign(void), bltin(void), varread(void);
extern void prexpr(void), prstr(void);
extern void gt(void), lt(void), eq(void), ge(void), le(void), ne(void);
extern void and (void), or (void), not(void);
extern void ifcode(void), whilecode(void), forcode(void);
extern void call(void), oprcall(void);
extern void procret(void);
// extern void preinc(void), predec(void), postinc(void), postdec(void);
extern void execute(Inst *);
extern void printtop(void);

extern double Log(double), Log10(double), Gamma(double), Sqrt(double), Exp(double);
extern double Asin(double), Acos(double), Sinh(double), Cosh(double), integer(double);
extern double Pow(double, double);

extern void init(void);
extern int yylex(void);
extern int yyparse(void);
extern void execerror(char *, char *);
extern void *emalloc(unsigned);

extern void defnonly(char *);
extern void warning(char *s, char *t);

extern Datum double2Datum(double);
extern Datum str2Datum(char *);
extern double *valpop(void);
extern Symbol *parseVar(Symbol *);
extern void ret(void);
extern void valpush(void);
extern void exprpush(void);

extern void test(void);
extern void setFlag(void);
extern void printStack();
extern void init_LIST_opr();