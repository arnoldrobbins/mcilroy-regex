/* Posix BRE/ERE recognizer. */

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "regex.h"
#include "array.h"

#undef RE_DUP_MAX			// posix puts this in limits.h!
enum {	RE_DUP_MAX  = -(INT_MIN/2)-1,	// 2*RE_DUP_MAX won't overflow
	RE_DUP_INF = RE_DUP_MAX + 1,	// infinity, for *
	BACK_REF_MAX = 9
};

#ifndef REG_NULL
#define REG_NULL 0
#endif
#ifndef REG_ANCH
#define REG_ANCH 0
#endif
#ifndef REG_LITERAL
#define REG_LITERAL 0
#endif
#ifndef REG_AUGMENTED
#define REG_AUGMENTED 0
#endif

/* it is believed that the codes defined in regex.h are
   contiguous, but their order is not recalled */

enum {	CFLAGS = REG_EXTENDED | REG_ICASE | REG_NOSUB | REG_NEWLINE,
	EFLAGS = REG_NOTBOL | REG_NOTEOL,
	GFLAGS = REG_NULL | REG_ANCH | REG_LITERAL | REG_AUGMENTED,
	ALLBIT0 = CFLAGS | EFLAGS | GFLAGS,
	NEWBIT1 = (ALLBIT0<<1) & ~ALLBIT0,
	NEWBIT2 = NEWBIT1 << 1,
	NEWBIT3 = NEWBIT2 << 1
};

typedef unsigned char uchar;

#define elementsof(x) (sizeof(x)/sizeof(x[0]))
#define ustrlen(s) strlen((char*)(s))
#define ustrncmp(a,b,n) strncmp((char*)(a), (char*)(b), n)
#define ustrchr(s,n) (uchar*)strchr((char*)(s), n)

	/* avoid dependence on the C++ library */
#ifdef DEBUG
extern int Rexmalloc;
void *operator new(size_t);
void operator delete(void*);
#define VIRTUAL virtual
extern void flagprint(regex_t*);
#else
inline void *operator new(size_t size) { return malloc(size); }
inline void operator delete(void *p) { free(p); }
#define VIRTUAL
#endif

struct Seg;

enum Type {
	OK,			// null string, used internally
	ANCHOR,			// initial ^
	END,			// final $
	DOT,			// .
	ONECHAR,		// a single-character literal
	STRING,			// some chars
	TRIE,			// alternation of strings
	CLASS,			// [...]
	BACK,			// \1, \2, etc
	SUBEXP,			// \(...\)
	ALT,			// a|b
	CONJ,			// a&b
	REP,			// Kleene closure
	NEG,			// negation
	KMP,			// Knuth-Morris-Pratt
	KR,			// modified Karp-Rabin
	DONE,			// completed match, used internally
	TEMP			// node kept on stack
};

enum {			// local environment flags
	SPACE = NEWBIT1,	// out of space
	EASY = 0,		// greedy match known to work
	HARD = NEWBIT2,		// otherwise
	ONCE = NEWBIT3		// if 1st parse fails, quit
};

struct Eenv;	// environment during regexec()
struct Cenv;	// environment during regcomp()
struct Stat;	// used during regcomp()

/* Rex is a node in a regular expression; TEMP nodes live
   temporarily on the stack during recognition by regexec;
   their next pointer should not be followed by ~Rex()
*/
struct Rex {
	uchar type;	// what flavor of Rex
	short serial;	// subpattern number
	Rex *next;	// following part of reg exp
	Rex(int type=TEMP) : type(type), next(0) { }
	virtual ~Rex();
	virtual Stat stat(Cenv*);
	virtual int serialize(int n);
	virtual int parse(uchar*, Rex*, Eenv*);
	VIRTUAL void print();
	int follow(uchar *s, Rex *cont, Eenv *env);
protected:
	void dprint(const char *, const uchar *);
};

struct Dup : Rex {	// for all duplicated expressions
	int lo, hi;
	Dup(int lo, int hi, int typ) :
		Rex(typ), lo(lo), hi(hi) { }
	Stat stat(Cenv*);
	void print();
};

/* A segment is a string pointer and length.  A length
   of -1 for a subexpression match means no match.
*/
struct Seg {
	uchar *p;
	int n;
	Seg() { }
	Seg copy();
	Seg(uchar *p, int n) : p(p), n(n) { }
	void next(int d=1) { p+=d; n-=d; }
	void prev(int d=1) { p-=d; n+=d; }
};

// A set of ascii characters, represented as a bit string

struct Set {
	uchar cl[(UCHAR_MAX+1)/CHAR_BIT];
	Set() { memset(cl,0,sizeof cl); }
	void insert(uchar c);
	int in(uchar c) { return (cl[c/CHAR_BIT]>>(c%CHAR_BIT)) & 1; }
	void orset(Set*);
	void neg();
	void clear();
};

/* various kinds of Rex.  Each has a recognizer .parse and
   a debugging .print function 

   A parsing function takes a Segment s, and a regular
   expression for the continuation of the parse.
   The environment has a match list in which are recorded the
   current strings for each referenceable subexpression */

struct Ok : Rex {
	Ok() : Rex(OK) { };
	int parse(uchar *, Rex*,Eenv*);
	void print();
};

