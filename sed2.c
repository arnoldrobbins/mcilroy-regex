#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "sed.h"

#define ustrchr(p, c) (uchar*)strchr((char*)(p), c)

int selected(uchar*, Text*);

#define Re Ie
#define Ce Ie
#define Se Ie
#define re ae

/* execution functions return pointer to next instruction */

typedef uchar *exef(Text*, uchar *, Text*);
exef Ce, Se, Ee;	/* colon, semicolon, equal */
exef Le, Re;		/* left {, right { */
exef Ie, vv;		/* ignore, error */
exef De, Ge, He, Ne, Pe;
exef ae, be, ce, de, ge, he, ie, le, ne;
exef pe, qe, re, se, te, we, xe, ye;

static exef *excom[128] = {
	vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,Ie,vv,vv,vv,vv,vv,
	vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,
	vv,vv,vv,Ie,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv, /* # */
	vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,Ce,Se,vv,Ee,vv,vv, /* :;= */
	vv,vv,vv,vv,De,vv,vv,Ge,He,vv,vv,vv,vv,vv,Ne,vv, /* DGHN */
	Pe,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv,vv, /* P */
	vv,ae,be,ce,de,vv,vv,ge,he,ie,vv,vv,le,vv,ne,vv, /* a-n */
	pe,qe,re,se,te,vv,vv,we,xe,ye,vv,Le,vv,Re,vv,vv  /* p-y{} */
};

#define IBUG "interpreter bug %d"
char *stdouterr = "writing standard output";

Text hold;

void
cputchar(int c)
{
	if(putchar(c) == EOF)
		quit(stdouterr);
}

void
writeline(Text *data)
{
	int n = data->w - data->s;
	if(fwrite(data->s, 1, n, stdout) != n)
		quit(stdouterr);
	cputchar('\n');
}

void
execute(Text *script, Text *data)
{
	uchar *pc;
	int sel;
	for(pc = script->s; pc < script->w; ) {
		sel = selected(pc, data);
		if(sel) {
			int cmd = code(*instr(pc));
			if(sel==2 && cmd=='c')
				cmd = 'd';
			pc = excom[cmd](script, pc, data);
			if(pc == 0)
				return;
		} else
			pc = nexti(pc);
	}
	if(!nflag)
		writeline(data);
}

/* return 1 if action is to be taken on current line,
         -1 if (numeric) address has been passed,
	  0 otherwise*/
int
sel1(int addr, Text *data)
{
	if(addr & REGADR)
		return regexec(readdr(addr),(char*)data->s,0,0,0) == 0;
	if(addr == recno)
		return 1;
	if(addr == DOLLAR)
		return ateof();
	if(addr < recno)
		return -1;
	return 0;
}

/* return 2 on non-final line of a selected range,
          1 on any other selected line,
	  0 on non-selected lines 
   (the 1-2 distinction matters only for 'c' commands) */

int
selected(uchar *pc, Text *data)
{
	int active;
	int *ipc = (int*)pc;	/* points to address words */
	int *q = instr(pc);	/* points to instruction word */
	int neg = !!(*q & NEG);
	switch(q - ipc)	{
	case 0:			/* 0 address */
		return !neg;
	case 1:			/* 1 address */
		return neg ^ sel1(ipc[0], data)==1;
	case 2:
		quit(IBUG,1);
	case 3:			/* 2 address */
		q--;		/* points to activity indicator */
		active = !(*q & INACT);
		if((*q&AMASK) < recno) {
			switch(sel1(ipc[active], data)) {
			case 0:
				if((active&ateof()) == 0)
					break;
			case 1:
				*q = recno;
				if(active)
					*q |= INACT;
				return (neg^1) << (!active&!ateof());
			case -1:
				if(active) {
					*q = recno | INACT;
					return neg;
				}
			}
		}
		return (neg^active) << 1;
	default:
		quit(IBUG,2);
		return 0;	/* dummy */
	}
}

void
vacate(Text *t)
{
	assure(t, 1);
	t->w = t->s;
	*t->w = 0;
}

