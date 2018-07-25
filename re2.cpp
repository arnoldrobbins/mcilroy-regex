// regular expression compiler

#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include "re.h"

#ifdef DEBUG
int cdebug = 0;
#else
#define cdebug 0
#endif

Rex* Done::done;

/* Lexical analysis */

enum Token {
	T_META = UCHAR_MAX+1,	// must be first
	T_CFLX,
	T_DOT,
	T_END,
	T_BAD,
	T_DOLL,
	T_STAR,
	T_PLUS,
	T_QUES,
	T_OPEN,
	T_CLOSE,
	T_LEFT,
	T_RIGHT,
	T_BRA,
	T_BAR,
	T_AND,
	T_BANG,
	T_BACK = 512,
	T_NEXT = T_BACK+BACK_REF_MAX+1	// dummy spaceholder
};

/* table of special characters.  the "funny" things get special
   treatment at ends of BRE. escindex[c], if nonzero, tells
   where to find key c in the escape table.  to make this
   work, row zero of the escape table can't be used */

enum { BRE, ERE, ARE };	


static uchar escindex[UCHAR_MAX+1];

static struct {
	uchar key;
	struct { short unesc, esc; } val[3];
} escape[] = {
//	  key	     BRE	     ERE	     ARE
	{							},
	{ '\\',	'\\', 	'\\', 	'\\',	'\\',	'\\',	'\\'	},
	{ '^',	'^',	'^',	T_CFLX, '^',	T_CFLX, '^'	}, // funny
	{ '.',	T_DOT,	'.',	T_DOT, 	'.',	T_DOT, 	'.'	},
	{ '$',	'$',	'$',	T_DOLL, '$',	T_DOLL, '$'	}, // funny
	{ '*',	T_STAR,	'*',	T_STAR, '*',	T_STAR, '*'	},
	{ '[',	T_BRA,	'[',	T_BRA,	'[',	T_BRA,	'['	},
	{ '|',	'|',	T_BAD,	T_BAR,	'|',	T_BAR,	'|'	},
	{ '+',	'+',	T_BAD,	T_PLUS,	'+',	T_PLUS,	'+'	},
	{ '?',	'?',	T_BAD,	T_QUES, '?',	T_QUES, '?'	},
	{ '(',	'(',	T_OPEN,	T_OPEN, '(',	T_OPEN, '('	},
	{ ')',	')',	T_CLOSE,T_CLOSE,')',	T_CLOSE,')'	},
	{ '{',	'{',	T_LEFT,	T_LEFT,	'{',	T_LEFT,	'{'	},
	{ '&',	'&',	T_BAD,	'&',	T_BAD,	T_AND,	'&'	},
	{ '!',	'!',	T_BAD,	'!',	T_BAD,	T_BANG, '!'	},
	{ '}',	'}',	T_RIGHT,  '}',	T_BAD,	'}',	T_BAD	},
	{ '1',	'1',	T_BACK+1, '1',	T_BAD,	'1',	T_BAD	},
	{ '2',	'2',	T_BACK+2, '2',	T_BAD,	'2',	T_BAD	},
	{ '3',	'3',	T_BACK+3, '3',	T_BAD,	'3',	T_BAD	},
	{ '4',	'4',	T_BACK+4, '4',	T_BAD,	'4',	T_BAD	},
	{ '5',	'5',	T_BACK+5, '5',	T_BAD,	'5',	T_BAD	},
	{ '6',	'6',	T_BACK+6, '6',	T_BAD,	'6',	T_BAD	},
	{ '7',	'7',	T_BACK+7, '7',	T_BAD,	'7',	T_BAD	},
	{ '8',	'8',	T_BACK+8, '8',	T_BAD,	'8',	T_BAD	},
	{ '9',	'9',	T_BACK+9, '9',	T_BAD,	'9',	T_BAD	}
};

/* character maps for REG_ICASE. conceptually they are statics
   in regex_t, but initializing them would entail dependence
   on C++ runtime system, which we don't want */

static uchar ident[UCHAR_MAX+1];
static uchar fold[UCHAR_MAX+1];

