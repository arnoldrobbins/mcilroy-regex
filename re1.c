#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "re.h"

/* regular expression recognizer. parse() is coded in
   continuation-passing style.
   see re.h for other descriptive comments */

#ifdef DEBUG
int edebug = 0;	// OR of types to be traced in parsing
#define debug(type, msg, s) \
	if(edebug & (1<<type)) dprint(msg, s); else
#else
#define edebug 0
#define debug(type, msg, s)
#endif

/* Pos is for comparing parses. An entry is made in the
   array at the beginning and at the end of each Rep,
   each iteration in a Rep, and each Alt
*/

struct Pos {
	uchar *p;	// where in string
	short serial;	// subpattern number in preorder
	uchar be;	// which end of pair
};
enum {
	BEGR,		// beginning of a repetition
	BEGI,		// beginning of one iteration of a rep
	BEGA,		// beginning of an alt
	BEGS,		// beginning of a subexpression
	ENDP		// end of any of above
};

/* returns from parse(). seemingly one might better handle
   BAD by longjmp, but that would not work with threads
   and it would skip the ~Save destructor.  as for
   possibly using exception handling ... */

enum {	NONE,		// no parse found
	GOOD,		// some parse was found
	BEST,		// an unbeatable parse was found
	BAD		// error ocurred
};

/* execution environment.  would be more efficient if it
   were in static store.  kept on stack so it will run
   under multiple threads */

struct Eenv {
	int flags;		// compile and exec flags
	const regex_t *preg;	// the 
	uchar *p;		// beginning of string
	uchar *last;		// end of string
	int npos;		// how much of pos is used
	int nbestpos;		// ditto for bestpos
	Array<Pos> pos;		// posns of certain subpatterns
	Array<Pos> bestpos;	// ditto for best match
	Array<regmatch_t> match;// subexrs in current match 
	Array<regmatch_t> best;	// ditto in best match yet
	Eenv(const regex_t *preg, int eflags, uchar *string, size_t len);
	int pushpos(Rex*, uchar*, int);
	void poppos() { npos--; }
};

int Eenv::pushpos(Rex *rex, uchar *p, int b_e )
{
	if(pos.assure(npos+1))	// +1 is probably superstition
		return 1;
	pos[npos].serial = rex->serial;
	pos[npos].p = p;
	pos[npos].be = b_e;
	npos++;
	return 0;
}

#ifdef DEBUG
static void printpos(Pos *pos, int n, Eenv *env)	/* for debugging */
{
	int i;
	for(i=0; i<n; i++) {
		switch(pos[i].be) {
		case BEGI: printf("(I"); break;
		case BEGA: printf("(A"); break;
		case BEGR: printf("(R"); break;
		case BEGS: printf("(S"); break;
		}
		printf("%d,%d", pos[i].serial, pos[i].p-env->p);
		if(pos[i].be == ENDP) printf(")");
		printf(" ");
	}
	printf("\n");
	fflush(stdout);
}
#else
#define printpos(pos, n, env)
#endif

static regmatch_t NOMATCH = { -1, -1 };

inline
Eenv::Eenv(const regex_t *preg, int eflags, uchar *string, size_t len) :
	preg(preg), p(string), last(string+len),
	flags(eflags&EFLAGS | preg->flags)
{
	int n = preg->re_nsub;
	if(match.assure(n) || best.assure(n)) {
		flags |= SPACE;
		return;
	}
	npos = nbestpos = 0;
	best[0].rm_so = 0;
	best[0].rm_eo = -1;
}

Seg Seg::copy()
{
	Seg seg(new uchar[n+1], n);
	if(seg.p) {
		memmove(seg.p, p, (size_t)n);
		seg.p[n] = 0;
	}
	return seg;
}

void Set::insert(uchar c)
{
	cl[c/CHAR_BIT] |= 1 << (c%CHAR_BIT);
}
void Set::neg()
{
	int i;
	for(i=0; i<sizeof(cl); i++)
		cl[i] ^= ~0;
}
void Set::or(Set *y)
{
	int i;
	for(i=0; i<sizeof(cl); i++)
		cl[i] |= y->cl[i];
}
void Set::clear()
{
	memset(cl, 0, sizeof(cl));
}

