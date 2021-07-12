#include "hoc.h"
#include "y.tab.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define NSTACK 256

static Datum stack[NSTACK]; /* the stack */
static Datum *stackp;		/* next free spot on stack */

#define NPROG 2000
Inst prog[NPROG];	   /* the machine */
Inst *progp;		   /* next free spot for code generation */
Inst *pc;			   /* program counter during execution */
Inst *progbase = prog; /* start of current subprogram */
int returning;		   /* 1 if return stmt seen */

int execDepth;

extern int debugLevel;
extern int debugFlag;

typedef struct Frame
{						 /* proc/func call stack frame */
	Inst *retpc;		 /* where to resume after return */
	Symbol *varList;
	long nargs;			 /* number of arguments */
} Frame;
#define NFRAME 100
Frame frame[NFRAME];
Frame *fp; /* frame pointer */

void debugC(int flag, int level, const char *format, ...) {
	va_list va;
	memset(&va, 0, sizeof(va_list));
	char buf[1024] = {0};

	if (debugLevel >= level && (debugFlag & flag) != 0) {
		if (flag == hocExec)
			for (int i = 0; i < execDepth; i++)
				printf("\t");

		va_start(va, format);
		vsprintf(buf, format, va);
		va_end(va);
	}
	printf("%s", buf);
}

void debug(int level, const char *format, ...) {
	va_list va;
	memset(&va, 0, sizeof(va_list));
	char buf[1024] = {0};

	if (debugLevel >= level) {
		va_start(va, format);
		vsprintf(buf, format, va);
		va_end(va);
	}
	printf("%s", buf);
}

Symbol *parseVar(Symbol *name_sp) {
	Symbol *sp;
	if (fp == frame) {
		sp = lookup(globalSymbolList, name_sp->name);
	} else {
		sp = lookup(fp->varList, name_sp->name);
	}
	return sp;
}

void initcode(void)
{
	progp = progbase;
	stackp = stack;
	fp = frame;
	returning = 0;
}

void push(Datum d)
{
	if (stackp >= &stack[NSTACK])
		execerror("stack too deep", 0);
	*stackp++ = d;
}

Datum pop(void)
{
	if (stackp == stack)
		execerror("stack underflow", 0);
	return *--stackp;
}

void xpop(void) /* for when no value is wanted */
{
	if (stackp == stack)
		execerror("stack underflow(xpop)", (char *)0);
	--stackp;
}

void constpush(void)
{
	Datum d;
	d.val = *(((Symbol *)*pc++)->u.objPtr->u.numberVal);
	push(d);
}

void strpush(void)
{
	// ?
	Datum d;
	d.sym = (Symbol *)(*pc++);
	push(d);
}

void varpush(void)
{
	Datum d;
	Symbol *sp = (Symbol *)(*pc++);
	Symbol *var = parseVar(sp);

	d.sym = var;

	push(d);
}

void whilecode(void)
{
	Datum d;
	Inst *savepc = pc;

	execute(savepc + 2); /* condition */
	d = pop();
	while (d.val)
	{
		execute(*((Inst **)(savepc))); /* body */
		if (returning)
			break;
		execute(savepc + 2); /* condition */
		d = pop();
	}
	if (!returning)
		pc = *((Inst **)(savepc + 1)); /* next stmt */
}

void forcode(void)
{
	Datum d;
	Inst *savepc = pc;

	execute(savepc + 4); /* precharge */
	pop();
	execute(*((Inst **)(savepc))); /* condition */
	d = pop();
	while (d.val)
	{
		execute(*((Inst **)(savepc + 2))); /* body */
		if (returning)
			break;
		execute(*((Inst **)(savepc + 1))); /* post loop */
		pop();
		execute(*((Inst **)(savepc))); /* condition */
		d = pop();
	}
	if (!returning)
		pc = *((Inst **)(savepc + 3)); /* next stmt */
}