struct Anchor : Rex {
	Anchor() : Rex(ANCHOR) { }
	int parse(uchar*, Rex*,Eenv*);
	void print();
};

struct End : Rex {
	End() : Rex(END) { }
	int parse(uchar*, Rex*,Eenv*);
	void print();
};

struct Dot : Dup {
	Dot(int lo=1, int hi=1) : Dup(lo, hi, DOT) { }
	int parse(uchar*, Rex*, Eenv*);
	void print();
};

struct Class : Dup {
	Set cl;
	Class() : Dup(1,1,CLASS), cl() { }
	int parse(uchar *, Rex*,Eenv*);
	int in(int c) { return cl.in(c); }
	void orset(Set*);
	void icase(uchar *map);
	void neg(int cflags);
	void print();
};

struct Onechar : Dup {
	uchar c;
	Onechar(int c, int lo=1, int hi=1) :
		Dup(lo,hi,ONECHAR), c(c) { }
	int parse(uchar*, Rex*, Eenv*);
	void print();
};

struct String : Rex {
	Seg seg;	
	String(Seg seg, uchar *map = 0);
	~String() { delete(seg.p); }
	Stat stat(Cenv*);
	int parse(uchar *, Rex*, Eenv*);
	void print();
protected:
	String() { };		// for Kmp and Kr only
};

struct Kmp : String {		// for string first in pattern
	Array<int> fail;
	Kmp(Seg seg, int*);		// ICASE-mapped already
	int parse(uchar*, Rex*, Eenv*);
};

/* data structure for an alternation of pure strings
   son points to a subtree of all strings with a common
   prefix ending in character c.  sib links alternate
   letters in the same position of a word.  end=1 if
   some word ends with c.  the order of strings is
   irrelevant, except long words must be investigated
   before short ones.  The first level of trie is indexed
   into buckets.
*/
struct Trie : Rex {
	enum { MASK = UCHAR_MAX, NROOT = MASK+1 };
	struct Tnode {
		uchar c;
		uchar end;
		Tnode *son;
		Tnode *sib;
		Tnode(uchar c) : c(c), end(0), son(0), sib(0) { }
		~Tnode() { delete son; delete sib; }
	};
	int min, max;		// length of entry
	Tnode *root[NROOT];	// index of trie roots
	int insert(uchar*);
	Trie() : Rex(TRIE), min(INT_MAX), max(0) { 
		memset(root, 0, sizeof(root)); }
	~Trie() { for(int i=0; i<NROOT; i++) delete root[i]; }
	Stat stat(Cenv*);
	int parse(uchar *s, Rex *contin, Eenv *env);
	void print();
private:
	int parse(Tnode*, uchar*, Rex*, Eenv*);
	void print(Tnode*, int, Array<uchar>&);
};

struct Back : Rex {
	int n;
	Back(int n) : Rex(BACK), n(n) { }
	Stat stat(Cenv*);
	int parse(uchar *, Rex*,Eenv*);
	void print();
};

struct Subexp : Rex {
	short n;		// subexpression number
	uchar used;		// nonzero if backreferenced
	Rex *rex;		// contents
	Subexp(int n, Rex *rex)
		: Rex(SUBEXP), n(n), rex(rex), used(0) { }
	~Subexp() { delete(rex); }
	int serialize(int);
	Stat stat(Cenv*);
	int parse(uchar *, Rex*,Eenv*);
	void print();
};

struct Alt: Rex {
	int n1, n2;	// same as in Rep
	Rex *left;
	Rex *right;
	int rserial;
	Alt(int n1, int n2, Rex* left, Rex *right) :
	  Rex(ALT), n1(n1), n2(n2), left(left), right(right) { }
	~Alt() { delete(left); delete(right); }
	int serialize(int);
	Stat stat(Cenv*);
	int parse(uchar *, Rex*,Eenv*);
	void print();
};

struct Conj: Rex {
	Rex *left;
	Rex *right;
	Conj(Rex *left, Rex *right) :
	   Rex(CONJ), left(left), right(right) { }
	~Conj() { delete left; delete right; }
	int serialize(int);
	Stat stat(Cenv*);
	int parse(uchar*, Rex*, Eenv*);
	void print();
};

struct Rep : Dup {
	int n1;		// subexpression number, or 0
	int n2;		// last contained subexpression number
	Rex *rex;
	Rep(int lo, int hi, int n1, int n2, Rex *rex) :
		Dup(lo,hi,REP), n1(n1), n2(n2),
		rex(rex) { }
	~Rep() { delete rex; };
	int serialize(int);
	Stat stat(Cenv*);
	int parse(uchar *, Rex*, Eenv*);
	int dorep(int, uchar *, Rex*, Eenv*);
	void print();
};

struct Neg : Rex {
	Rex *rex;
	Neg(Rex *rex) : Rex(NEG), rex(rex) { }
	~Neg() { delete rex; }
	int serialize(int);
	Stat stat(Cenv*);
	int parse(uchar*, Rex*, Eenv*);
	void print();
};

/* Only one copy of class Done is ever needed, however
   that copy is initialized dynamically, not statically.
   in order to avoid dependence on C++ library, so the
   object code can be loaded by cc */

struct Done : Rex {
	static Rex *done;	// pointer to the one copy
	int parse(uchar *, Rex*, Eenv*);
	void print() { }
};