#ifdef DEBUG
int mallocbytes;
int mallocblocks = -1;	// -1 accounts for Done::done
int freeblocks;
void *operator new(size_t size) {
	void *p = malloc(size);
	if(p) {
		mallocbytes += size;
		mallocblocks++;
	}
	return p;
}
void operator delete(void *p) {
	if(p) {
		freeblocks++;
		free(p);
	}
}
#endif

Rex::~Rex()
{
	if(type!=TEMP)
		delete next;
}

void Rex::dprint(char *msg, uchar *s)
{
	printf("%s _", msg);
	print();
	printf("_ _%s_\n", s);
}
void Rex::print() { }

#ifdef DEBUG		// debuggint output routines
extern "C" void jprint(regex_t *preg)	
{
	preg->rex->print();
	printf("\n");
}

void flagprint(regex_t *re)
{
	int flags = re->flags;
	if(flags&REG_EXTENDED) printf("EXTENDED:");
	if(flags&REG_AUGMENTED) printf("AUGMENTED:");
	if(flags&REG_ICASE) printf("ICASE:");
	if(flags&REG_NOSUB) printf("NOSUB:");
	if(flags&REG_NEWLINE) printf("NEWLINE:");
	if(flags&REG_NOTBOL) printf("NOTBOL:");
	if(flags&REG_NOTEOL) printf("NOTEOL:");
	if(flags&REG_NULL) printf("NULL:");
	if(flags&REG_ANCH) printf("ANCH:");
	if(flags&REG_LITERAL) printf("LITERAL:");
	if(flags&HARD) printf("HARD:");
	if(flags&ONCE) printf("ONCE:");
}

void Dup::print()
{
	if(lo == 1 && hi == 1)
		;
	else if(lo == 0 && hi == RE_DUP_INF)
		printf("*");
	else if(hi == lo)
		printf("\\{%d\\}", lo);
	else if(hi == RE_DUP_INF)
		printf("\\{%d,\\}", lo);
	else
		printf("\\{%d,%d\\}", lo, hi);
	if(next)
		next->print();
}
void Ok::print() {
	if(next)
		next->print();
}
void Anchor::print() {
	printf("^");
	if(next)
		next->print();
}
void End::print() {
	printf("$");
	if(next)
		next->print();
}
void Dot::print()
{
	printf(".");
	this->Dup::print();
}
void Onechar::print()
{
	printf("%c", c);
	this->Dup::print();
}
void Class::print()
{
	int i;
	printf("[");
	for(i=0; i<128; i++)
		if(in(i))
			printf(isprint(i)?"%c":"\\x%.2x", i);
	printf("]");
	this->Dup::print();
}
void String::print()
{
	printf("%.*s", seg.n, seg.p);
	if(next)
		next->print();
} 
void Trie::print()
{
	int i;
	Array<uchar> s;
	int count = 0;
	for(i=0; i<elementsof(root); i++)
		if(root[i]) {
			if(count++)
				printf("|");
			print(root[i], 0, s);
		}
}
void Trie::print(Tnode *node, int n, Array<uchar> &s)
{
	for(;;) {
		s.assure(n);
		s[n] = node->c;
		if(node->son) {
			print(node->son, n+1, s);
			if(node->end)
				printf("|");
		}
		if(node->end)
			printf("%.*s", n+1, &s[0]);
		node = node->sib;
		if(node == 0)
			return;
		printf("|");
	}
}
void Back::print()
{
	printf("\\%d", n);
	if(next)
		next->print();
}
void Subexp::print()
{
	printf("\\(");
	rex->print();
	printf("\\)");
	if(next)
		next->print();
}
void Alt::print()
{
	left->print();
	printf("|");
	right->print();
}
void Conj::print()
{
	left->print();
	printf("&");
	right->print();
}
void Rep::print()
{
	rex->print();
	this->Dup::print();
}
void Neg::print()
{
	rex->print();
	printf("!");
	if(next)
		next->print();
}
#endif