static void
init()
{
	int i;
	for(i=0; (unsigned)i<elementsof(escape); i++)
		escindex[escape[i].key] = i;
	for(i=0; i<=UCHAR_MAX; i++) {
		ident[i] = i;
		fold[i] = toupper(i);
	}
}

/* compilation environment, one static copy would do were it
   not for threads */

struct Cenv {
	int flags;	// cflags arg to regcomp -- keep first
	Seg cursor;	// current point in re being compiled
	Seg expr;	// the whole reg exp
	uchar *map;	// for REG_ICASE
	int parno;	// number of last open paren
	int parnest;	// nesting depth
	short backref;	// bit vector of backref numbers
	uchar retype;	// BRE, ERE, or ARE
	uchar paren[BACK_REF_MAX+1];// paren[i] is 1 if \i is defined
	int posixkludge;// used by token() to make * nonspecial
	Cenv(const char *pattern, int cflags);
	Cenv(int cflags) : flags(cflags) { }	// short form
};

Cenv::Cenv(const char *pattern, int cflags) : flags(cflags),
	parno(0), parnest(0), backref(0),
	cursor(Seg((uchar*)pattern, strlen(pattern)))
{
	if(fold[UCHAR_MAX] == 0)
		init();
	map = flags&REG_ICASE? fold: ident;

	retype = flags&REG_AUGMENTED? ARE:	// ARE=>ERE
		 flags&REG_EXTENDED? ERE:
		 BRE;
	posixkludge = retype == BRE;
	expr = cursor;
	memset(paren, 0, sizeof(paren));
}

#ifdef DEBUG
static void
printnew(Rex *rex)
{
	int t;
	printf("new %s\n",
		rex==0? "ESPACE":
		(t=rex->type)==OK? "OK":
		t==ANCHOR? "ANCHOR":
		t==END? "END":
		t==DOT? "DOT":
		t==ONECHAR? "ONECHAR":
		t==STRING? "STRING":
		t==KMP? "KMP":
		t==KR? "KR":
		t==TRIE? "TRIE":
		t==CLASS? "CLASS":
		t==BACK? "BACK":
		t==SUBEXP? "SUBEXP":
		t==ALT? "ALT":
		t==REP? "REP":
		t==TEMP? "TEMP":
		"HUH");
}
#else
#define printnew(rex)
#endif

static Rex *ERROR = 0;

Rex *NEWinit(Rex *rex, Cenv *env)
{
	if(cdebug)
		printnew(rex);
	if(env->flags&SPACE) {
		delete rex;
		return ERROR;
	} else
		return rex;
}
#define NEW(x) NEWinit(new x, env)

/* determine whether greedy matching will work, i.e. produce
   the best match first.  such expressions are "easy", and
   need no backtracking once a complete match is found.  
   if an expression has backreferences or alts it's hard
   else if it has only one closure it's easy
   else if all closures are simple (i.e. one-character) it's easy
   else it's hard.
*/

/* struct in which statistics about an re are gathered */

struct Stat {
	int  n;		// min length
	uchar s;	// number of simple closures
	uchar c;	// number of closures
	uchar b;	// number of backrefs
	uchar t;	// number of tries
	int a;		// number of alternations
	uchar p;	// number of parens (subexpressions)
	uchar o;	// nonzero on overflow of some field
	Stat() { memset(this, 0, sizeof(Stat)); }
};