void ifcode(void)
{
	Datum d;
	Inst *savepc = pc; /* then part */

	execute(savepc + 3); /* condition */
	d = pop();
	if (d.val)
		execute(*((Inst **)(savepc)));
	else if (*((Inst **)(savepc + 1))) /* else part? */
		execute(*((Inst **)(savepc + 1)));
	if (!returning)
		pc = *((Inst **)(savepc + 2)); /* next stmt */
}

void defineBegin(Symbol *sp) /* put func/proc in symbol table */
{
	Object *newObject = (Object *)emalloc(sizeof(Object));
	newObject->type = FUNCTION;

	Info *newFuncInfo = (Info *)emalloc(sizeof(Info));
	newFuncInfo->argBegin = 0;
	newFuncInfo->paras = 0;
	newFuncInfo->defn = 0;
	newFuncInfo->nargs = 0;

	newObject->u.funcInfo = newFuncInfo;
	curDefiningFunction = newFuncInfo;

	sp->u.objPtr = newObject;
}

void defineEnd(Symbol *sp) {
	curDefiningFunction->defn = progbase;
	progbase = progp;	   /* next code starts here */
}

void setArg(int narg) {
	curDefiningFunction->argBegin = curDefiningFunction->paras;
	curDefiningFunction->nargs = narg;
} 

void globalBinding() {
	Symbol *sp = ((Symbol *)*pc++);
	Symbol *var = lookup(fp->varList, sp->name);
	Symbol *globalVar = lookup(globalSymbolList, sp->name);
	if (!globalVar)
		execerror(var->name, "can not find global one");
	var->u.objPtr = globalVar->u.objPtr;
}

void call(void) /* call a function */
{
	Symbol *name_sp = (Symbol *)pc[0]; /* symbol table entry */
								  /* for function */
	Symbol *sp = parseVar(name_sp);
	if (sp->u.objPtr == 0 || sp->u.objPtr->type != FUNCTION)
		execerror(sp->name, "not a function object");
	if (fp++ >= &frame[NFRAME - 1])
		execerror(sp->name, "call nested too deeply");

	Info *funcInfo = sp->u.objPtr->u.funcInfo;
	fp->nargs = (long)pc[1];
	if (funcInfo->nargs != fp->nargs)
		execerror(sp->name, "args not match");

	fp->retpc = pc + 2;

	// debugC(hocExec, 1, "calling %s nargs %d type %d\n", sp->name, fp->nargs, sp->type);
	// debugC(hocExec, 5, "entry address %p  return address %p\n", sp->u.defn, fp->retpc);

	// instantiate the variables
	for (Symbol *cur = funcInfo->paras; cur != funcInfo->argBegin; cur = cur->next) {
		if (strcmp(cur->name, "") != 0) {
			fp->varList = install(fp->varList, cur->name, VAR, 0.0);
		}
	}
	// bind the args, currently, we can only bind the numbers, this should be amend to bind the object directly
	for (Symbol *cur = funcInfo->argBegin; cur != 0; cur = cur->next) {
		Datum d = pop();
		if (strcmp(cur->name, "") != 0) {
			fp->varList = install(fp->varList, cur->name, VAR, 0.0);

			// this part should be fixed
			Object *newObj = (Object *)emalloc(sizeof(Object));
			newObj->type = NUMBER;
			newObj->u.numberVal = (double *)emalloc(sizeof(Object));
			*(newObj->u.numberVal) = d.val;
			fp->varList->u.objPtr = newObj;
		}
	}

	execute(funcInfo->defn);
	returning = 0;
}

static void ret(void) /* common return from func or proc */
{
	int i;
	pc = (Inst *)fp->retpc;
	--fp;
	returning = 1;
}

void funcret(void) /* return from a function */
{
	Datum d;
	// if (fp->sp->type == PROCEDURE)
	// 	execerror(fp->sp->name, "(proc) returns value");
	d = pop(); /* preserve function return value */
	ret();
	push(d);
}

void procret(void) /* return from a procedure */
{
	// if (fp->sp->type == FUNCTION)
	// 	execerror(fp->sp->name, "(func) returns no value");
	ret();
}

void bltin(void)
{

	Datum d;
	d = pop();
	d.val = (*(double (*)(double)) * pc++)(d.val);
	push(d);
}

