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
int flag = 1; // flag = 0 : varpush	flag = 1 : valpush

extern int debugLevel;
extern int debugFlag;

typedef struct Frame
{				 /* proc/func call stack frame */
	Inst *retpc; /* where to resume after return */
	Symbol *varList;
	long nargs; /* number of arguments */
} Frame;
#define NFRAME 100
Frame frame[NFRAME];
Frame *fp; /* frame pointer */

Symbol *cur_opr_sym; //当前操作(A.opr)的对象(A)
typedef void (*FunType)(void);

Object *objpop() {
	Datum d = pop();
	return d.u.obj;
}

void append(void)
{
	Object *obj = objpop();
	int size = cur_opr_sym->u.objPtr->size + 1;
	Object **tmp = (Object **)emalloc(size * sizeof(Object *));
	for (int i = 0; i < size - 1; ++i)
		tmp[i] = cur_opr_sym->u.objPtr->u.list[i];
	tmp[size - 1] = obj;
	free(cur_opr_sym->u.objPtr->u.list);
	cur_opr_sym->u.objPtr->u.list = tmp;
	cur_opr_sym->u.objPtr->size = size;
}

void test(void)
{
	printf("test\n");
}

Datum double2Datum(double val)
{
	Datum d;
	d.setflag = 0;
	d.u.obj = (Object *)emalloc(sizeof(Object));
	d.u.obj->type = NUMBER;
	d.u.obj->size = 1;
	d.u.obj->u.valuelist = (double *)emalloc(sizeof(double));
	*(d.u.obj->u.valuelist) = val;
	return d;
}

Datum str2Datum(char *str)
{
	Datum d;
	d.setflag = 0;
	d.u.obj = (Object *)emalloc(sizeof(Object));
	d.u.obj->type = STRING;
	d.u.obj->size = strlen(str);
	d.u.obj->u.str = (char *)emalloc((strlen(str)+1) * sizeof(char));
	strcpy(d.u.obj->u.str, str);

	return d;
}

// problem : double->double *
double *valpop(void)
{
	Datum d;
	d = pop();
	if (d.setflag == 0)
		return d.u.obj->u.value;
	// d contains sym
	Symbol *sp = parseVar(d.u.sym);
	verify(sp);

	return sp->u.objPtr->u.value;
}

void debugC(int flag, int level, const char *format, ...)
{
	va_list va;
	memset(&va, 0, sizeof(va_list));
	char buf[1024] = {0};

	if (debugLevel >= level && (debugFlag & flag) != 0)
	{
		if (flag == hocExec)
			for (int i = 0; i < execDepth; i++)
				printf("\t");

		va_start(va, format);
		vsprintf(buf, format, va);
		va_end(va);
	}
	printf("%s", buf);
}

void debug(int level, const char *format, ...)
{
	va_list va;
	memset(&va, 0, sizeof(va_list));
	char buf[1024] = {0};

	if (debugLevel >= level)
	{
		va_start(va, format);
		vsprintf(buf, format, va);
		va_end(va);
	}
	printf("%s", buf);
}