static Stat addStat(Stat &st1, Stat &st2)
{	
	Stat st = st1;
	st.n += st2.n;
	st.s += st2.s;
	st.c += st2.c;
	st.b += st2.b;
	st.a += st2.a;
	st.p += st2.p;
	st.t += st2.t;
	if(st.n<st1.n || st.c<st1.c || st.b<st1.b ||
	   st.a<st1.a || st.p<st1.p || st.t<st1.t)
		st.o |= 1;
	return st;
}
static Stat addStat(Stat &st1, Rex *rex, Cenv *env)
{
	if(rex == 0)
		return st1;
	Stat st2 = rex->stat(env);
	return addStat(st1, st2);
}
Stat Rex::stat(Cenv *env)
{
	static Stat nullStat;
	return addStat(nullStat, next, env);
}
Stat Dup::stat(Cenv *env)
{
	Stat st1;
	st1.n = lo;
	st1.s = st1.c = hi != lo;
	return addStat(st1, next, env);
}
Stat Back::stat(Cenv *env)
{
	static Stat backStat;
	backStat.b = 1;
	return addStat(backStat, next, env);
}
Stat Subexp::stat(Cenv *env)
{
	Stat st = rex->stat(env);
	if(env->backref & 1<<n)     // sole reason stat() has env param
		used = 1;
	if(++st.p <= 0)
		st.o |= 1;
	return addStat(st, next, env);
}
Stat Alt::stat(Cenv *env)
{
	Stat st1 = left->stat(env);
	Stat st2 = right->stat(env);
	Stat st = addStat(st1, st2);
	st.n = st1.n<=st2.n? st1.n: st2.n;
	if(++st.a <= 0)
		st.o |= 1;
	return addStat(st, next, env);
}
Stat Conj::stat(Cenv *env)
{
	Stat st1 = left->stat(env);
	Stat st = addStat(st1, right, env);
	return addStat(st, next, env);
}
Stat Rep::stat(Cenv *env)
{
	Stat st = rex->stat(env);
	if(st.n == 1 && st.c+st.b == 0)
		st.s++;
	st.c++;
	st.n *= lo;
	if(st.n<0 || st.c<=0)
		st.o |= 1;
	return addStat(st, next, env);
}
Stat Neg::stat(Cenv *env)
{
	Stat st = rex->stat(env);
	return addStat(st, next, env);
}
Stat String::stat(Cenv *env)
{
	Stat st;
	st.n = seg.n;
	return addStat(st, next, env);
}
Stat Trie::stat(Cenv *env)
{
	Stat st;
	st.n = min;
	if(min == max)
		return st;
	if(++st.t <= 0)
		st.o |= 1;
	return st;
}

int hard(Stat *stat)
{
	if(stat->a | stat->b)
		return HARD;
	else if(stat->t <=1 && stat->c == 0)
		return EASY;
	else if(stat->t)
		return HARD;
	else if(stat->c<=1 || stat->s==stat->c)
		return EASY;
	else
		return HARD;
}

/* Assign subpattern numbers by a preorder tree walk. */

int Rex::serialize(int n)
{
	serial = n++;		// hygiene; won't be used
	return next? next->serialize(n): n;
}

int Subexp::serialize(int n)
{
	serial = n++;
	n = rex->serialize(n);
	return next? next->serialize(n): n;
}

int Alt::serialize(int n)
{
	serial = n++;
	n = left->serialize(n);
	rserial = n++;
	n = right->serialize(n);
	return next? next->serialize(n): n;
}

int Conj::serialize(int n)
{
	serial = n++;
	n = left->serialize(n);
	n = right->serialize(n);
	return next? next->serialize(n): n;
}

int Rep::serialize(int n)
{
	serial = n++;
	n = rex->serialize(n);
	return next? next->serialize(n): n;
}

int Neg::serialize(int n)
{
	serial = n++;
	n = rex->serialize(n);
	return next? next->serialize(n): n;
}
/* extended and escaped are both 0-1 arguments */


/* this stuff gets around posix failure to define isblank,
   and the fact that ctype functions are macros */

static int Isalnum(int c) { return isalnum(c); }
static int Isalpha(int c) { return isalpha(c); }
static int Isblank(int c) { return c==' ' || c=='\t'; }
static int Iscntrl(int c) { return iscntrl(c); }
static int Isdigit(int c) { return isdigit(c); }
static int Isgraph(int c) { return isgraph(c); }
static int Islower(int c) { return islower(c); }
static int Isprint(int c) { return isprint(c); }
static int Ispunct(int c) { return ispunct(c); }
static int Isspace(int c) { return isspace(c); }
static int Isupper(int c) { return isupper(c); }
static int Isxdigit(int c){ return isxdigit(c);}

static struct {
	const char *name;
	int(*ctype)(int);
} ctype[] = {
	 { "alnum", Isalnum },
	 { "alpha", Isalpha },
	 { "blank", Isblank },
	 { "cntrl", Iscntrl },
	 { "digit", Isdigit },
	 { "graph", Isgraph },
	 { "lower", Islower },
	 { "print", Isprint },
	 { "punct", Ispunct },
	 { "space", Isspace },
	 { "upper", Isupper },
	 { "xdigit",Isxdigit}
};

