#include <stdlib.h>

/*
	Slow and certain Posix RE recognizer.
*/

/*
	stack of endpoints for adjudicating best match */

char line[100];
char *last;

enum { NMATCH = 20 };
struct Match {
	char *s;	// start pointer
	char *e;	// end pointer
};
Match match[NMATCH];
Match bestmatch[NMATCH];

struct Save {
	Match space[NMATCH];
	Save(int nsub = NMATCH);
	restore() { memmove(match, space, sizeof match); }
};

Save::Save(int nsub)
{
	memmove(space, match, sizeof match);
	memset(&match[nsub],0,(NMATCH-nsub)*sizeof(Match));
}

struct End {
	enum { SIZE = 100 };
	int n;
	struct {
		int serial;
		char *s;
	} pos[SIZE];
} end, best;

inline void push() { end.n++; }
inline void pop() { end.:n--; }
void push(int serial, int s)
{
	end.pos[end.n].serial = serial;
	end.pos[end.n].s = s;
	push();
	if(end.n >= End::SIZE)
		abort();
};

struct Rex {
	int serial;
	virtual void print() { }
	virtual void parse(char *s, Rex *cont);
};

/* Continuation routines to get control at the end of
   a component.
   Coda: simple coda
   ACoda: Alt coda backpatch position into stack
   PCoda: push position onto stack
   SCoda: push; also record regmatch_t.eo
*/
Struct Coda : Rex {
	Rex *rex;	// the successor
	Rex *cont;	// and its continuation
	Coda(Rex *rex, Rex *cont) : rex(rex), cont(cont) { }
	void parse(char *s, Rex *cont);
};

struct Pcoda : Coda {
	int serial;
	Pcoda(int serial, Rex *rex, Rex *cont)
	     : serial(serial), Coda(rex, cont) { }
	void parse(char *s, Rex *cont);
};

struct ACoda : Coda {
	int n;		// original value of end.n
	ACoda(Rex *rex, Rex *cont, int n)
	     : Coda(rex, cont), n(n) { }
	void parse(char *s, Rex *cont);
};

struct SCoda : PCoda {
	int nsub;
	SCoda(int serial, int nsub, Rex *rex, Rex *cont) 
	     : nsub(nsub), PCoda(serial, rex, cont) { }
	void parse(char *s, Rex *cont);
};

struct Done : Rex {
	void parse(char *s, Rex *cont);
}	

struct Char : Rex {
	char c;
	Char(int c) c(c) { }
	void print() { printf("<%d:%c>", serial, c); }
	void parse(char *s, Rex *cont);
};

struct Dot : Rex {
	void print(); { printf(".<%d>", serial); }
	void parse(char *s, Rex *cont);
};

struct Seq : Rex {
	Rex *left;
	Rex *right;
	Seq(Rex *left, Rex *right) : left(left), right(right) { }
	void print(); { left->print(); right->print(); }
	void parse(char *s, Rex *cont);
};

struct Alt : Rex {
	Rex *left;
	Rex *right;
	Alt(Rex *left, Rex *right) : left(left), right(right) { }
	void print() {
	void parse(char *s, Rex *cont);
};

void Alt::print()
{
	printf("<%d:", serial);
	left->print();
	printf("|");
	right->print();
	printf(">");
}

struct Sub : Rex {
	int nsub;
	Rex *rex;
	Sub(int nsub, Rex *rex) : nsub(nsub), rex(rex) { }
	void print();
	void parse(char *s, Rex *cont);
};

void Sub::print()
{
	printf("(%d:", serial);
	rex->print();
	printf(")");
}

struct Back : Rex {
	int nsub;
	void print() { printf("<%d:\\%d>", serial, nsub); }
	void parse(char *s, Rex *cont);
};

struct Rep : Rex {
	Rex *rex;
	void print();
	void parse(char *s, Rex *cont);
};

void Rep::print()
{
	printf("<%d:", serial);
	rex->print();
	printf(">*");
}

void Done::parse(char *s, Rex *cont)
{
	int i;
	for(i=0; i<end.n; i++)
		printf("<%d:%d>"end.pos[i].serial,end.pos[i].s-line);
	printf("\n");
}	

void Char::parse(char *s, Rex *cont)
{
	if(s>=last || *s!=c)
		return;
	push(serial, s);
	cont->parse(s+1, 0, tree);
	pop();
}	

void Dot::parse(char *s, Rex *cont)
{
	if(s>=last)
		return;
	push(serial, s);
	cont->parse(s+1, 0, tree);
	pop();
}

void Alt::parse(char *s, Rex *cont)
{
	Save save;
	ACoda coda(cont, 0, end.n);
	push(serial, 0);
	left->parse(s, &coda);
	save.restore();
	right->parse(s, &coda);
	pop();
	save.restore();
}

void Seq::parse(char *s, Rex *cont)
{
	Next next(right, cont);
	left->parse(s, &next);
}

void Sub::parse(char *s, Rex *cont)
{
	Save save;
	SCoda coda(serial, nsub, cont, 0);
	rex->parse(s, &coda);
	save.restore();
}

void Rep::parse(char *s, Rex *cont)
{
	PCoda coda(serial, rex, &coda);
	push(serial, s);
	cont->parse(s, 0);
	pop();
}

void Coda::parse(char *s, Rex*)
{
	rex->parse(s, cont);
}

void Pcoda::parse(char *s, Rex*)
{
	push(serial, s);
	Coda::parse(s, 0);
	pop();
}

void ACoda::parse(char *s, Rex*)
{
	end.pos[end.n].s = s;
	Coda::parse(s, 0);
}

void SCoda::parse(char *s, Rex*)
{
	match[nsub].e = s;
	PCoda::parse(s, 0);
}

Rex *mkAlt()
{
	Rex *alt1 = mkSeq();
	if(*s != '|')
		return alt1;
	s++;
	Rex *alt2 = mkAlt();
	return new Alt(alt1, alt2);
}

Rex *mkSeq()
{
	Rex *seq1 = mkPrim();
	if(*s == '*') {
		s++;
		seq1 = new Rep(seq1);
	}
	Rex *seq2 = MkSeq();
	if(seq2 == 0)
		return seq1;
	return new Seq(seq1, seq2);
}

Rex *mkPrim()
{
	Rex *prim;
	if(*s == 0)
		return 0;
	if(*s == '(') {
		s++:
		prim = mkAlt();
		prim = new Sub(++nsub, prim);
		if(*s++ != ')')
			abort();
	} else if(s == '.') {
		s++;
		prim = new Dot;
	} else if(isalpha(*s))
		prim = new Char(*s++);
	else
		abort();
	return prim;