Symbol *parseVar(Symbol *name_sp)
{
	Symbol *sp;
	if (fp == frame)
	{
		sp = lookup(globalSymbolList, name_sp->name);
	}
	else
	{
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

void objpush(void)
{
	Datum d;
	d.u.obj = (Object *)(*pc);
	d.setflag = 0;
	pc++;
	push(d);
}

void listpush(void)
{
	Datum d;
	long size = (long)*pc++;
	Object **valuelist = (Object **)emalloc(size * sizeof(Object *));
	for (int i = size - 1; i >= 0; --i)
	{
		// have some problem : can't support a = [[1,2,3],4,5] case
		Object *obj = objpop();
		valuelist[i] = obj;
	}
	d.setflag = 0;
	d.u.obj = (Object *)emalloc(sizeof(Object));
	d.u.obj->type = (long)LIST;
	d.u.obj->size = size;
	d.u.obj->u.list = valuelist;
	push(d);
}

void memberpush(void)
{
	Datum d;
	Symbol *name = (Symbol *)*pc++;
	Symbol *sp = parseVar(name);
	
	Object *obj = objpop();
	if (obj->type != NUMBER)
		execerror(sp->name, " can not support non-number index");
	
	int id = *(obj->u.value);
	if (sp->u.objPtr->type != LIST)
		execerror(sp->name, " is not a LIST type");
	if (id >= sp->u.objPtr->size)
		execerror(sp->name, " doesn't has enough member");

	d.setflag = 0;
	d.u.obj = sp->u.objPtr->u.list[id];
	push(d);
}

void varpush(void)
{
	Datum d;
	Symbol *sp = (Symbol *)(*pc++);
	Symbol *var = parseVar(sp);
	d.u.sym = var;
	d.setflag = 1;
	push(d);
}

void exprpush(void)
{
	Datum d;
	Symbol *sp = (Symbol *)(*pc++);
	Symbol *var = parseVar(sp);
	verify(var);

	// // if (!var->u.objPtr)
	// // 	execerror("valpush error: ", var->name);
	// double val = *(var->u.objPtr->u.valuelist);
	// d.setflag = 0;
	// d.u.obj = (Object *)emalloc(sizeof(Object));
	// d.u.obj->type = var->u.objPtr->type;
	// d.u.obj->size = var->u.objPtr->size;
	// d.u.obj->u.valuelist = (double *)emalloc(d.u.obj->size * sizeof(double));
	// // for (int i = 0; i < d.u.obj->size; ++i)
	// // 	d.u.obj->u.valuelist[i] = var->u.objPtr->u.valuelist[i];
	// d.u.obj->u.valuelist = var->u.objPtr->u.valuelist;

	d.u.obj = var->u.objPtr;
	d.setflag = 0;
	push(d);
}

void whilecode(void)
{
	Inst *savepc = pc;

	execute(savepc + 2); /* condition */
	Object *obj = objpop();
	if (obj->type != NUMBER) {
		execerror("expect number when checking condition", "");
	}
	double cond = *(obj->u.value);
	
	while (cond)
	{
		execute(*((Inst **)(savepc))); /* body */
		if (returning)
			break;
		execute(savepc + 2); /* condition */
		cond = *valpop();
	}
	if (!returning)
		pc = *((Inst **)(savepc + 1)); /* next stmt */
}

void forcode(void)
{
	Inst *savepc = pc;

	execute(savepc + 4); /* precharge */
	pop();
	execute(*((Inst **)(savepc))); /* condition */

	Object *obj = objpop();
	if (obj->type != NUMBER) {
		execerror("expect number when checking condition", "");
	}

	double cond = *(obj->u.value);

	while (cond)
	{
		execute(*((Inst **)(savepc + 2))); /* body */
		if (returning)
			break;
		execute(*((Inst **)(savepc + 1))); /* post loop */
		pop();
		execute(*((Inst **)(savepc))); /* condition */
		cond = *valpop();
	}
	if (!returning)
		pc = *((Inst **)(savepc + 3)); /* next stmt */
}

void ifcode(void)
{
	Datum d;
	Inst *savepc = pc; /* then part */

	execute(savepc + 3); /* condition */

	Object *obj = objpop();
	if (obj->type != NUMBER) {
		execerror("expect number when checking condition", "");
	}

	double cond = *(obj->u.value);

	if (cond)
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

void defineEnd(Symbol *sp)
{
	curDefiningFunction->defn = progbase;
	progbase = progp; /* next code starts here */
}

void setArg(int narg)
{
	curDefiningFunction->argBegin = curDefiningFunction->paras;
	curDefiningFunction->nargs = narg;
}

// global binding also affects the unchangeable object. this should be fixed
void globalBinding()
{
	Symbol *sp = ((Symbol *)*pc++);
	
	// release the previous obj
	Symbol *var = lookup(fp->varList, sp->name);
	var->u.objPtr = NULL;
	
	Symbol *globalVar = lookup(globalSymbolList, sp->name);
	if (!globalVar)
		execerror(var->name, "can not find global one");
	if (fp == frame) 
		execerror(var->name, "should not use global in global field");
	
	var->u.objPtr = globalVar->u.objPtr;
}

void call(void) /* call a function */
{
	Symbol *name_sp = (Symbol *)pc[0]; /* symbol table entry */
									   /* for function */
	Symbol *sp = parseVar(name_sp);
	if (sp->u.objPtr == 0 || sp->u.objPtr->type != FUNCTION)
		execerror(sp->name, "is not a function object");
	if (fp++ >= &frame[NFRAME - 1])
		execerror(sp->name, "call nested too deeply");

	Info *funcInfo = sp->u.objPtr->u.funcInfo;
	fp->nargs = (long)pc[1];
	if (funcInfo->nargs != fp->nargs)
		execerror(sp->name, "args not match");

	fp->retpc = pc + 2;

	debugC(hocExec, 1, "callings %s nargs %d type %d\n", sp->name, fp->nargs, sp->type);
	// debugC(hocExec, 5, "entry address %p  return address %p\n", sp->u.defn, fp->retpc);

	// instantiate the variables
	for (Symbol *cur = funcInfo->paras; cur != funcInfo->argBegin; cur = cur->next)
	{
		if (strcmp(cur->name, "") != 0)
		{
			fp->varList = install(fp->varList, cur->name, VAR);
		}
	}
	// bind the args, currently, we can only bind the numbers, this should be amend to bind the object directly
	for (Symbol *cur = funcInfo->argBegin; cur != 0; cur = cur->next)
	{
		Object *obj = objpop();
		if (strcmp(cur->name, "") != 0)
		{
			fp->varList = install(fp->varList, cur->name, VAR);
			fp->varList->u.objPtr = obj;
		}
	}

	execute(funcInfo->defn);
	returning = 0;
}

// maybe you can combine call and oprcall into a general_call, update in future
void oprcall(void)
{
	Symbol *name_sp = (Symbol *)pc[0];
	Symbol *sp = parseVar(name_sp);
	cur_opr_sym = sp;

	Symbol *s_opr = (Symbol *)pc[1];

	TypeLookupEntry *type = NULL;
	switch (sp->u.objPtr->type) {
	case LIST:
		type = findTypeTable("list");
		break;
	case NUMBER:
		type = findTypeTable("number");
		break;
	default:
		execerror(sp->name, "doesn't support member function call");
	}

	MemberCallLookupEntry *memberCallInfo = findMemberCall(s_opr->name, type->memberTable);
	if (!memberCallInfo)
		execerror(s_opr->name, "is not a correct member function call");

	long nargs = (long)pc[2];
	if (nargs != memberCallInfo->opr.nargs)
		execerror(s_opr->name, "nargs match error");

	Inst *retpc = pc + 3;
	(*memberCallInfo->opr.func)();
	pc = (Inst *)retpc;
}

void listchange() {
	Object *obj = objpop();
	Object *indexObj = objpop();
	int index = *(indexObj->u.value);
	
	if (index >= cur_opr_sym->u.objPtr->size)
		execerror(cur_opr_sym->name, "out of bounds");
	
	cur_opr_sym->u.objPtr->u.list[index] = obj;
}

void numberchange() {
	Object *obj = objpop();
	cur_opr_sym->u.objPtr = obj;
}

Datum double2Datum(double val)
{
	Datum d;
	d.setflag = 0;
	d.u.obj = (Object *)emalloc(sizeof(Object));
	d.u.obj->type = NUMBER;
	d.u.obj->size = 1;
	d.u.obj->u.value = (double *)emalloc(sizeof(double));
	*(d.u.obj->u.value) = val;
	return d;
}

void ret(void) /* common return from func or proc */
{
	int i;
	pc = (Inst *)fp->retpc;
	--fp;
	returning = 1;
}

void procret(void) /* common return from func or proc */
{
	execerror("program returning with no value", "");
}

void bltin(void)
{
	double val = *valpop();
	val = (*(double (*)(double)) * pc++)(val);
	Datum d = double2Datum(val);
	push(d);
}

void add(void)
{
	Datum d;
	Object *d1, *d2;
	d2 = objpop();
	d1 = objpop();
	
	// check undefined var.
	if (!d1 || !d2)
		execerror("add error, ", "d1 or d2 is undefined");

	if (d1->type == NUMBER && d2->type == NUMBER) 
		d = double2Datum(*(d1->u.valuelist) + *(d2->u.valuelist));
	else if (d1->type == STRING && d2->type == STRING)
	{
		char *tmp = (char *)emalloc((strlen(d1->u.str)+strlen(d2->u.str)+1) * sizeof(char));
		strcpy(tmp, d1->u.str);
		strcat(tmp, d2->u.str);
		d = str2Datum(tmp);
	} 
	else 
	{
		d.setflag = 1;
		d.u.sym = (Symbol *)emalloc(sizeof(Symbol));
		d.u.sym->type = (long int)emalloc(sizeof(long int));
		d.u.sym->type = UNDEF;
		execerror("View variable types", "");
	}

	push(d);
}

void sub(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 - d2);
	push(d);
}

void mul(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 * d2);
	push(d);
}

void divop(void)
{
	double d1, d2;
	d2 = *valpop();
	if (d2 == 0.0)
		execerror("division by zero", (char *)0);
	d1 = *valpop();
	Datum d = double2Datum(d1 / d2);
	push(d);
}

void mod(void)
{
	double d1, d2;
	long x;
	d2 = *valpop();
	if (d2 == 0.0)
		execerror("division by zero", (char *)0);
	d1 = *valpop();
	x = d1;
	x %= (long)d2;
	Datum d = double2Datum(x);
	push(d);
}

void negate(void)
{
	double val = *valpop();
	Datum d = double2Datum(-val);
	push(d);
}

void printObj(Object *obj) {
	if (obj->type == STRING) {
		printf("%s", obj->u.str);
	} else if (obj->type == LIST) {
		for (int i = 0; i < obj->size; i++) {
			printObj(obj->u.list[i]);
			printf(" ");
		}
	} else {
		printf("%.10g", *(obj->u.value));
	}
}

void printStack() {
	printf("printing stack\n");
	for (Datum *cur = stackp - 1; cur >= stack; cur--) {
		if (cur->setflag == 0) {
			printObj(cur->u.obj);
			printf(" ");
		} else {
			printf("sym: %s\n", cur->u.sym->name);
		}
	}
}

void verify(Symbol *s)
{
	if (s->type != VAR && s->type != UNDEF)
		execerror("attempt to evaluate non-variable", s->name);
	if (s->type == UNDEF)
		execerror("undefined variable", s->name);
}

void gt(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 > d2);
	push(d);
}