static int
getcharcl(int c, Set *set, Cenv *env)
{
	int i, j, n;
	for(i=0; (unsigned)i<elementsof(ctype); i++) {
		n = strlen(ctype[i].name);
		if(env->cursor.n > n+2 &&
		   strncmp(ctype[i].name,(char*)env->cursor.p,n) == 0 &&
		   env->cursor.p[n] == c && env->cursor.p[n+1] == ']') {
			env->cursor.next(n+2);
			int (*f)(int) = ctype[i].ctype;
			for(j=0; j<UCHAR_MAX+1; j++)
				if(f(j))
					set->insert(j);
			return 1;
		}
	}
	return 0;
}

// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html#tag_07_03_01_01
const char *collelem[] = {
	"NUL",
	"SOH",
	"STX",
	"ETX",
	"EOT",
	"ENQ",
	"ACK",
	"alert",
	"backspace",
	"tab",
	"newline",
	"vertical-tab",
	"form-feed",
	"carriage-return",
	"SO",
	"SI",
	"DLE",
	"DC1",
	"DC2",
	"DC3",
	"DC4",
	"NAK",
	"SYN",
	"ETB",
	"CAN",
	"EM",
	"SUB",
	"ESC",
	"IS4",
	"IS3",
	"IS2",
	"IS1",
	"space",
	"exclamation-mark",
	"quotation-mark",
	"number-sign",
	"dollar-sign",
	"percent-sign",
	"ampersand",
	"apostrophe",
	"left-parenthesis",
	"right-parenthesis",
	"asterisk",
	"plus-sign",
	"comma",
	"hyphen-minus",
	"period",
	"slash",
	"zero",
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"colon",
	"semicolon",
	"less-than-sign",
	"equals-sign",
	"greater-than-sign",
	"question-mark",
	"commercial-at",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"left-square-bracket",
	"backslash",
	"right-square-bracket",
	"circumflex",
	"underscore",
	"grave-accent",
	"a",
	"b",
	"c",
	"d",
	"e",
	"f",
	"g",
	"h",
	"i",
	"j",
	"k",
	"l",
	"m",
	"n",
	"o",
	"p",
	"q",
	"r",
	"s",
	"t",
	"u",
	"v",
	"w",
	"x",
	"y",
	"z",
	"left-curly-bracket",
	"vertical-line",
	"right-curly-bracket",
	"tilde",
	"DEL",
};

/* find a collating element delimited by [c c], where c is
   either '=' or '.' */
static int
findcollelem(int c, Cenv *env)
{
	int i, l;
	const uchar *p, *ep;

	if(env->cursor.n < 3)
		return -1;
	p = env->cursor.p;
	if(p[1] == c && p[2] == ']') {
		// TODO: testre.dat requires that [.?.] and [.[.] be rejected but it's unclear why.
		// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap09.html does not help.
		if(p[0] == '?' || p[0] == '[')
			return -1;
		i = p[0];
		env->cursor.next(3);
		return i;
	}
	ep = env->cursor.p+env->cursor.n; 
	for(p++; ep-p >= 2; p++) {
		if(p[0] == c && p[1] == ']') {
			// Found end of bracket, look up name.
			for(i=0; i<128; i++) {
				l = strlen(collelem[i]);
				if(p - env->cursor.p == l && memcmp(collelem[i], env->cursor.p, l) == 0) {
					env->cursor.next(l+2);
					return i;
				}
			}
			break;
		}
	}
	return -1;
}