int Rex::parse(uchar*, Rex*, Eenv*)	// pure virtual, avoid libC++
{
	abort();			// "can't happen"
	return 0;
}

inline int Rex::follow(uchar *s, Rex *cont, Eenv *env)
{
	return next? next->parse(s, cont, env):
		     cont->parse(s, 0, env);
}

int Ok::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(OK, "Ok", s);
	return follow(s, cont, env);
}

int Anchor::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(ANCHOR, "Anchor", s);
	if((env->flags&REG_NEWLINE) &&
	   s>env->p && s[-1]=='\n' ||
	   !(env->flags&REG_NOTBOL) && s==env->p)
		return follow(s, cont, env);
	return NONE;
}

int End::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(END, "End", s);
	if((*s==0 && !(env->flags&REG_NOTEOL)) ||
	    (env->flags&REG_NEWLINE) && *s=='\n')
		return follow(s, cont, env);
	return NONE;
}

int Dot::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(DOT, "Dot", s);
	int n = hi;
	if(n > env->last-s)
		n = env->last-s;
	if(env->flags&REG_NEWLINE) {
		for(int i=0 ; i<n; i++)
			if(s[i] == '\n')
				n = i;
	}
	int result = NONE;
	for(s+=n; n-->=lo; s--)
		switch(follow(s, cont, env)) {
		case BEST:
			return BEST;
		case BAD:
			return BAD;
		case GOOD:
			result = GOOD;
		}
	return result;
}

int Onechar::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(ONECHAR, "Onechar", s);
	int n = hi;
	uchar *map = env->preg->map;
	if(n > env->last-s)
		n = env->last-s;
	int i = 0;
	for( ; i<n; i++,s++)
		if(map[*s] != c)
			break;
	int result = NONE;
	for( ; i-->=lo; s--)
		switch(follow(s, cont, env)) {
		case BEST:
			return BEST;
		case BAD:
			return BAD;
		case GOOD:
			result = GOOD;
		}
	return result;
}

int Class::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(CLASS, "Class", s);
	int n = hi;
	if(n > env->last-s)
		n = env->last-s;
	for(int i=0; i<n; i++)
		if(!cl.in(s[i]))
			n = i;
	int result = NONE;
	for(s+=n; n-->=lo; s--)
		switch(follow(s, cont, env)) {
		case BEST:
			return BEST;
		case BAD:
			return BAD;
		case GOOD:
			result = GOOD;
		}
	return result;
}
void Class::or(Set *y)
{
	cl.or(y);
}
void Class::neg(int cflags)
{
	cl.neg();
	if(cflags&REG_NEWLINE)
		cl.cl['\n'/CHAR_BIT] &= ~(1 << ('\n'%CHAR_BIT));
}
void Class::icase(uchar *map)
{
	if(map['A'] != map['a'])
		return;
	for(int i=0; i<256; i++)
		if(cl.in(i)) {
			cl.insert(toupper(i));
			cl.insert(tolower(i));
		}
}

String::String(Seg s, uchar *map) : Rex(STRING), seg(s)
{
	uchar *p;
	if(map)
		for(p=seg.p; *p; p++)
			*p = map[*p];
}
int String::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(STRING, "String", s);
	if(s+seg.n > env->last)
		return NONE;
	uchar *map = env->preg->map;
	uchar *p = seg.p;
	while(*p)
		if(map[*s++] != *p++)
			return NONE;
	return follow(s, cont, env);
}