void lt(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 < d2);
	push(d);
}

void ge(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 >= d2);
	push(d);
}

void le(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 <= d2);
	push(d);
}

void eq(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 == d2);
	push(d);
}

void ne(void)
{
	double d1, d2;
	d2 = *valpop();
	d1 = *valpop();
	Datum d = double2Datum(d1 != d2);
	push(d);
}

void and (void)
{
	double d1 = *valpop();
	double d2 = *valpop();
	push(double2Datum((double)(d1 != 0 && d2 != 0)));
}

void or (void)
{
	// Datum d1, d2;
	// d2 = pop();
	// d1 = pop();
	// d1.val = (double)(d1.val != 0.0 || d2.val != 0.0);
	// push(d1);
}

void not(void)
{
	// Datum d;
	// d = pop();
	// d.val = (double)(d.val == 0.0);
	// push(d);
}

void power(void)
{
	// Datum d1, d2;
	// d2 = pop();
	// d1 = pop();
	// d1.val = Pow(d1.val, d2.val);
	// push(d1);
}

// void freeobj(Datum d)
// {
// 	if (d.u.sym->u.objPtr)
// 	{
// 		if (d.u.sym->u.objPtr->type == NUMBER || d.u.sym->u.objPtr->type == LIST)
// 			free(d.u.sym->u.objPtr->u.valuelist);
// 		else if (d.u.sym->u.objPtr->type == STRING)
// 			free(d.u.sym->u.objPtr->u.str);
// 		free(d.u.sym->u.objPtr);
// 	}
// }

