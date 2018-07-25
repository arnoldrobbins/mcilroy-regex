#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "sed.h"

#define ustrlen(p) strlen((char*)(p))
#define ustrcmp(p, q) strcmp((const char*)(p), (const char*)(q))
#define ustrcpy(p, q) (uchar*)strcpy((char*)(p), (const char*)(q))
#define ustrchr(p, c) (uchar*)strchr((const char*)(p), c)

int blank(Text*);
void fixlabels(Text*);
void fixbrack(Text*);
void ckludge(Text*, int, int, int, Text*);
int addr(Text*, Text*);
int pack(int, int, int);
int* instr(uchar*);
uchar *succi(uchar*);
extern void jprint(regex_t*);	/* secret entry into regex pkg */

int semicolon;
Text rebuf;

uchar adrs[256] = {	/* max no. of addrs, 3 is illegal */
	0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 3, 3, 3, 3, 3, /* <nl> */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 2, 3, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/* !# */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 3, 1, 3, 3, /* := */
	3, 3, 3, 3, 2, 3, 3, 2, 2, 3, 3, 3, 3, 3, 2, 3, /* DGHN */
	2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/* P */
	3, 1, 2, 2, 2, 3, 3, 2, 2, 1, 3, 3, 2, 3, 2, 3, /* a-n */
	2, 1, 2, 2, 2, 3, 3, 2, 2, 2, 3, 2, 3, 0, 3, 3, /* p-y{} */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

#define Ec Tc	/* commands that have same compilation method */
#define Dc Tc
#define Gc Tc
#define Hc Tc
#define Nc Tc
#define Pc Tc
#define dc Tc
#define gc Tc
#define hc Tc
#define lc Tc
#define nc Tc
#define pc Tc
#define qc Tc
#define xc Tc
#define tc bc
#define ic ac
#define cc ac

typedef void cmdf(Text*, Text*);
cmdf Xc, Cc, Ec;	/* comment #, colon, equal */
cmdf Lc, Rc;		/* left {, right }, */
cmdf Ic, Tc, xx;	/* ignore, trivial, error */
cmdf Dc, Gc, Hc, Nc, Pc;
cmdf ac, bc, cc, dc, gc, hc, ic, nc;
cmdf pc, qc, rc, sc, tc, wc, xc, yc;

static cmdf *docom[128] = {
	xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,Ic,xx,xx,xx,xx,xx, /* <nl> */
	xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,
	xx,Ic,xx,Xc,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx, /* !# */
	xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,Cc,Ic,xx,Ec,xx,xx, /* :;= */
	xx,xx,xx,xx,Dc,xx,xx,Gc,Hc,xx,xx,xx,xx,xx,Nc,xx, /* DGHN */
	Pc,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx, /* P */
	xx,ac,bc,cc,dc,xx,xx,gc,hc,ic,xx,xx,lc,xx,nc,xx, /* a-n */
	pc,qc,rc,sc,tc,xx,xx,wc,xc,yc,xx,Lc,xx,Rc,xx,xx  /* p-y{} */
};

uchar *synl;	/* current line pointer for syntax errors */

void
compile(Text *script, Text *t)
{
	int loc;	/* progam counter */
	int neg;	/* ! in effect */
	int cmd;
	int naddr;
	int *q;		/* address of instruction word */
	t->w = t->s;	/* here w is a read pointer */
	while(*t->w) {
		assure(script, 4*sizeof(int));
		loc = script->w - script->s;
		synl = t->w;
		naddr = 0;
		while(blank(t)) ;
		naddr += addr(script, t);
		if(naddr && *t->w ==',') {
			t->w++;
			naddr += addr(script, t);
			if(naddr < 2)
				syntax("missing address");
		}
		q = (int*)script->w;
		if(naddr == 2)
			*q++ = INACT;
		script->w = (uchar*)(q+1);
		neg = 0;
		for(;;) {
			while(blank(t));
			cmd = *t->w++;
			if(neg && docom[cmd&0xff]==Ic)
				syntax("improper !");
			if(cmd != '!')
				break;
			neg = NEG;
		}
		if(!neg) {
			switch(adrs[cmd]) {
			case 1:
				if(naddr <= 1)
					break;
			case 0:
				if(naddr == 0)
					break;
				syntax("too many addresses");
			}
		}
		docom[cmd&0xff](script, t);
		switch(*t->w) {
		case 0:
			script->w = script->s + loc;
			break;
		default:
			if(cmd == '{')
				break;
			syntax("junk after command");
		case ';':
			if(!semicolon++)
				synwarn("semicolon separators");
		case '\n':
				t->w++;
		}
		*q = pack(neg,cmd,script->w-script->s-loc);
	}
	fixbrack(script);
	fixlabels(script);
}
	

/* COMMAND LAYOUT */

int
blank(Text *t)
{
	if(*t->w==' ' || *t->w=='\t') {
		t->w++;
		return 1;
	} else
		return 0;
}

int *
instr(uchar *p)		/* get address of command word */
{
	int *q = (int*)p;
	while((*q & IMASK) != IMASK)
		q++;
	return q;
}

uchar *
succi(uchar *p)
{
	int *q = instr(p);
	if(code(*q) == '{')
		return (uchar*)(q+1);
	else
		return p + (*q & LMASK);
}

int
pack(int neg, int cmd, int length)
{
	int l = length & LMASK;
	if(length != l)
		syntax("<command-list> or <text> too long");
	return IMASK | neg | cmd << 2*BYTE | l;
}

void
putint(Text *s, int n)
{
	assure(s, sizeof(int));
	*(int*)s->w = n;
	s->w += sizeof(int);
}

int
number(Text *t)
{
	unsigned n = 0;
	while(isdigit(*t->w)) {
		if(n > (INT_MAX-9)/10)
			syntax("number too big");
		n = n*10 + *t->w++ - '0';
	}
	return n;
}	

int
addr(Text *script, Text *t)
{
	int n;
	switch(*t->w) {
	default:
		return 0;
	case '$':
		t->w++;
		n = DOLLAR;
		break;
	case '\\':
		t->w++;
		if(*t->w=='\n' ||*t->w=='\\')
			syntax("bad regexp delimiter");
	case '/':
		n = recomp(&rebuf, t, *t->w++) | REGADR;
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		n = number(t);
		if(n == 0)
			syntax("address is zero");
	}
	putint(script, n);
	return 1;
}

regex_t *
readdr(int x)
{
	return (regex_t*)(rebuf.s + (x&AMASK));
}

/* LABEL HANDLING */

/* the labels array consists of intptr_t values followed by strings.
   value -1 means unassigned; other values are relative to the
   beginning of the script.
   
   the files array, which is also accessed using lablook,
   stores FILE* pointers into the intptr_t slots.
   value -1 still means unassigned.

   on the first pass, every script ref to a label becomes the
   integer offset of that label in the labels array, or -1 if
   it is a branch to the end of script

   on the second pass (fixlabels), the script ref is replaced
   by the value from the labels array. */

Text labels;

intptr_t *
lablook(uchar *l, Text *labels)
{
	uchar *p, *q;
	int n;
	assure(labels, 1);
	for(p = labels->s; p < labels->w; ) {
		q = p + sizeof(intptr_t);
		if(ustrcmp(q, l) == 0)
			return (intptr_t*)p;
		q += ustrlen(q) + 1;
		p = (uchar*)intptrp(q);
	}
	n = ustrlen(l);
	assure(labels, sizeof(intptr_t)+n+1+sizeof(intptr_t));
	*(intptr_t*)p = -1;
	q = p + sizeof(intptr_t);
	ustrcpy(q, l);
	q += ustrlen(q) + 1;
	labels->w = (uchar*)intptrp(q);
	return (intptr_t*)p;
}

/* find pos in label list; assign value i to label if i>=0 */

int
getlab(Text *t, int i)
{
	intptr_t *p;
	uchar *u;
	while(blank(t));	/* not exactly posix */
	for(u=t->w; *t->w!='\n'; t->w++)
		if(!isprint(*t->w) || *t->w==' ')
			synwarn("invisible character in name");
	if(u == t->w)
		return -1;
	*t->w = 0;
	p = lablook(u, &labels);
	if(*p == -1)
		*p = i;
	else if(i != -1)
		syntax("duplicate label");
	*t->w = '\n';
	return (uchar*)p - labels.s;
}

void
Cc(Text *script, Text *t)	/* colon */
{
	if(getlab(t, script->w - sizeof(int) - script->s) == -1)
		syntax("missing label");
}

void
bc(Text *script, Text *t)
{
	int g;
	g = getlab(t, -1);	/* relative pointer to label list */
	putint(script, g);
}
			
void
fixlabels(Text *script)
{
	uchar *p;
	int *q;
	for(p=script->s; p<script->w; p=succi(p)) {
		q = instr(p);
		switch(code(*q)) {
		case 't':
		case 'b':
			if(q[1] == -1)
				q[1] = script->w - script->s;
			else if(*(intptr_t*)(labels.s+q[1]) != -1)
				q[1] = *(intptr_t*)(labels.s+q[1]);
			else
				quit("undefined label: ",
					labels.s+q[1]+sizeof(intptr_t));
		}
	}
	free(labels.s);
}

/* FILES */

Text files;

void
rc(Text *script, Text *t)
{
	uchar *u;
	if(!blank(t))
		synwarn("no space before file name");
	while(blank(t)) ;
	for(u=t->w; *t->w!='\n'; t->w++) ;
	if(u == t->w)
		syntax("missing file name");
	*t->w = 0;
	putint(script, (uchar*)lablook(u, &files) - files.s);
	*t->w = '\n';
}

void
wc(Text *script, Text *t)
{
	intptr_t *p;
	rc(script, t);
	p = (intptr_t*)(files.s + ((int*)script->w)[-1]);
	if(*p != -1)
		return;
	*(FILE**)p = fopen((char*)(p+1), "w");
	if(*p == 0)
		syntax("can't open file for writing");
}

/* BRACKETS */

Text brack;

/* Lc() stacks (in brack) the location of the { command word.
   Rc() stuffs into that word the offset of the } sequel
   relative to the command word.
   fixbrack() modifies the offset to be relative to the
   beginning of the instruction, including addresses. */

void				/* { */
Lc(Text *script, Text *t)
{
	while(blank(t));
	putint(&brack, script->w - sizeof(int) - script->s);
}

void				/* } */
Rc(Text *script, Text *t)
{
	int l;
	int *p;
	t = t;
	if(brack.w == 0 || (brack.w-=sizeof(int)) < brack.s)
		syntax("unmatched }");
	l = *(int*)brack.w;
	p = (int*)(script->s + l);
	l = script->w - script->s - l;
	if(l >= LMASK - 3*sizeof(int))	/* fixbrack could add 3 */
		syntax("{command-list} too long)");
	*p = (*p&~LMASK) | l;
}

void
fixbrack(Text *script)
{
	uchar *p;
	int *q;
	if(brack.w == 0)
		return;
	if(brack.w > brack.s)
		syntax("unmatched {");
	for(p=script->s; p<script->w; p=succi(p)) {
		q = instr(p);
		if(code(*q) == '{')
			*q += (uchar*)q - p;
	}
	free(brack.s);
}

/* EASY COMMANDS */

void
Xc(Text *script, Text *t)	/* # */
{
	script = script;	/* avoid use/set diagnostics */
	if(t->s[1]=='n')
		nflag = 1;
	while(*t->w != '\n')
		t->w++;
}

void
Ic(Text *script, Text *t)	/* ignore */
{
	script = script;
	t->w--;
}

void
Tc(Text *script, Text *t)	/* trivial to compile */
{
	script = script;
	t = t;
}

void
xx(Text *script, Text *t)
{
	script = script;
	t = t;
	syntax("unknown command");
}

/* MISCELLANY */

void
ac(Text *script, Text *t)
{
	if(*t->w++ != '\\' || *t->w++ != '\n')
		syntax("\\<newline> missing after command");
	for(;;) {
		while(bflag && blank(t)) ;
		assure(script, 2 + sizeof(int));
		switch(*t->w) {
		case 0:
			quit("bug: missed end of <text>");
		case '\n':
			*script->w++ = *t->w;
			*script->w++ = 0;
			script->w = (uchar*)intp(script->w);
			return;
		case '\\':
			t->w++;
		default:
			*script->w++ = *t->w++;
		}
	}
}
void
yc(Text *script, Text *t)
{
	int i;
	int delim = *t->w++;
	uchar *s = script->w;
	uchar *p, *q;
	uchar c, d;
	if(delim == '\n' || delim=='\\')
		syntax("missing delimiter");
	assure(script, 256);
	for(i=0; i<256; i++) 
		s[i] = 0;
	for(q=t->w; *q!=delim; q++)
		if(*q == '\n')
			syntax("missing delimiter");
		else if(*q=='\\' && q[1]==delim)
			q++;
	for(p=t->w, q++; *p != delim; p++, q++) {
		if(*p=='\\' && p[1]==delim)
			p++;
		if(*q == '\n')
			syntax("missing delimiter");
		if(*q == delim)
			syntax("string lengths differ");
		if(*q=='\\' && q[1]==delim)
			q++;
		if(s[*p] && s[*p]!=*q)
			syntax("ambiguous map");
		if(s[*p])
			synwarn("redundant map");
		s[*p] = *q;
	}
	if(*q++ != delim)
		syntax("string lengths differ");
	for(i=0; i<256; i++)
		if(s[i] == 0)
			s[i] = i;
	t->w = q;
	script->w += 256;
}

void
sc(Text *script, Text *t)
{
	int c, flags, re;
	int *q;
	int n = -1;
	int nsub;
	int delim = *t->w++;
	switch(delim) {
	case '\n':
	case '\\':
		syntax("improper delimiter");
	}
	re = recomp(&rebuf, t, delim);
	putint(script, re);
	nsub = readdr(re)->re_nsub;
	flags = script->w - script->s;
	putint(script, 0);		/* space for flags */
	while((c=*t->w++) != delim) {
		assure(script, 3+sizeof(int*));
		if(c == '\n')
			syntax("unterminated command");
		else if(c == '\\') {
			int d = *t->w;
			if(d==delim)
				;
			else if(d=='&' || d=='\\')
				*script->w++ = c;
			else if(d>='0' && d<='9') {
				if(d > '0'+nsub)
					syntax("improper backreference");
				*script->w++ = c;
			}
			c = *t->w++;
		}
		*script->w++ = c;
	}
	*script->w++ = 0;
	script->w = (uchar*)intp(script->w);
	q = (int*)(script->s + flags);
	*q = 0;
	for(;;) {
		switch(*t->w) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if(n != -1)
				syntax("extra flags");
			n = number(t);
			if(n == 0 || (n&(PFLAG|WFLAG)) != 0)
				syntax("count out of range");
			continue;
		case 'p':
			if(*q & PFLAG)
				syntax("extra flags");
			t->w++;
			*q |= PFLAG;
			continue;
		case 'g':
			t->w++;
			if(n != -1)
				syntax("extra flags");
			n = 0;
			continue;
		case 'w':
			t->w++;
			*q |= WFLAG;	 
			wc(script, t);
		}
		break;
	}
	*q |= n==-1? 1: n;
}		

void
synwarn(const char *s)
{
	uchar *t = ustrchr(synl, '\n');
	warn("%s: %.*s", s, t-synl, synl);
}

void
syntax(const char *s)
{
	uchar *t = ustrchr(synl, '\n');
	quit("%s: %.*s", s, t-synl, synl);
}


void
printscript(Text *script)
{
/*
	uchar *s;
	int *q;
	for(s=script->s; s<script->w; s = succi(s)) {
		q = (int*)s;
		if((*q&IMASK) != IMASK) {
			if((*q&REGADR) == 0)
				printf("%d", *q);
			else
				jprint((regex_t*)(*q & AMASK));
			q++;
		}
		if((*q&IMASK) != IMASK) {
			if((*q&REGADR) == 0)
				printf(",%d", *q);
			else
				jprint((regex_t*)(*q & AMASK));
			q += 2;
		}
		if(code(*q) == '\n')
			continue;
		printf("%s%c\n", *q&NEG?"!":"", code(*q));
	}	
*/
}

/* debugging code 2; execute stub.
   prints the compiled script (without arguments)
   then each input line with line numbers	

void
execute(Text *script, Text *y)
{
	if(recno == 1)
		printscript(script);
	printf("%d:%s",recno,y->s);
}

*/