static int
token(Cenv *env)
{
	long n = env->cursor.n;
	if(n <= 0)
		return T_END;
	int c = *env->cursor.p;
	if(env->flags & REG_LITERAL)
		return c;
	if(env->posixkludge) {
		env->posixkludge = 0;
		if(c ==	'*')
			return c;	// * first in subexr
	}
	if(c == '\\') {
		if(n < 2)
			return T_BAD;
		c = env->cursor.p[1];
		if(c=='(' && env->retype==BRE)
			env->posixkludge = 1;
		else if(c==')' && env->retype==BRE && env->parnest==0)
			return T_BAD;
		if(escindex[c])
			return escape[escindex[c]].val[env->retype].esc;
		else
			return T_BAD;
	}
	if(c=='$' && n==1)
		return T_DOLL;
	else if(c=='^' && env->retype==BRE && env->cursor.p==env->expr.p) {
		env->posixkludge = 1;
		return T_CFLX;
	} else if(c==')' && env->retype!=BRE && env->parnest==0)
		return c;
	return escindex[c]? escape[escindex[c]].val[env->retype].unesc: c;
}
inline void
eat(Cenv *env)
{
	env->cursor.next(1 + (*env->cursor.p=='\\'));
}

static Rex*			// bracket expression
regbra(Cenv *env)
{
	Class *r = (Class*)NEW(Class);
	Set set;
	int c, i, neg, last, inrange, init;
	neg = 0;
	if(env->cursor.n>0 && *env->cursor.p=='^') {
		env->cursor.next();
		neg = 1;
	}
	if(env->cursor.n < 2)
		goto error;
	inrange = 0;	// 0=no, 1=possibly, 2=definitely
	for(init=1; ; init=0) {
		if(env->cursor.n <= 0)
			goto error;
		c = *env->cursor.p;
		env->cursor.next();
		if(c == ']') {
			if(init) {
				last = c;
				inrange = 1;
				continue;
			}
			if(inrange != 0)
				set.insert(last);
			if(inrange == 2)
				set.insert('-');
			break;
		} else if(c == '-') {
			if(inrange == 0 && !init)
				goto error;
			if(inrange == 1) {
				inrange = 2;
				continue;
			}
		} else if(c == '[') {
			if(env->cursor.n < 2)
				goto error;
			c = *env->cursor.p;
			switch(c) {
			case ':':
				env->cursor.next();
				if(inrange == 1)
					set.insert(last);
				if(!getcharcl(c, &set, env))
					goto error;
				inrange = 0;
				continue;
			case '=':
				env->cursor.next();
				if(inrange == 2)
					goto error;
				if(inrange == 1)
					set.insert(last);
				i = findcollelem(c, env);
				if(i == -1)
					goto error;
				set.insert(i);
				inrange = 0;
				continue;
			case '.':
				env->cursor.next();
				c = findcollelem(c, env);
				if(c == -1)
					goto error;
				break;
			default:
				c = '[';
			}
		}
		if(inrange == 2) {
			if(last > c)
				goto error;
			for(i=last; i<=c; i++)
				set.insert(i);
			inrange = 0;
		} else if(inrange == 1)
			set.insert(last);
		else
			inrange = 1;
		last = c;
	}
	r->orset(&set);
	r->icase(env->map);
	if(neg)
		r->neg(env->flags);
	return r;
error:
	delete r;
	return ERROR;
}

static Rex*
regRep(Rex *e, int n1, int n2, Cenv *env)
{
	if(cdebug)
		printf("regRep %lx.%d\n",
			(unsigned long)env->cursor.p,env->cursor.n);
	int c;
	unsigned long m = 0;
	unsigned long n = RE_DUP_INF;
	char *sp, *ep;
	if(e == ERROR)
		return e;
	c = token(env);
	switch(c) {
	default:
		return e;
	case T_BANG:
		eat(env);
		return NEW(Neg(e));
	case T_QUES:
		n = 1;
		eat(env);
		break;
	case T_STAR:
		eat(env);
		break;
	case T_PLUS:
		m = 1;
		eat(env);
		break;
	case T_LEFT:
		eat(env);
		errno = 0;
		sp = (char*)env->cursor.p;
		n = m = strtoul(sp, &ep, 10);
		if(ep == sp || ep-sp >= env->cursor.n)
			goto error;
		if(*ep == ',') {
			sp = ep + 1;
			if(*sp == '\\' || *sp == '}') {
				n = RE_DUP_INF;
				ep = sp;
			} else {
				n = strtoul(sp, &ep, 10);
				if(ep == sp || n > RE_DUP_MAX)
					goto error;
				if(ep- (char*)env->cursor.p >= env->cursor.n)
					goto error;
			}
		}
		env->cursor.next(ep - (char*)env->cursor.p);
		if(errno || m > n || m > RE_DUP_MAX)
			goto error;
		else if(env->flags&REG_EXTENDED) {
			if(token(env) != '}')
				goto error;
		} else if(token(env) != T_RIGHT)
				goto error;
		eat(env);
		break;
	}
	switch(e->type) {
	case DOT:
	case CLASS:
	case ONECHAR:
		((Dup*)e)->lo = (int)m;
		((Dup*)e)->hi = (int)n;
		return e;
	}
	return NEW(Rep((int)m, (int)n, n1, n2, e));
error:
	delete e;
	return ERROR;
}