void add(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val += d2.val;
	push(d1);
}

void sub(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val -= d2.val;
	push(d1);
}

void mul(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val *= d2.val;
	push(d1);
}

void divop(void)
{
	Datum d1, d2;
	d2 = pop();
	if (d2.val == 0.0)
		execerror("division by zero", (char *)0);
	d1 = pop();
	d1.val /= d2.val;
	push(d1);
}

void mod(void)
{
	Datum d1, d2;
	long x;
	d2 = pop();
	if (d2.val == 0.0)
		execerror("division by zero", (char *)0);
	d1 = pop();
	/* d1.val %= d2.val; */
	x = d1.val;
	x %= (long)d2.val;
	d1.val = d2.val = x;
	push(d1);
}

void negate(void)
{
	Datum d;
	d = pop();
	d.val = -d.val;
	push(d);
}

void verify(Symbol *s)
{
	if (s->type != VAR && s->type != UNDEF)
		execerror("attempt to evaluate non-variable", s->name);
	if (s->type == UNDEF)
		execerror("undefined variable", s->name);
}

void eval(void) /* evaluate variable on stack */
{
	Datum d;
	d = pop();
	Symbol *sp = parseVar(d.sym);
	verify(sp);
	d.val = *(sp->u.objPtr->u.numberVal);
	push(d);
}

void preinc(void)
{
	Datum d;
	d.sym = (Symbol *)(*pc++);
	verify(d.sym);
	d.val = *(d.sym->u.objPtr->u.numberVal) += 1.0;
	push(d);
}

void predec(void)
{
	Datum d;
	d.sym = (Symbol *)(*pc++);
	verify(d.sym);
	d.val = *(d.sym->u.objPtr->u.numberVal) -= 1.0;
	push(d);
}

void postinc(void)
{
	Datum d;
	double v;
	d.sym = (Symbol *)(*pc++);
	verify(d.sym);
	v = *(d.sym->u.objPtr->u.numberVal);
	*(d.sym->u.objPtr->u.numberVal) += 1.0;
	d.val = v;
	push(d);
}

void postdec(void)
{
	Datum d;
	double v;
	d.sym = (Symbol *)(*pc++);
	verify(d.sym);
	v = *(d.sym->u.objPtr->u.numberVal);
	*(d.sym->u.objPtr->u.numberVal) -= 1.0;
	d.val = v;
	push(d);
}

void gt(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val > d2.val);
	push(d1);
}

void lt(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val < d2.val);
	push(d1);
}

void ge(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val >= d2.val);
	push(d1);
}

void le(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val <= d2.val);
	push(d1);
}

void eq(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val == d2.val);
	push(d1);
}

void ne(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val != d2.val);
	push(d1);
}

void and (void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val != 0.0 && d2.val != 0.0);
	push(d1);
}

void or (void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = (double)(d1.val != 0.0 || d2.val != 0.0);
	push(d1);
}

void not(void)
{
	Datum d;
	d = pop();
	d.val = (double)(d.val == 0.0);
	push(d);
}

void power(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	d1.val = Pow(d1.val, d2.val);
	push(d1);
}

void assign(void)
{
	Datum d1, d2;
	d1 = pop();
	d2 = pop();
	if (d1.sym->type != VAR && d1.sym->type != UNDEF)
		execerror("assignment to non-variable",
				  d1.sym->name);
	if (d1.sym->u.objPtr) {
		if (d1.sym->u.objPtr->type == NUMBER)
			free(d1.sym->u.objPtr->u.numberVal);
		free(d1.sym->u.objPtr);
	}
	Object *newObj = (Object *)emalloc(sizeof(Object));
	newObj->type = NUMBER;
	newObj->u.numberVal = (double *)emalloc(sizeof(Object));
	d1.sym->u.objPtr = newObj;

	*(d1.sym->u.objPtr->u.numberVal) = d2.val;
	d1.sym->type = VAR;
	push(d2);
}

