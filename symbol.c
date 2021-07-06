#include <stdlib.h>
#include "hoc.h"
#include "y.tab.h"
#include <string.h>
#include <stdio.h>

static Symbol *symlist = 0; /* symbol table: linked list */
static Symbol *keylist = 0;
extern Info Infostack[MAX];
extern Info *Infostackp;

Symbol *install_key(char *s, int t, double d) /* install s in keyword table */
{
	Symbol *sp;
	sp = emalloc(sizeof(Symbol));
	sp->name = emalloc(strlen(s) + 1); /* +1 for '\0' */
	strcpy(sp->name, s);
	sp->type = t;
	sp->u.val = d;
	sp->next = keylist; /* put at front of list */
	keylist = sp;
	return sp;
}

Symbol *lookup_key(char *s) /* find s in keyword table */
{
	// printf("lookup global\n");
	Symbol *sp;
	for (sp = keylist; sp != (Symbol *)0; sp = sp->next)
		if (strcmp(sp->name, s) == 0)
			return sp;
	return 0; /* 0 ==> not found */
}

Symbol *install_global(char *s, int t, double d) /* install s in symbol table */
{
	Symbol *sp;
	sp = emalloc(sizeof(Symbol));
	sp->name = emalloc(strlen(s) + 1); /* +1 for '\0' */
	strcpy(sp->name, s);
	sp->type = t;
	sp->u.val = d;
	sp->next = symlist; /* put at front of list */
	symlist = sp;
	return sp;
}

Symbol *lookup_global(char *s) /* find s in symbol table */
{
	// printf("lookup global\n");
	Symbol *sp;
	for (sp = symlist; sp != (Symbol *)0; sp = sp->next)
		if (strcmp(sp->name, s) == 0)
			return sp;
	return 0; /* 0 ==> not found */
}

Symbol *install_para(Info *Infostackp, char *s, int t, double d)
{
	// printf("install para : %s\n", s);
	Symbol *sp;
	sp = emalloc(sizeof(Symbol));
	sp->name = emalloc(strlen(s) + 1); /* +1 for '\0' */
	strcpy(sp->name, s);
	sp->type = t;
	sp->u.val = d;
	sp->next = Infostackp->paras; /* put at front of list */
	Infostackp->paras = sp;
	++Infostackp->nparas;

	return sp;
}

Symbol *lookup_para(Info *Infostackp, char *s)
{
	Symbol *sp;
	for (sp = Infostackp->paras; sp != (Symbol *)0; sp = sp->next)
	{
		// printf("cmp : %s\n", sp->name);
		if (strcmp(sp->name, s) == 0)
			return sp;
	}
	return 0;
}

void *emalloc(unsigned n) /* check return from malloc */
{
	char *p;

	p = malloc(n);
	if (p == 0)
		execerror("out of memory", (char *)0);
	return p;
}

Symbol *lookup_global_ThoughAddress(Symbol *p)
{
	Symbol *sp;
	for (sp = symlist; sp != (Symbol *)0; sp = sp->next)
		if (sp == p)
			return sp;
	return 0;
}