/* combine e and f into a sequence, collapsing them if
   either is Ok, or if both are Dots. */

static Rex *
mkSeq(Rex *e, Rex *f)
{
	Rex *g;
	if(f == ERROR) {
		delete e;
		return f;
	} else if(e->type == OK) {
		delete e;
		return f;
	} else if(f->type == OK) {
		g = (Rex*)f->next;
		f->next = 0;
		delete f;
		f = g;
	} else if(e->type==DOT && f->type==DOT) {
		unsigned m = ((Dot*)e)->lo + ((Dot*)f)->lo;
		unsigned n = ((Dot*)e)->hi + ((Dot*)f)->hi;
		if(m <= RE_DUP_MAX) {
			if(((Dot*)e)->hi > RE_DUP_MAX ||
			   ((Dot*)f)->hi > RE_DUP_MAX) {
				n = RE_DUP_INF;	
				goto n_ok;
			} else if(n <= RE_DUP_MAX) { // unless ovfl,
			n_ok:	((Dot*)e)->lo = m;   // combine
				((Dot*)e)->hi = n;
				g = (Rex*)f->next;
				f->next = 0;
				delete f;
				f = g;
			}
		}
	}
	e->next = f;
	return e;
}

static Rex *regSeq(Cenv *env);
static Rex *regConj(Cenv *env);
static Rex *regTrie(Rex *e, Rex *f, Cenv *env);

static Rex *
regAlt(int n1, Cenv *env) // n1= no. of 1st subexpr in alt
{
	if(cdebug)
		printf("regAlt %lx.%d parno=%d\n",
			(unsigned long)env->cursor.p, env->cursor.n, env->parno);
	Rex *e = regConj(env);
	if(e == ERROR)
		return e;
	else if(token(env) != T_BAR)
		return e;
	eat(env);
	Rex *f = regAlt(n1, env);
	if(f == ERROR) {
		delete e;
		return f;
	}
	Rex *g = regTrie(e, f, env);
	if(g != ERROR)
		return g;
	if(e->type==OK || f->type==OK)
		goto bad;
	g = NEW(Alt(n1, env->parno, e, f));
	if(g != ERROR)
		return g;
bad:
	delete e;
	delete f;
	return ERROR;
}

static Rex*
regConj(Cenv *env)
{
	if(cdebug)
		printf("regConj %lx.%d parno=%d\n",
			(unsigned long)env->cursor.p, env->cursor.n, env->parno);
	Rex *e = regSeq(env);
	if(env->retype != ARE)
		return e;
	if(e == ERROR)
		return e;
	if(token(env) != T_AND)
		return e;
	eat(env);
	Rex *f = regConj(env);
	if(f == ERROR) {
		delete e;
		return f;
	}
	Rex *g = NEW(Conj(e, f));
	if(g == ERROR){
		delete e;
		delete f;
	}
	return g;
}

/* regTrie tries to combine nontrivial e and f into a Trie. unless
   ERROR is returned, e and f are deleted as far as possible */