void assign(void)
{
	Datum d1, d2;
	d1 = pop();
	d2 = pop();

	if (d1.u.sym->type != VAR && d1.u.sym->type != UNDEF)
		execerror("assignment to non-variable", d1.u.sym->name);

	// // if we free obj here, will have problem, like a = a.
	// // d2 is obj
	// if (d2.setflag == 0)
	// {
	// 	Object *newObj = (Object *)emalloc(sizeof(Object));
	// 	newObj->type = d2.u.obj->type;
	// 	newObj->size = d2.u.obj->size;

	// 	if (d2.u.obj->type == NUMBER) 
	// 	{
	// 		newObj->u.valuelist = (double *)emalloc(sizeof(double));
	// 		// newObj->u.valuelist = d2.u.obj->u.valuelist;
	// 		double val = *(d2.u.obj->u.valuelist);
	// 		*(newObj->u.valuelist) = val;
	// 	} else if (d2.u.obj->type == LIST)
	// 	{
	// 		newObj->u.valuelist = (double *)emalloc(newObj->size * sizeof(double));
	// 		newObj->u.valuelist = d2.u.obj->u.valuelist;
	// 	} else if (d2.u.obj->type == STRING)
	// 	{
	// 		newObj->u.str = (char *)emalloc((newObj->size+1) * sizeof(char));
	// 		strcpy(newObj->u.str, d2.u.obj->u.str);
	// 	}
	// 	freeobj(d1);
	// 	d1.u.sym->u.objPtr = newObj;
	// 	d1.u.sym->type = VAR;
	// } 
	// // d2 is sym
	// else 
	// {
	// 	if (d2.u.sym->type == UNDEF)
	// 		execerror("assign error, undefined variable", d2.u.sym->name);
	// 	Object *newObj = (Object *)emalloc(sizeof(Object));
	// 	newObj->type = d2.u.sym->u.objPtr->type;
	// 	newObj->size = d2.u.sym->u.objPtr->size;

	// 	if (d2.u.sym->u.objPtr->type == NUMBER)
	// 	{
	// 		newObj->u.valuelist = (double *)emalloc(sizeof(double));
	// 		// newObj->u.valuelist = d2.u.sym->u.objPtr->u.valuelist;
	// 		double val = *(d2.u.sym->u.objPtr->u.valuelist);
	// 		*(newObj->u.valuelist) = val;
	// 	} else if (d2.u.sym->u.objPtr->type == LIST) 
	// 	{
	// 		newObj->u.valuelist = (double *)emalloc(newObj->size * sizeof(double));
	// 		newObj->u.valuelist = d2.u.sym->u.objPtr->u.valuelist;
	// 	} else if (d2.u.sym->u.objPtr->type == STRING)
	// 	{
	// 		newObj->u.str = (char *)emalloc((newObj->size+1) * sizeof(char));
	// 		strcpy(newObj->u.str, d2.u.sym->u.objPtr->u.str);
	// 	}
	// 	freeobj(d1);
	// 	d1.u.sym->u.objPtr = newObj;
	// 	d1.u.sym->type = VAR;
	// }
	d1.u.sym->u.objPtr = d2.u.obj;
	d1.u.sym->type = VAR;
	push(d2);
}

