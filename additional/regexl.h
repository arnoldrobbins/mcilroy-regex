/* Posix BRE (basic regular expression) recognizer.
   A backtrack parser on Floyd's model.  (Backtracking is
   nearly unavoidable; BRE parsing is NP-complete, aside 
   from the minor limitation to 9 backreferences.) 

   The program realizes an outside-in understanding
   of the longest-leftmost rule, which chooses "the
   longest" recursively by length of substrings in
   an in-order walk over a parse tree (where catenation
   is right-associative). */

enum RexType {
	OK,			/* always succeed, used internally */
	ANCHOR,			/* initial ^ */
	END,			/* final $ */
	DOT,			/* . */
	STRING,			/* some chars */
	CLASS,			/* [...] */
	BACK,			/* \1, \2, etc */
	SUBEXP,			/* \(...\) */
	REP,			/*  closure */
	SEQ,			/* concatenation */
	NREXTYPE		/* dummy */
};

enum { REGEX = NREXTYPE };	/* for debugging */

typedef adt Seg;
typedef adt Rex;
typedef (int, Seg*, int) Match;	/* length, subexpressions, best */
enum { NMATCH = 9 };
enum { DONE = -1 };

/* A parsing function takes a regular expression e, a Segment s,
   an array m of subexpression matches, and a demand channel c.
   It communicates back to its caller a sequence of match lengths
   and subexpression-match lists.  A length of DONE
   denotes no more matches.  Each time a better match
   is reported, the "best" flag is 1, else 0.
   
   A demand channel (Chan) has a control channel, by which
   the caller asks for another alternative (MORE), an end
   of parsing (KILL), or to RESET the calculation of "best". */

/* the functions .make, .unmake initialize and destroy;
   in other adt's functions .new and .free do heap allocation */ 

enum Ctl { MORE, KILL, RESET };
adt Chan {
	extern chan(Ctl) ctl;
	extern chan(Match) data;
	Chan make();
	void unmake(Chan);
};

typedef void Parse(Rex *e, Seg s, Seg *m, Chan c);

Parse *parse[];

void (*rexprint[])(Rex *);

void (*rexfree[])(Rex *);

/* A segment is a string pointer and length.  A length
   of -1 for a subexpression match means no match. 

   Function prefix returns 1 if segment a is a prefix
   of segemnt b, else 0

   Function append appends segment s as backref pattern n
   to match list m1, clearing out all later backref pats,
   putting result in match list m2 
   Function clear copies m1 to m2 and clears from backref
   n onwards */

adt Seg {
	extern byte *p;
	extern int n;
	Seg make(byte *p, int n);
	int prefix(Seg a, Seg b);
	void append(Seg s, Seg *m1, int n, Seg *m2);
	void clear(int n, Seg *m1, Seg *m2);
	void next(*Seg);
	void prev(*Seg);
};

void matchprint(Seg *m, Seg s);

/* A set of ascii characters, represented as a bitstring */

adt Set {
	byte cl[256/8];
	void clear(*Set);
	void init(*Set, Seg s);
	void insert(*Set, byte c);
	int in(*Set, byte c);
	void or(*Set, Set*);
	void neg(*Set);
};

/* adt's for the various kinds of Rex.  Each has a
   recognizer .parse, a .new, a .free, and a debugging
   .print function */

adt Ok {
	Rex *new();
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt Anchor {
	extern Rex *rex;
	Rex *new(Rex *e);
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt End {
	Rex *new();
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt Dot {
	Rex *new();
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt Class {
	Set cl;
	Rex *new(Seg);
	void free(Rex*);
	Parse parse;
	int in(*Class, int c);
	void or(*Class, Set*);
	void neg(*Class);
	void print(*Rex);
};

adt String {
	extern Seg seg;
	Rex *new(Seg);
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt Back {
	int n;
	Rex *new(int);
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt Subexp {
	int n;
	extern Rex *rex;
	Rex *new(int n, Rex *);
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt Rep {
	int lo;
	int hi;
	int n;		/* largest preceding backref index */
	extern Rex *rex;
	Rex *new(int lo, int hi, int n, Rex*);
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};

adt Seq {
	extern Rex *rex;
	extern Rex *seq;
	Rex *new(Rex *rex, Rex *seq);
	void free(Rex*);
	Parse parse;
	void print(*Rex);
};	
enum Hard { EASY, HARD };
aggr Stat {	/* used only in Rex.hard1() */
	int n;	/* length of regex, if no closure or backref */
	int s;	/* number of simple closures */
	int c;	/* number of closures */
	int b;	/* number of backrefs */
};
adt Rex {
	extern RexType type;
	union {
		Ok;
		Anchor;
		End;
		Dot;
		String;
		Class;
		Back;
		Subexp;
		Rep;
		Seq;
	};
	Rex *new(*Rex, RexType type);	/* called from new()s */
	Hard hard(*Rex);
	intern Stat hard1(*Rex);
	void free(*Rex);
	void print(*Rex);
};

/* parse a possibly anchored regular expression */
	
Seg *regex(Rex *rex, Seg s);
Rex *regcomp(Seg s);