/* Knuth-Morris-Pratt, adapted from Corman-Leiserson-Rivest */
Kmp::Kmp(Seg seg, int *flags) : String(seg)
{
	type = KMP;
	if(fail.assure(seg.n)) {
		*flags |= SPACE;
		return;
	}
	int q, k ;
	fail[0] = k = -1;
	for(q=1; q<seg.n; q++) {
		while(k>=0 && seg.p[k+1] != seg.p[q])
			k = fail[k];
		if(seg.p[k+1] == seg.p[q])
			k++;
		fail[q] = k;
	}
}
Kmp::parse(uchar *s, Rex* cont, Eenv *env)
{
	debug(KMP, "Kmp", s);
	uchar *map = env->preg->map;
	uchar *t = s;
	uchar *last = env->last;
	while(t+seg.n <= last) {
		int k = -1;
		for( ; t<last; t++) {
			while(k>=0 && seg.p[k+1] != map[*t])
				k = fail[k];
			if(seg.p[k+1] == map[*t])
				k++;
			if(k+1 == seg.n) {
				env->best[0].rm_so = ++t - s - seg.n;
				switch(follow(t, cont, env)) {
				case GOOD:
				case BEST:
					return BEST;
				case BAD:
					return BAD;
				}
				t -= seg.n - 1;
				break;
			}
		}
	}
	return NONE;
}	


int Trie::parse(uchar *s, Rex *contin, Eenv *env)
{
	Tnode *node = root[env->preg->map[*s]&MASK];
	if(node==0 || s+min>env->last)
		return NONE;
	return parse(node, s, contin, env);
}
int Trie::parse(Tnode *node, uchar *s, Rex* contin, Eenv *env)
{
	debug(TRIE, "Trie", s);
	uchar *map = env->preg->map;
	for(;;) {
		if(s >= env->last)
			return NONE;
		while(node->c != map[*s]) {
			node = node->sib;
			if(node == 0)
				return NONE;
		}
		if(node->end)
			break;
		node = node->son;
		s++;
	}
	int longresult = NONE;
	if(node->son)
		longresult = parse(node->son, s+1, contin, env);
	if(longresult==BEST || longresult==BAD)
		return longresult;
	int shortresult = follow(s+1, contin, env);
	return shortresult==NONE? longresult: shortresult;
}
/* returns 1 if out of space
   string s must be nonempty */
int Trie::insert(uchar *s)
{
	int len;
	Tnode *node = root[*s&MASK];
	if(node == 0)
		node = root[*s&MASK] = new Tnode(*s);
	for(len=1; ; ) {
		if(node == 0)
			return 1;
		if(node->c == *s) {
			if(s[1] == 0)
				break;
			if(node->son == 0)
				node->son = new Tnode(s[1]);
			node = node->son;
			len++;
			s++;
		} else {
			if(node->sib == 0)
				node->sib = new Tnode(*s);
			node = node->sib;
		}	
	}
	if(len < min)
		min = len;
	else if(len > max)
		max = len;
	node->end = 1;
	return 0;
}

int Back::parse(uchar *s, Rex *cont, Eenv *env)
{
	regmatch_t &m = env->match[n];
	debug(BACK, "Back", s);
	if(m.rm_so < 0)
		return NONE;
	uchar *p = env->p + m.rm_so;
	long n = m.rm_eo - m.rm_so;
	if(s+n > env->last)
		return NONE;
	uchar *map = env->preg->map;
	while(--n >= 0)
		if(map[*s++] != map[*p++])
			return NONE;
	return follow(s, cont, env);
}

struct Subexp1 : Rex {
	Rex *cont;
	Subexp *ref;
	Subexp1(Subexp *ref, Rex *cont) : ref(ref),
		cont(cont) { next = ref->next; }
	int parse(uchar*, Rex*, Eenv*);
};
int Subexp::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(SUBEXP, "Subexp", s);
	int result;
	regoff_t &so = env->match[n].rm_so;
	Subexp1 subexp1(this, cont);
	so = s - env->p;
	if(env->pushpos(this, s, BEGS))
		return BAD;
	result = rex->parse(s, &subexp1, env);
	env->poppos();
	so = -1;
	return result;
}
int Subexp1::parse(uchar *s, Rex*, Eenv *env)
{
	debug(SUBEXP, "Subexp1", s);
	int result;
	regoff_t &eo = env->match[ref->n].rm_eo;
	eo = s - env->p;
	if(env->pushpos(ref, s, ENDP))
		return BAD;
	result = follow(s, cont, env);
	env->poppos();
	eo = -1;
	return result;
}