void addeq(void)
{
	// a += b ==> d2: a, d1: b
	Datum d1, d2;
	d2 = pop();
	d1 = pop();

	if (d2.u.sym->type != VAR)
		// execerror(d2.u.sym->name, "undefined");
		execerror("addeq: assignment to non-variable", d2.u.sym->name);

	if (d1.u.obj->type == NUMBER && d2.u.sym->u.objPtr->type == NUMBER)
		*d2.u.sym->u.objPtr->u.valuelist += *d1.u.obj->u.valuelist;
	else if (d1.u.obj->type == STRING && d2.u.sym->u.objPtr->type == STRING)
	{
		char *tmp = (char *)emalloc((strlen(d2.u.sym->u.objPtr->u.str)+strlen(d1.u.obj->u.str)+1) * sizeof(char));
		strcpy(tmp, d2.u.sym->u.objPtr->u.str);
		strcat(tmp, d1.u.obj->u.str);
		free(d2.u.sym->u.objPtr->u.str);
		d2.u.sym->u.objPtr->u.str = (char *)emalloc(strlen(tmp) * sizeof(char));
		strcpy(d2.u.sym->u.objPtr->u.str, tmp);
	}
	else
		execerror("addeq: check var type", (char *)0);

	push(d1);
}

void subeq(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	if (d2.u.sym->type != VAR)
		// execerror("assignment to non-variable", d2.u.sym->name);
		execerror("subeq: assignment to non-variable", d2.u.sym->name);

	if (d1.u.obj->type == NUMBER && d2.u.sym->u.objPtr->type == NUMBER)
		*d2.u.sym->u.objPtr->u.valuelist -= *d1.u.obj->u.valuelist;
	else 
		execerror("subeq: check var type", (char *)0);
	push(d1);
}