void
tcopy(Text *from, Text *to)
{
	int n = from->w - from->s;
	assure(to, n+1);
	memmove(to->w, from->s, n);
	to->w += n;
	*to->w = 0;
}
	

/* EASY COMMANDS */

uchar *
vv(Text *script, uchar *pc, Text *data)
{
	script = script;
	pc = pc;
	data = data;
	quit(IBUG,3);
	return 0;	/* dummy */
}

uchar *
be(Text *script, uchar *pc, Text *data)
{
	script = script;
	data = data;
	return script->s + instr(pc)[1];
}

uchar *
De(Text *script, uchar *pc, Text *data)
{
	int n;
	uchar *end = (uchar*)ustrchr(data->s, '\n');
	if(end == 0)
		return de(script, pc, data);
	end++;
	n = data->w - end;
	memmove(data->s, end, n+1);
	data->w = data->s + n;
	return script->s;
}

uchar *
de(Text *script, uchar *pc, Text *data)
{
	pc = pc;
	vacate(data);
	return 0;
}

uchar *
Ee(Text *script, uchar *pc, Text *data)
{
	script = script;
	data = data;
	if(printf("%d\n", recno) <= 0)
		quit(stdouterr);
	return nexti(pc);
}

uchar *
Ge(Text *script, uchar *pc, Text *data)
{
	script = script;
	if(hold.s == 0) 
		vacate(&hold);
	if(data->w > data->s)
		*data->w++ = '\n';
	tcopy(&hold, data);
	return nexti(pc);
}

uchar *
ge(Text *script, uchar *pc, Text *data)
{
	vacate(data);
	return Ge(script, pc, data);
}

uchar *
He(Text *script, uchar *pc, Text *data)
{
	script = script;
	assure(&hold, 1);
	*hold.w++ = '\n';
	tcopy(data, &hold);
	return nexti(pc);
}

uchar *
he(Text *script, uchar *pc, Text *data)
{
	script = script;
	vacate(&hold);
	tcopy(data, &hold);
	return nexti(pc);
}

uchar *
Ie(Text *script, uchar *pc, Text *data)
{
	script = script;
	data = data;
	return nexti(pc);
}

uchar *
ie(Text *script, uchar *pc, Text *data)
{
	script = script;
	data = data;
	if(printf("%s", (char*)(instr(pc)+1)) <= 0)
		quit(stdouterr);
	return nexti(pc);
}

uchar *
Le(Text *script, uchar *pc, Text *data)
{
	script = script;
	data = data;
	return (uchar*)(instr(pc)+1);
}

uchar *
Ne(Text *script, uchar *pc, Text *data)
{
	assure(data, 1);
	*data->w++ = '\n';
	if(readline(data))
		return nexti(pc);
	*--data->w = 0;
	return de(script, pc, data);
}

uchar *
ne(Text *script, uchar *pc, Text *data)
{
	if(!nflag)
		writeline(data);
	vacate(data);
	if(readline(data))
		return nexti(pc);
	return 0;
}

uchar *
Pe(Text *script, uchar *pc, Text *data)
{
	int n;
	uchar *end = ustrchr(data->s, '\n');
	if(end == 0)
		n = data->w - data->s;
	else
		n = end - data->s;
	if(fwrite(data->s, 1, n, stdout) != n)
		quit(stdouterr);
	cputchar('\n');
	script = script;
	return nexti(pc);
}

uchar *
pe(Text *script, uchar *pc, Text *data)
{
	writeline(data);
	script = script;
	return nexti(pc);
}

uchar *
qe(Text *script, uchar *pc, Text *data)
{
	pc = pc;
	data = data;
	qflag++;
	return script->w;
}

uchar *
te(Text *script, uchar *pc, Text *data)
{
	int tflag = sflag;
	sflag = 0;
	if(tflag)
		return be(script, pc, data);
	else
		return nexti(pc);
}