void addeq(void)
{
	Datum d1, d2;
	d1 = pop();
	d2 = pop();
	if (d1.sym->type != VAR && d1.sym->type != UNDEF)
		execerror("assignment to non-variable",
				  d1.sym->name);
	// d2.val = d1.sym->u.val += d2.val;
	d1.sym->type = VAR;
	push(d2);
}

void subeq(void)
{
	Datum d1, d2;
	d1 = pop();
	d2 = pop();
	if (d1.sym->type != VAR && d1.sym->type != UNDEF)
		execerror("assignment to non-variable",
				  d1.sym->name);
	// d2.val = d1.sym->u.val -= d2.val;
	d1.sym->type = VAR;
	push(d2);
}

void muleq(void)
{
	Datum d1, d2;
	d1 = pop();
	d2 = pop();
	if (d1.sym->type != VAR && d1.sym->type != UNDEF)
		execerror("assignment to non-variable",
				  d1.sym->name);
	// d2.val = d1.sym->u.val *= d2.val;
	d1.sym->type = VAR;
	push(d2);
}

void diveq(void)
{
	Datum d1, d2;
	d1 = pop();
	d2 = pop();
	if (d1.sym->type != VAR && d1.sym->type != UNDEF)
		execerror("assignment to non-variable",
				  d1.sym->name);
	// d2.val = d1.sym->u.val /= d2.val;
	d1.sym->type = VAR;
	push(d2);
}

void modeq(void)
{
	Datum d1, d2;
	long x;
	d1 = pop();
	d2 = pop();
	if (d1.sym->type != VAR && d1.sym->type != UNDEF)
		execerror("assignment to non-variable",
				  d1.sym->name);
	/* d2.val = d1.sym->u.val %= d2.val; */
	// x = d1.sym->u.val;
	x %= (long)d2.val;
	// d2.val = d1.sym->u.val = x;
	d1.sym->type = VAR;
	push(d2);
}

void printtop(void) /* pop top value from stack, print it */
{
	Datum d;
	static Symbol *s; /* last value computed */
	if (s == 0)
		s = install(globalSymbolList, "_", VAR, 0.0);
	d = pop();
	printf("%.*g\n", (int)*(lookup(keywordList, "PREC")->u.objPtr->u.numberVal), d.val);
	// s->u.objPtrj= d.val;
}

void prexpr(void) /* print numeric value */
{
	Datum d;
	d = pop();
	printf("%.*g ", (int)(*(lookup(keywordList, "PREC")->u.objPtr->u.numberVal)), d.val);
}

void prstr(void) /* print string value */
{
	/* 为什么能用？ */
	Symbol *s = (Symbol *)*pc++;
	printf("%s\n", s->u.objPtr->u.str);

	// Datum d;
	// d = pop();
	// printf("%s\n", d.sym->u.objPtr->u.str);
	// printf("str'size: %d\n", (int)strlen(d.sym->u.objPtr->u.str));
}

void varread(void) /* read into variable */
{
	Datum d;
	extern FILE *fin;
	Symbol *var = (Symbol *)*pc++;
Again:
	switch (fscanf(fin, "%lf", (var->u.objPtr->u.numberVal)))
	{
	case EOF:
		if (moreinput())
			goto Again;
		// d.val = var->u.val = 0.0;
		d.val = 0;
		break;
	case 0:
		execerror("non-number read into", var->name);
		break;
	default:
		d.val = *(var->u.objPtr->u.numberVal);
		break;
	}
	var->type = VAR;
	push(d);
}

Inst *code(Inst f) /* install one instruction or operand */
{
	Inst *oprogp = progp;
	if (progp >= &prog[NPROG])
		execerror("program too big", (char *)0);
	*progp++ = f;
	return oprogp;
}

void execute(Inst *p)
{
	debugC(hocExec, 4, "Executing from %ld\n", p - progbase);

	execDepth++;
	for (pc = p; *pc != STOP && !returning;)
		(*((++pc)[-1]))();
	execDepth--;

	debugC(hocExec, 4, "Executing ended at %ld, starting from %ld\n", pc - progbase, p - progbase);
}
