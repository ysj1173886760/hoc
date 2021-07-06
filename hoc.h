typedef void (*Inst)(void);
#define STOP (Inst)0
#define MAX 10

typedef struct Symbol
{ /* symbol table entry */
	char *name;
	long type;
	union
	{
		double val;			   /* VAR */
		double (*ptr)(double); /* BLTIN */
		Inst *defn;			   /* FUNCTION, PROCEDURE */
		char *str;			   /* STRING */
	} u;
	struct Symbol *next; /* to link to another */
} Symbol;
Symbol *install_key(char *, int, double), *lookup_key(char *);
Symbol *install_global(char *, int, double), *lookup_global(char *);

typedef struct Info
{
	char *name;	   //过函名称
	Symbol *paras; //局部变量起始位置
	int nargs;	   //传入参数的个数
	int nparas;	   //局部变量的个数
	Inst *defn;	   // code指令起始位置
} Info;
// TODO : 函数符号表的install和lookup
Info Infostack[MAX];
Info *Infostackp; // pointer to current func/proc info
Symbol *install_para(Info *, char *, int, double);
Symbol *lookup_para(Info *, char *);

typedef union Datum
{ /* interpreter stack type */
	double val;
	Symbol *sym;
} Datum;

typedef union ArgDatum
{
	double val;
	Symbol *sym;
} ArgDatum;

enum DebugFlag
{
	hocCompile = 1 << 0,
	hocExec = 1 << 1
};

extern char *getCodeThoughAddress(Inst inst);
extern Symbol *lookup_global_ThoughAddress(Symbol *p);

extern double Fgetd(int);
extern int moreinput(void);
extern void execerror(char *, char *);
extern void define(Symbol *), verify(Symbol *);
extern Datum pop(void);
extern void initcode(void), push(Datum), xpop(void), constpush(void);
extern void varpush(void);
extern void eval(void), add(void), sub(void), mul(void), divop(void), mod(void);
extern void negate(void), power(void);
extern void addeq(void), subeq(void), muleq(void), diveq(void), modeq(void);
extern void Infopush(Symbol *), getInfo_nargs(long);
extern void getVF_type(Symbol *);

extern Inst *progp, *progbase, prog[], *code(Inst);
extern void assign(void), bltin(void), varread(void);
extern void prexpr(void), prstr(void);
extern void gt(void), lt(void), eq(void), ge(void), le(void), ne(void);
extern void and (void), or (void), not(void);
extern void ifcode(void), whilecode(void), forcode(void);
extern void call(void), arg(void), argassign(void);
extern void funcret(void), procret(void);
extern void preinc(void), predec(void), postinc(void), postdec(void);
extern void argaddeq(void), argsubeq(void), argmuleq(void);
extern void argdiveq(void), argmodeq(void);
extern void execute(Inst *);
extern void printtop(void);
void vf();
double getvf(char *);
void get_curInfo(Symbol *);

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
extern void testaction(void);