#ifndef REGEX_H
#define REGEX_H
#include <stddef.h>		/* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef long regoff_t;

typedef struct {
	regoff_t rm_so;		/* offset of start */
	regoff_t rm_eo;		/* offset of end */
} regmatch_t;

typedef struct {
	size_t re_nsub;		/* number of subexpressions */
			/* local fields, not specified by posix */
	struct Rex *rex;	/* compiled expression */
	int flags;		/* flags from regcomp() */
	unsigned char *map;	/* for REG_ICASE folding */
	int unused1;
} regex_t;

int regcomp(regex_t*, const char*, int);
int regexec(const regex_t*, const char*, size_t, regmatch_t*, int);
size_t regerror(int, const regex_t*, char*, size_t);
void regfree(regex_t*);

	/* functions needed by grep (nonstandard) */

int regcomb(regex_t*, regex_t*);
int regnexec(const regex_t*, const char*, size_t, size_t, regmatch_t*, int);

			/* regcomp flags */
#define REG_EXTENDED 	0x0001
#define REG_ICASE 	0x0002
#define REG_NOSUB 	0x0004
#define REG_NEWLINE 	0x0008
			/* regexec flags */
#define REG_NOTBOL 	0x0010
#define REG_NOTEOL 	0x0020
			/* nonstandard flags */
#define REG_NULL 	0x0040	/* allow null patterns for grep */
#define REG_ANCH 	0x0080	/* grep option -x (no Kmp) */
#define REG_LITERAL 	0x0100	/* grep option -F (no operators) */
#define REG_AUGMENTED	0x0200	/* allow & and ! operators */
#define REG_WHICH	0x0400	/* enable \: progress marks */

enum {			/* regex error codes */
	REG_NOMATCH = -1,
	REG_BADPAT = -2,
	REG_ECOLLATE = -3,
	REG_ECTYPE = -4,
	REG_EESCAPE = -5,
	REG_ESUBREG = -6,
	REG_EBRACK = -7,
	REG_EPAREN = -8,
	REG_EBRACE = -9,
	REG_BADBR = -10,
	REG_ERANGE = -11,
	REG_ESPACE = -12,
	REG_BADRPT = -13
};

#ifdef __cplusplus
}
#endif

#endif
