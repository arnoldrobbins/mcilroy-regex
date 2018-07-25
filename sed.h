#include <limits.h>
#include "regex.h"


typedef unsigned char uchar;

typedef struct {
	uchar *w;		/* write pointer */
	uchar *e;		/* end */
	uchar *s;		/* start */
} Text;

extern void compile(Text *script, Text *raw);
extern void execute(Text *script, Text *input);
extern int recomp(Text *script, Text *t, int seof);
extern int match(uchar *re, Text *data, int gflag);
extern int substitute(regex_t*, Text* data, uchar *rhs, int gf);
extern regex_t *readdr(int addr);
extern void tcopy(Text *from, Text *to);
void printscript(Text *script);
extern void warn(const char*, ...);
extern void quit(const char*, ...);
extern void vacate(Text*);
extern void synwarn(const char*);
extern void syntax(const char*);
extern int readline(Text*);
extern int ateof(void);
extern void coda(void);

#define exch(a, b, t) ((t)=(a), (a)=(b), (b)=(t))
	
	/* space management; assure room for n more chars in Text */
#define assure(/*Text*/t, /*int*/ n) 		\
	if((t)->s==0 || (t)->w>=(t)->e-n-1)	\
		grow(t, n);			\
	else
extern void grow(Text*, int);

	/* round character pointer up to integer pointer.
	   portable to the cray; simpler tricks are not */

#define intp(/*uchar**/p) (int*)(p + sizeof(int) - 1 \
			- (p+sizeof(int)-1 - (uchar*)0)%sizeof(int))

#define intptrp(/*uchar**/p) (intptr_t*)(p + sizeof(intptr_t) - 1 \
			- (p+sizeof(intptr_t)-1 - (uchar*)0)%sizeof(intptr_t))

extern int recno;
extern int nflag;
extern int qflag;
extern int sflag;
extern int bflag;
extern int options;
extern const char *stdouterr;

extern Text files;

/* SCRIPT LAYOUT

   script commands are packed thus:
   0,1,or2 address words signed + for numbers - for regexp
   if 2 addresses, then another word indicates activity
	positive: active, the record number where activated
	negative: inactive, sign or-ed with number where deactivated
   instruction word
	high byte IMASK+flags; flags are NEG and SEL
	next byte command code (a letter)
	next two bytes, length of this command, including addrs
        (length is a multiple of 4; capacity could be expanded
	by counting the length in words instead of bytes)
   after instruction word
	on s command
		offset of regexp in rebuf
		word containing flags p,w plus n (n=0 => g) 
		replacement text
		word containing file designator, if flag w
	on y command
		256-byte transliteration table
	on b and t command
		offset of label in script
*/

enum {
	BYTE = 8,

	IMASK = (int)0xC0000000,/* instruction flag */
	NEG   = 0x01000000,	/* instruction written with ! */

	LMASK = 0xffff,		/* low half word */
	AMASK = (int)(~0u>>1),	/* address mask, clear sign bit */
	INACT = ~AMASK,		/* inactive bit, the sign bit */

	DOLLAR = AMASK,		/* huge address */
	REGADR = ~AMASK,	/* context address */

	PFLAG = (int)0x80000000,/* s/../../p */
	WFLAG = 0x40000000	/* s/../../w */
};

extern int pack(int neg, int cmd, int length);
extern int *instr(uchar*);
#define code(/*int*/ inst) ((inst)>>2*BYTE & 0xff)
#define nexti(/*uchar**/ p) ((p) + (*instr(p)&LMASK))