/* save and restore match records around alternate attempts,
   so that fossils will not be left in the match array.
   (These are the only entries in the match array that
   are not otherwise guaranteed to have current data
   in them when they get used)  If there's too much
   to save, dynamically allocate space,
   The recognizer will slow to a crawl,
   allocating memory on every repetition
   but it will only happen if 20 parentheses
   occur under one * or in one alternation.
*/
struct Save {
	int n1, n2;
	Array<regmatch_t> area;
	Save(int n1, int n2, Eenv *env);
	void restore(Eenv *env);
};
Save::Save(int n1, int nn2, Eenv *env) : n1(n1), n2(nn2)
{
	regmatch_t *match = &env->match[0];
	if(n1 != 0) {
		int i = n2 - n1;
		if(area.assure(i)) {
			env->flags |= SPACE;
			n2 = n1;
			return;
		}
		regmatch_t *a = &area[0];
		match += n1;
		do {
			*a++ = *match;
			*match++ = NOMATCH;
		} while(--i >= 0);
	}
}

void Save::restore(Eenv *env)
{
	regmatch_t *match = &env->match[0];
	if(n1 != 0) {
		int i = n2 - n1;
		match += n1;
		regmatch_t *a = &area[0];
		do {
			*match++ = *a++;
		} while(--i >= 0);
	}
}

/* Alt1 is a catcher, solely to get control at the end of an
   alternative to keep records for comparing matches.
*/

struct Alt1 : Rex {
	Rex *cont;
	Alt1(Rex *cont, int ser) : cont(cont) { serial = ser; }
	int parse(uchar*, Rex*, Eenv*);
};
int Alt::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(ALT, "Altl", s);
	Save save(n1, n2, env);
	if(env->flags&SPACE)
		return BAD;
	if(env->pushpos(this, s, BEGA))
		return BAD;
	Alt1 alt1(cont, serial);
	int result = left->parse(s, &alt1, env);
	if(result!=BEST && result!=BAD) {
		debug(ALT, "Altr", s);
		save.restore(env);
		env->pos[env->npos-1].serial = rserial;
		alt1.serial = rserial;
		int rightresult = right->parse(s, &alt1, env);
		if(rightresult != NONE)
			result = rightresult;
	}
	env->poppos();
	save.restore(env);
	return result;
}
int Alt1::parse(uchar *s, Rex*, Eenv *env)
{
	if(env->pushpos(this, s, ENDP))
		return BAD;
	int result = follow(s, cont, env);
	env->poppos();
	return result;
}

struct Conj2: Rex {		// right catcher
	uchar *last;		// end of left match
	Rex *cont;		// ambient continuation
	int parse(uchar*, Rex*, Eenv*);
	Conj2(Rex *cont, Rex *nex) : cont(cont) { next = nex; }
};	
struct Conj1 : Rex {		// left catcher
	uchar *p;		// beginning of left match
	Rex *right;		// right pattern
	Conj2 *conj2p;		// right catcher
	Conj1(uchar *p, Rex *right, Conj2 *conj2p) :
		p(p), right(right), conj2p(conj2p) { }
	int parse(uchar*, Rex*, Eenv*);
};
int Conj::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(CONJ, "Conjl", s);
	Conj2 conj2(cont, next);
	Conj1 conj1(s, right, &conj2);
	return left->parse(s, &conj1, env);
}
int Conj1::parse(uchar *s, Rex*, Eenv *env)
{
	debug(CONJ, "Conjr", p);
	conj2p->last = s;
	return right->parse(p, conj2p, env);
}
int Conj2::parse(uchar *s, Rex*, Eenv *env)
{
	if(s != last)
		return NONE;
	return follow(s, cont, env);
}

/* Rep1 nodes are catchers.  One is created on the stack for
   each iteration of a complex repetition.
*/