static int
isstring(Rex *e)
{
	switch(e->type) {
	case KMP:
	case KR:
	case STRING:
		return 1;
	case ONECHAR:
		return ((Onechar*)e)->lo==1 && ((Onechar*)e)->hi==1;
	}
	return 0;
}
static int
insert(Rex *f, Trie *g)
{
	uchar temp[2];
	switch(f->type) {
	case KMP:
	case KR:
	case STRING:
		 return g->insert(((String*)f)->seg.p);
	case ONECHAR:
		temp[0] = ((Onechar*)f)->c;
		temp[1] = 0;
		return g->insert(temp);
	}
	return 1;	// shouldn't happen
}
static Rex *
regTrie(Rex *e, Rex *f, Cenv *env)
{
	Trie *g = (Trie*)f;
	if(e->next || f->next || !isstring(e))
		return ERROR;
	if(isstring(f)) {
		g = (Trie*)NEW(Trie());		// env is used here
		if(g == ERROR)
			return ERROR;
		if(insert(f, g))
			goto nospace;
	} else if(f->type != TRIE)
		return ERROR;
	if(insert(e, g))
		goto nospace;
	delete e;
	if(f != g)
		delete f;
	return g;
nospace:
	if(g != f)
		delete g;
	return ERROR;
}

static Rex *
regSeq(Cenv *env)
{
	Rex *e, *f, *g;
	int c, parno;
	uchar ch;
	uchar data[101];
	if(cdebug)
		printf("regSeq %lx.%d parno=%d\n",
			(unsigned long)env->cursor.p, env->cursor.n, env->parno);
	Seg string(data, 0);
	for( ;  ; ch = c) {	// get string
		c = token(env);
		if(c>T_META || (unsigned)string.n>=sizeof(data)-1)
			break;
		string.p[string.n++] = c;
		eat(env);
	}
	if(c == T_BAD)
		return ERROR;
	if(string.n > 0) switch(c) {
	case T_STAR:
	case T_PLUS:
	case T_LEFT:
	case T_QUES:
	case T_BANG:
		string.n--;
		if(string.n < 0)
			return ERROR;
		if(string.n == 0)
			e = NEW(Ok);
		else {
			Seg copy = string.copy();
			if(copy.p == 0)
				return ERROR;
			e = NEW(String(copy, env->map));
		}
		f = NEW(Onechar(env->map[ch]));
		f = regRep(f, 0, 0, env);
		if(f == ERROR) {
			delete e;
			return f;
		}
		g = regSeq(env);
		return mkSeq(e, mkSeq(f, g));
	default:
		e = NEW(String(string.copy(), env->map));
		f = regSeq(env);
		return mkSeq(e, f);
	} else if(c > T_BACK) {
		eat(env);
		c -= T_BACK;
		if(c>env->parno || env->paren[c] == 0)
			return ERROR;
		env->backref |= 1<<c;
		e = regRep(NEW(Back(c)), 0, 0, env);
	} else switch(c) {
	case T_AND:
	case T_CLOSE:
	case T_BAR:
	case T_END:
		return NEW(Ok);
	case T_DOLL:
		eat(env);
		e = regRep(NEW(End), 0, 0, env);
		break;
	case T_CFLX:
		eat(env);
		e = NEW(Anchor);
		if(env->flags&REG_EXTENDED)
			e = regRep(e, 0, 0, env);
		break;
	case T_OPEN:
		eat(env);
		++env->parnest;
		parno = ++env->parno;
		e = regAlt(parno+1, env);
		if(e == ERROR)
			break;
		if(e->type==OK && env->flags&REG_EXTENDED) {
			delete e;
			return ERROR;
		} 
		if(token(env) != T_CLOSE) {
			delete e;
			return ERROR;
		}
		--env->parnest;
		eat(env);
		if(parno <= BACK_REF_MAX)
			env->paren[parno] = 1;
		e = NEW(Subexp(parno, e));
		if(e == ERROR)
			break;
		e = regRep(e, parno, env->parno, env);
		break;
	case T_BRA:
		eat(env);
		e = regRep(regbra(env), 0, 0, env);
		break;
	case T_DOT:
		eat(env);
		e = regRep(NEW(Dot), 0, 0, env);
		break;
	default:
		return ERROR;
	}
	if(e != ERROR && env->cursor.n > 0)
		e = mkSeq(e, regSeq(env));
	return e;
}


/* rewrite the expression tree for some special cases.
   1. it is a null expression - illegal
   2. it begins with an unanchored string - use KMP algorithm
   3. it begins with .* or ^ - regexec only need try it ONCE
   4. it begins with one of the above parenthesized and unduplicated
*/		