void muleq(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	if (d2.u.sym->type != VAR)
		// execerror("assignment to non-variable", d2.u.sym->name);
		execerror("muleq: assignment to non-variable", d2.u.sym->name);

	if (d1.u.obj->type == NUMBER && d2.u.sym->u.objPtr->type == NUMBER)
		*d2.u.sym->u.objPtr->u.valuelist *= *d1.u.obj->u.valuelist;
	else 
		execerror("muleq: check var type", (char *)0);
	push(d1);
}

void diveq(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	if (d2.u.sym->type != VAR)
		// execerror("assignment to non-variable", d2.u.sym->name);
		execerror("diveq: assignment to non-variable", d2.u.sym->name);

	if (d1.u.obj->type == NUMBER && d2.u.sym->u.objPtr->type == NUMBER)
	{
		if (*d1.u.obj->u.valuelist == 0.0)
			execerror("division by zero", (char *)0);
		*d2.u.sym->u.objPtr->u.valuelist /= *d1.u.obj->u.valuelist;
	}
	else 
		execerror("diveq: check var type", (char *)0);
	push(d1);
}

void modeq(void)
{
	Datum d1, d2;
	d2 = pop();
	d1 = pop();
	if (d2.u.sym->type != VAR)
		// execerror("assignment to non-variable", d2.u.sym->name);
		execerror("modeq: assignment to non-variable", d2.u.sym->name);

	if (d1.u.obj->type == NUMBER && d2.u.sym->u.objPtr->type == NUMBER)
	{
		if (*d1.u.obj->u.valuelist == 0.0)
			execerror("division by zero", (char *)0);
		
		long int x = (long int)*d2.u.sym->u.objPtr->u.valuelist;
		x %= (long int)*d1.u.obj->u.valuelist;
		// (long)*d2.u.sym->u.objPtr->u.valuelist %= (long)*d1.u.obj->u.valuelist;
		*d2.u.sym->u.objPtr->u.valuelist = (double)x;
	}
	else 
		execerror("modeq: check var type", (char *)0);
	push(d1);
}

void printtop(void) /* pop top value from stack, print it */
{
	// what's the diff between prittop and prexpr
	prexpr();
	printf("\n");
}

void prexpr(void) /* print expr value */
{
	Object *obj = objpop();
	printObj(obj);
}

void prstr(void) /* print string value */
{
	printf("%s", (char *)*pc++);
}

void varread(void) /* read into variable */
{
	// 	Datum d;
	// 	extern FILE *fin;
	// 	Symbol *var = (Symbol *)*pc++;
	// Again:
	// 	switch (fscanf(fin, "%lf", (var->u.objPtr->u.valuelist)))
	// 	{
	// 	case EOF:
	// 		if (moreinput())
	// 			goto Again;
	// 		// d.val = var->u.val = 0.0;
	// 		d.val = 0;
	// 		break;
	// 	case 0:
	// 		execerror("non-number read into", var->name);
	// 		break;
	// 	default:
	// 		d.val = *(var->u.objPtr->u.valuelist);
	// 		break;
	// 	}
	// 	var->type = VAR;
	// 	push(d);
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

void setFlag()
{
	flag = (flag == 0) ? 1 : 0;
}
