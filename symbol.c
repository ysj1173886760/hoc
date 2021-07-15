#include <stdlib.h>
#include "hoc.h"
#include "y.tab.h"
#include <string.h>

Symbol *lookup(Symbol *symList, char *s) /* find s in symbol table */
{
	Symbol *sp;

	for (sp = symList; sp != (Symbol *)0; sp = sp->next)
		if (strcmp(sp->name, s) == 0)
			return sp;
	return 0; /* 0 ==> not found */
}

Symbol *install(Symbol *symList, char *s, int t) /* install s in symbol table */
{
	Symbol *sp;

	sp = emalloc(sizeof(Symbol));
	sp->name = emalloc(strlen(s) + 1); /* +1 for '\0' */
	strcpy(sp->name, s);
	sp->type = t;
	sp->u.objPtr = 0;

	sp->next = symList; /* put at front of list */
	return sp;
}

void *emalloc(unsigned n) /* check return from malloc */
{
	char *p;

	p = malloc(n);
	if (p == 0)
		execerror("out of memory", (char *)0);
	return p;
}

Symbol *lookupThoughAddress(Symbol *symList, Symbol *p)
{
	Symbol *sp;
	for (sp = symList; sp != (Symbol *)0; sp = sp->next)
		if (sp == p)
			return sp;
	return 0;
}

TypeLookupEntry *findTypeTable(char *name) {
	for (TypeLookupEntry *cur = globalTypeTable; cur != 0; cur = cur->next) {
		if (strcmp(cur->typename, name) == 0) {
			return cur;
		}
	}
	return NULL;
}

MemberCallLookupEntry *findMemberCall(char *name, MemberCallLookupEntry *table) {
	for (MemberCallLookupEntry *cur = table; cur != 0; cur = cur->next) {
		if (strcmp(cur->name, name) == 0) {
			return cur;
		}
	}
	return NULL;
}