static int
special(regex_t *preg, Cenv *env)
{
	Rex* kmp;
	Rex* rex = preg->rex;
	String *string;
	if(rex == ERROR)
		return 0;
	switch(rex->type) {
	case SUBEXP:
		for(;;) {
			rex = ((Subexp*)rex)->rex;
			switch(rex->type) {
			case SUBEXP:
				continue;
			case DOT:
				goto dot;
			case ANCHOR:
				goto anchor;
			default:
				return 0;
			}
		}
	dot:
	case DOT:			// .*
		if(((Dot*)rex)->lo==0 && ((Dot*)rex)->hi==RE_DUP_INF)
			return ONCE;
		return 0;
	case OK:			// empty regexp
		if(env->flags & REG_NULL)
			return ONCE;
		regfree(preg);
		return 0;
	case STRING:
		if(env->flags & (REG_ANCH | REG_LITERAL))
			return 0;
		string = (String*)rex;
		kmp = NEW(Kmp(string->seg, &env->flags));
		if(kmp==ERROR || env->flags&SPACE) 
			return 0;
		kmp->next = rex->next;
		preg->rex = kmp;
		string->seg = Seg(0,0);
		string->next = 0;
		delete string;
		return ONCE;
	anchor:
	case ANCHOR: 
		if(!(env->flags & REG_NEWLINE))
			return ONCE;
	}
	return 0;
}		

int
regcomp(regex_t *preg, const char *pattern, int cflags)
{
	preg->rex = 0;
	if(Done::done==0 && (Done::done=new Done)==0)
		return REG_ESPACE;
	if(cflags & REG_AUGMENTED)
		cflags |= REG_EXTENDED;
	cflags &= CFLAGS|GFLAGS;
	Cenv env(pattern, cflags);
	if(env.flags&SPACE)
		return REG_ESPACE;

	preg->rex = regAlt(1, &env);
	cflags |= special(preg, &env);
	if(preg->rex == ERROR)
		return env.flags&SPACE? REG_ESPACE: REG_BADPAT;

	preg->rex->serialize(1);
	Stat st = preg->rex->stat(&env);
	if(st.o) {
		regfree(preg);
		return REG_ESPACE;
	}
	cflags |= hard(&st);
	if(cflags & REG_ANCH)
		cflags |= ONCE;
	preg->flags = cflags;
	preg->re_nsub = st.p;
	preg->map = env.map;
	return 0;
}

void
regfree(regex_t *preg)
{
	delete preg->rex;
	preg->rex = ERROR;
}

size_t
regerror(int errcode, const regex_t*, char *errbuf, size_t errbuf_size)
{
	if(errbuf_size == 0)
		return 0;
	const char *s = "unknown error";
	switch(errcode) {
	case 0:
		s = "success";
		break;
	case REG_NOMATCH:
		s = "no match";
		break;
	case REG_ESPACE:
		s = "out of space";
		break;
	case REG_BADPAT:
	case REG_ECOLLATE:
	case REG_ECTYPE:
	case REG_EESCAPE:
	case REG_ESUBREG:
	case REG_EBRACK:
	case REG_EPAREN:
	case REG_EBRACE:
	case REG_BADBR:
	case REG_ERANGE:
	case REG_BADRPT:
		s = "improper regular expression";
		break;
	}
	strncpy(errbuf, s, errbuf_size);
	errbuf[errbuf_size-1] = 0;
	return 1+strlen(errbuf);
}

/* combine two regular expressions if possible,
   replacing first with the combination and freeing second.
   return 1 on success.
   the only combinations handled are building a Trie
   from String|Kmp|Trie and String|Kmp */

int
regcomb(regex_t *preg0, regex_t *preg1)
{
	if(cdebug)
		printf("regcomb\n");
	Rex *rex0 = preg0->rex;
	Rex *rex1 = preg1->rex;
	Cenv env(preg0->flags);
	if(rex0==ERROR || rex0->next ||
	   rex1==ERROR || rex1->next)
		return 0;
	Rex *g = regTrie(rex1, rex0, &env);
	if(g == 0)
		return 0;
	preg0->rex = g;
	if((preg0->flags&REG_ANCH) == 0)
		preg0->flags &= ~ONCE;
	preg1->rex = ERROR;
	return 1;
}