uchar *
ww(Text *script, uchar *pc, Text *data, int offset)
{
	int *q = (int*)(files.s + offset);
	FILE *f = *(FILE**)q;
	int n = data->w - data->s;
	assure(data, 1);
	*data->w = '\n';
	if(fwrite(data->s, 1, n+1, f) != n+1 ||
	   fflush(f) == EOF)	/* in case of subsequent r */
		quit("error writing %s", (char*)(q+1));
	*data->w = 0;
	script = script;
	return nexti(pc);
}

uchar *
we(Text *script, uchar *pc, Text *data)
{
	return ww(script, pc, data, instr(pc)[1]);
}

uchar *
xe(Text *script, uchar *pc, Text *data)
{
	uchar *t;
	script = script;
	if(hold.s == 0)
		vacate(&hold);
	exch(data->s, hold.s, t);
	exch(data->e, hold.e, t);
	exch(data->w, hold.w, t);
	return nexti(pc);
}

uchar *
ye(Text *script, uchar *pc, Text *data)
{
	uchar *s = (uchar*)data->s;
	uchar *w = (uchar*)data->w;
	uchar *tbl = (uchar*)(instr(pc)+1);
	for( ; s<w; s++)
		*s = tbl[*s];
	script = script;
	return nexti(pc);
}

/* MISCELLANY */

uchar *
se(Text *script, uchar *pc, Text *data)
{
	int *q = instr(pc);
	int flags = q[2];
	uchar *p = (uchar*)(q+3);
	int n = flags & ~(PFLAG|WFLAG);

	sflag = substitute(readdr(q[1]), data, p, n);
	if(!sflag)
		return nexti(pc);
	if(flags & PFLAG)
		pe(script, pc, data);
	if(flags & WFLAG)
		return ww(script, pc, data, ((int*)nexti(pc))[-1]);
	return nexti(pc);
}

struct { char p, q; } digram[] = {
	'\\',	'\\',
	'\a',	'a',
	'\b',	'b',
	'\f',	'f',
	'\n',	'n',
	'\r',	'r',
	'\t',	't',
	'\v',	'v',
};

uchar *
le(Text *script, uchar *pc, Text *data)
{
	int i = 0;
	int j;
	uchar *s;
	script = script;
	for(s=data->s; s<data->w; s++, i++) {
		if(i >= 60) {
			cputchar('\\');
			cputchar('\n');
			i = 0;
		}
		for(j=0; j<sizeof(digram)/sizeof(*digram); j++)
			if(*s == digram[j].p) {
				cputchar('\\');
				cputchar(digram[j].q);
				goto cont;
			}
		if(!isprint(*s)) {
			if(printf("\\%3.3o", *s) <= 0)
				quit(stdouterr);
		} else
			cputchar(*s);
	cont:	;
	}
	cputchar('$');
	cputchar('\n');
	return nexti(pc);
}	

/* END-OF-CYCLE STUFF */

Text todo;

uchar *
ae(Text *script, uchar *pc, Text *data)
{
	script = script;
	data = data;
	assure(&todo, sizeof(uchar*));
	*(uchar**)todo.w = pc;
	todo.w += sizeof(uchar*);
	return nexti(pc);
}

uchar *
ce(Text *script, uchar *pc, Text *data)
{	
	if(printf("%s", (char*)(instr(pc)+1)) <= 0)
		quit(stdouterr);
	return de(script, pc, data);
}

void
coda(void)
{
	int c;
	int *q;
	uchar *p;
	FILE *f;
	if(todo.s == 0)
		return;
	for(p=todo.s; p<todo.w; p+=sizeof(int)) {
		q = instr(*(uchar**)p);
		switch(code(*q)) {
		case 'a':
			if(printf("%s", (char*)(q+1)) <= 0)
				quit(stdouterr);
			continue;
		case 'r':
			f = fopen((char*)(files.s+q[1]+sizeof(int)), "r");
			if(f == 0)
				continue;
			while((c=getc(f)) != EOF)
				cputchar(c);
			fclose(f);
			continue;
		default:
			quit(IBUG,5);
		}
	}
	vacate(&todo);
}