struct Rep1 : Rex {
	struct Rep *ref;	// where the original node is
	uchar *p1;		// where this iteration began
	int n;			// iteration count
	Rex *cont;
	Rep1(Rep *ref, uchar *p1, int n, Rex *cont)
		: ref(ref), p1(p1), n(n), cont(cont) {
		next = ref->next; serial=ref->serial; }
	int parse(uchar *, Rex*, Eenv*);
};
int Rep::dorep(int n, uchar *s, Rex *cont, Eenv *env)
{
	int result = NONE;
	if(hi > n) {
		Rep1 rep1(this, s, n+1, cont);
		Save save(n1, n2, env);
		if(env->flags&SPACE)
			return BAD;
		if(env->pushpos(this, s, BEGI))	
			return BAD;
		result = rex->parse(s, &rep1, env);
		env->poppos();
		save.restore(env);
	}
	if(result==BEST || result==BAD || lo>n)
		return result;
	if(env->pushpos(this, s, ENDP))	// end BEGR
		return BAD;
	int res1 = follow(s, cont, env);
	env->poppos();
	return res1==NONE? result: res1;
}
int Rep1::parse(uchar *s, Rex*, Eenv *env)
{
	int result;
	debug(REP, "Rep1", s);
	if(env->pushpos(this, s, ENDP))	// end BEGI
		return BAD;
	if(s==p1 && n>ref->lo)		// optional empty iteration
		if(env->flags&REG_EXTENDED || (env->flags&HARD) == 0)
			result = NONE;	// unwanted
		else if(env->pushpos(this, s, ENDP))	// end BEGR
				return BAD;
		else {
			result = follow(s, cont, env);
			env->poppos();
		}
	else
		result = ref->dorep(n, s, cont, env);
	env->poppos();
	return result;
}
int Rep::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(REP, "Rep", s);
	if(env->pushpos(this, s, BEGR))
		return BAD;
	int result = dorep(0, s, cont, env);
	env->poppos();
	return result;
}

/* Neg1 catcher determines what string lengths can be matched,
   then Neg investigates continuations of other lengths.
   this is inefficient.  for EASY expressions, we can do better:
   since matches to rex will be enumerated in decreasing order,
   we can investigate continuations whenever a length is
   skipped.   */

struct Neg1 : Rex {
	uchar *p;		// start of negated match
	Array<char> index;	// bit array of string sizes seen
	Neg1(uchar *p, int n);
	int parse(uchar *s, Rex*, Eenv*);
	void bitset(int n) {
		index[n/CHAR_BIT] |= 1<<(n%CHAR_BIT); }
	int bittest(int n) {
		return index[n/CHAR_BIT] & (1<<(n%CHAR_BIT)); }
};
int Neg::parse(uchar *s, Rex *cont, Eenv *env)
{
	debug(NEG, "Neg", s);
	int n = env->last - s;
	Neg1 neg1(s, n);
	if(rex->parse(s, &neg1, env) == BAD)
		return BAD;
	int result = NONE;
	for( ; n>=0; n--) {
		if(neg1.bittest(n))
			continue;
		int res1 = follow(s+n, cont, env);
		if(res1==BAD || res1==BEST)
			return res1;
		if(res1 == GOOD)
			result = GOOD;
	}
	return result;
}
Neg1::Neg1(uchar *p, int n) : p(p)
{
	n = (n+CHAR_BIT-1)/CHAR_BIT;
	index.assure(n);
	memset(index.p,0,n+1);
}
int Neg1::parse(uchar *s, Rex*, Eenv*)
{ 
	debug(NEG, "Neg1", s);
	bitset(s-p);
	return NONE;
}


static Pos *rpos(Pos *a)	/* find matching right pos record */
{
	int serial = a->serial;
	int inner;
	for(inner=0;;) {
		if((++a)->serial != serial)
			continue;
		if(a->be != ENDP)
			inner++;
		else if(inner-- <= 0)
			return a;
	}
}

/* two matches are known to have the same length
   os is start of old pos array, ns is start of new,
   oend and nend are end+1 pointers to ends of arrays.
   oe and ne are ends (not end+1) of subarrays.
   returns 1 if new is better, -1 if old, else 0 */

static int better(Pos *os, Pos *ns, Pos *oend, Pos *nend)
{
	Pos *oe, *ne;
	int k;
	for( ; os<oend && ns<nend; os=oe+1, ns=ne+1) {
		if(ns->serial > os->serial)
			return -1;
		if(os->serial > ns->serial)
			abort();	// folk theorem bites the dust
		if(os->p > ns->p)
			return -1;
		if(ns->p > os->p)
			return 1;	// believed impossible
		oe = rpos(os);
		ne = rpos(ns);
		if(ne->p > oe->p)
			return 1;
		if(oe->p > ne->p)
			return -1;
		k = better(os+1, ns+1, oe, ne);
		if(k)
			return k;
	}
	if(ns < nend)
		abort();		// another one bites the dust
	return os < oend;		// true => inessential null
}

int Done::parse(uchar *s, Rex*, Eenv *env)
{
	if(edebug & (1<<DONE)) {
		dprint("Done", s);
		printpos(&env->pos[0], env->npos, env);
	}
	if(env->flags&REG_ANCH && s!=env->last)
		return NONE;
	if(env->flags & REG_NOSUB)
		return BEST;
	int n = s - env->p;
	int nsub = env->preg->re_nsub;
	if((env->flags&HARD) == 0) {
		env->best[0].rm_eo = n;
		memmove(&env->best[1], &env->match[1],
			nsub*sizeof(regmatch_t));
		return BEST;
	}
	if(env->best[0].rm_eo >= 0) {	/* only happens on HARD */
		long d = env->best[0].rm_eo;
		if(n < d)
			return GOOD;
		if(n == d) {
			if(edebug & (1<<DONE))
				printpos(&env->bestpos[0],
					 env->nbestpos, env);
		   	d = better(&env->bestpos[0],
				   &env->pos[0],
				   &env->bestpos[env->nbestpos],
				   &env->pos[env->npos]);
			if(d <= 0)
				return GOOD;
		}
	}
	env->best[0].rm_eo = n;
	memmove(&env->best[1], &env->match[1],
		nsub*sizeof(regmatch_t));
	n = env->npos;
	if(env->bestpos.assure(n)) {
		env->flags |= SPACE;
		return BAD;
	}
	env->nbestpos = n;
	memmove(&env->bestpos[0], &env->pos[0], n*sizeof(Pos));
	return GOOD;
}

/* regnexec is a side door for use when string length is known.
   returning REG_BADPAT or REG_ESPACE is not explicitly
    countenanced by the standard. */

int regnexec(const regex_t *preg, const char *string, size_t len,
	     size_t nmatch, regmatch_t *match, int eflags)
{
	int i;
	if(preg->rex == 0)	// not required, but kind
		return REG_BADPAT;
	Eenv env(preg, eflags, (uchar*)string, len);
	if(env.flags&SPACE)
		return REG_ESPACE;
	if(env.flags&REG_NOSUB)
		nmatch = 0;
	for(i=0; i<nmatch && i<=preg->re_nsub; i++)
		env.match[i] = NOMATCH;

	while(preg->rex->parse((uchar*)string,Done::done,&env) == NONE) {
		if(env.flags & ONCE)
			return REG_NOMATCH;
		if((uchar*)++string > env.last)
			return REG_NOMATCH;
		env.best[0].rm_so++;
	}
	if(env.flags & SPACE)
		return REG_ESPACE;

	for(i=0; i<nmatch; i++)
		if(i <= preg->re_nsub)
			match[i] = env.best[i];
		else
			match[i] = NOMATCH;
	return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch,
	    regmatch_t *match, int eflags)
{
	const char *s = string;
	while(*s)
		s++;
	return regnexec(preg, string, s-string, nmatch, match, eflags);
}
