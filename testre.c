/*
 * regex tester
 *
 * testre [-n] [-tN] [-v] < testre.dat
 *
 *	-n	repeat each test with REG_NOSUB
 *	-tN	time limit, N sec per test (default=10, no limit=0)
 *	-v	list each test line
 *
 * see comments in testre.dat for description of format
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <regex.h>
#include <setjmp.h>
#include <signal.h>

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

#ifdef DEBUG		/* tied to MDM's regex package */
#define MSTAT 1
extern int mallocblocks;	/* to look for memory leaks */
extern int mallocbytes;
extern int freeblocks;
#else
#define MSTAT 0
int mallocblocks;		/* to keep the compiler happy */
int mallocbytes;
int freeblocks;
#endif

#define elementsof(x)	(sizeof(x)/sizeof(x[0]))
#define streq(a,b)	!strcmp(a,b)

#define NOTEST ~0

struct codes {
	int code;
	char *name;
} codes[] = {
	REG_NOMATCH,	"NOMATCH",
	REG_BADPAT,	"BADPAT",
	REG_ECOLLATE,	"ECOLLATE",
	REG_ECTYPE,	"ECTYPE",
	REG_EESCAPE,	"EESCAPE",
	REG_ESUBREG,	"ESUBREG",
	REG_EBRACK,	"EBRACK",
	REG_EPAREN,	"EPAREN",
	REG_EBRACE,	"EBRACE",
	REG_BADBR,	"BADBR",
	REG_ERANGE,	"ERANGE",
	REG_ESPACE,	"ESPACE",
	REG_BADRPT,	"BADRPT"
};

int errors;
int lineno;
char *which;
int prog;
int nflag;
int verbose;
int timelim = 10;
char *nosubmsg = "";
regmatch_t NOMATCH = {-2, -2};

static char*
null(char* s)
{
	return s ? (*s ? s : "NULL") : "NULL";
}

static void
report(char *comment, char *re, char *s)
{
	errors++;
	printf("%d:%s versus %s %s %s %s",
		lineno, null(re), null(s), which, nosubmsg, comment);
}

static void
bad(char *comment, char *re, char *s)
{
	nosubmsg = "";
	report(comment, re, s);
	printf(", test run abandoned\n");
	exit(1);
}

static void
doregerror(int code, regex_t *preg)
{
	char buf[200];
	char *msg = buf;

	switch(code) {
	case -SIGBUS:
		msg = "bus error";
		break;
	case -SIGSEGV:
		msg = "memory fault";
		break;
	case -SIGALRM:
		msg = "did not terminate";
		break;
	default:
		regerror(code, preg, msg, sizeof buf);
		break;
	}
	printf("%s\n", msg);
}

static int
readfield(char *f, char end)
{
	int c;
	for(;;) {
		*f = 0;
		c = getc(stdin);
		if(c == EOF)
			return 1;
		if(c == end)
			break;
		if(c == '\n')
			return 1;
		*f++ = c;
	} 
	if(c == '\t') {
		while(c == end)
			c = getc(stdin);
		ungetc(c, stdin);
	}
	return 0;
}

static int
hex(int c)
{
	return	isdigit(c)? c-'0':
		isupper(c)? c-'A'+10:
		c-'a'+10;
}

void escape(char *s)
{
	char *t;
	for(t=s; *t=*s; s++, t++) {
		if(*s != '\\')
			continue;
		switch(*++s) {
		case 0:
			*++t = 0;
			break;
		case 'n':
			*t = '\n';
			break;
		case 'x':
			if(!isxdigit(s[1]) || !isxdigit(s[2]))
				bad("bad \\x\n", 0, 0);
			*t = hex(*++s) << 4;
			*t |= hex(*++s);
			break;
		default:
			s--;
		}
	}
}

static int
getprog(char *ans)
{
	char *s = ans + strlen(ans);
	while(isdigit(s[-1]))
		s--;
	if(*s == 0)
		prog = -1;
	else
		prog = atoi(s);
}

static int
readline(char *spec, char *re, char *s, char *ans)
{
	int c = getchar();
	switch(c) {
	case EOF:
		return 0;
	case '#':
		while(c != '\n')
			c = getchar();
	case '\n':
		*spec = 0;
		return 1;
	}
	ungetc(c, stdin);
	if(readfield(spec, '\t')) return 0;
	if(readfield(re, '\t')) return 0;
	if(readfield(s, '\t')) return 0;
	if(readfield(ans, '\n')) return 0;
	escape(re);
	escape(s);
	getprog(ans);
	return 1;
}

static void
matchprint(regmatch_t *match, int nmatch, int m)
{
	int i;
	for( ; nmatch>m; nmatch--)
		if(match[nmatch-1].rm_so != -1)
			break;
	for(i=0; i<nmatch; i++) {
		printf("(");
		if(match[i].rm_so == -1)
			printf("?");
		else
			printf("%d", match[i].rm_so);
		printf(",");
		if(match[i].rm_eo == -1)
			printf("?");
		else
			printf("%d", match[i].rm_eo);
		printf(")");
	}
	printf("\n");
	fflush(stdout);
}

static void
matchcheck(int nmatch, regmatch_t *match, char *ans, char *re, char *s)
{
	char *p;
	int i, m, n;
	int fail = 0;
	for(i=0, p=ans; i<nmatch && *p; i++) {
		if(*p++ != '(')
			bad("improper answer\n", re, s);
		if(*p == '?') {
			m = -1;
			p++;
		} else
			m = strtol(p, &p, 10);
		if(*p++ != ',')
			bad("improper answer\n", re, s);
		if(*p == '?') {
			n = -1;
			p++;
		} else
			n = strtol(p, &p, 10);
		if(*p++ != ')')
			bad("improper answer\n", re, s);
		if(m!=match[i].rm_so || n!=match[i].rm_eo)
			fail++;
	}
	m = i;
	if(fail) {
		report("match was: ", re, s);
		matchprint(match, nmatch, m);
		return;
	}
	for( ; i<nmatch; i++) {
		if(match[i].rm_so!=-1 || match[i].rm_eo!=-1) {
			report("match was: ", re, s);
			matchprint(match, nmatch, m);
			return;
		}
	}
	if(match[nmatch].rm_so	!= NOMATCH.rm_so) {
		report("extra assignments to match array: ", re, s);
		matchprint(match, nmatch+1, m);
	}
}

int codeval(char *s)
{
	int i;
	for(i=0; i<elementsof(codes); i++)
		if(streq(s, codes[i].name))
			return codes[i].code;
	return -1;
}

jmp_buf jbuf;
void gotcha(int sig)
{
	alarm(0);
	signal(sig, gotcha);
	longjmp(jbuf, sig);
}

int alarmcomp(regex_t *preg, const char *re, int cflags)
{
	int sig, ret;
	sig = setjmp(jbuf);
	if(sig == 0) {
		alarm(timelim);
		ret = regcomp(preg, re, cflags);
		alarm(0);
	} else
		ret = -sig;
	return ret;
}

int alarmexec(const regex_t *preg, const char *s, size_t nmatch, regmatch_t *match, int eflags)
{
	int sig, ret;
	sig = setjmp(jbuf);
	if(sig == 0) {
		alarm(timelim);
		ret = regexec(preg, s, nmatch, match, eflags);				alarm(0);
	} else
		ret = -sig;
	return ret;
}

#define nonstd(flag) (flag? flag: NOTEST)

main(int argc, char **argv)
{
	int testno = 0;
	int flags, cflags, eflags, are, bre, ere, lre;
	char spec[10];
	char re[1000];
	char s[100000];
	char ans[500];
	char msg[500];
	regmatch_t match[100];
	regex_t preg;
	char *p;
	int nmatch;
	int cret, eret;
	int i, len;
	
	printf("TEST	<regex>");
	while((p = *++argv) && *p == '-')
		for(;;)
		{
			switch(*++p)
			{
			case 0:
				break;
			case 'n':
				nflag = 1;
				printf(", NOSUB");
				continue;
			case 't':
				if(*++p == 0)
					p = "0";
				timelim = atof(p);
				break;	
			case 'v':
				verbose = 1;
				printf(", verbose");
				continue;
			default:
				printf(", invalid option %c", *p);
				continue;
			}
			break;
		}
	if(p)
		printf(", argument(s) ignored");
	printf("\n");
	signal(SIGALRM, gotcha);
	signal(SIGBUS, gotcha);
	signal(SIGSEGV, gotcha);
	while(readline(spec, re, s, ans)) {
		lineno++;
		if(*spec == 0)
			continue;

	/* interpret: */

		cflags = eflags = are = bre = ere = lre = 0;
		nmatch = 20;
		for(p=spec; *p; p++) {
			if(isdigit(*p)) {
				nmatch = strtol(p, &p, 10);
				p--;
				continue;
			}
			switch(*p) {
			case 'A':
				are = REG_AUGMENTED;
				continue;
			case 'B':
				bre = 1;
				continue;
			case 'E':
				ere = 1;
				continue;
			case 'L':
				lre = REG_LITERAL;
				continue;
			case 'N':
				cflags |= nonstd(REG_NOSUB);
				continue;
			case 'I':
				cflags |= nonstd(REG_ICASE);
				continue;
			case 'W':
				cflags |= nonstd(REG_NEWLINE);
				continue;
			case 'U':
				cflags |= nonstd(REG_NULL);
				continue;
			case 'C':
				cflags |= nonstd(REG_ANCH);
				continue;
			case 'b':
				eflags |= nonstd(REG_NOTBOL);
				continue;
			case 'e':
				eflags |= nonstd(REG_NOTEOL);
				continue;
			default:
				bad("bad spec\n", re, s);
			}
		}
		if(streq(re, "NULL"))
			re[0] = 0;
		if((cflags|eflags) == NOTEST)
			continue;

	compile:
		fflush(stdout);
		if(bre) {
			which = "BRE";
			bre = 0;
			flags = cflags;
		} else if(ere) {
			which = "ERE";
			ere = 0;
			flags = cflags | REG_EXTENDED;
		} else if(are) {
			which = "ARE";
			are = 0;
			flags= cflags | REG_AUGMENTED;
		} else if (lre) {
			which = "LRE";
			lre = 0;
			flags = cflags | REG_LITERAL;
		} else
			continue;

	nosub:
		nosubmsg = (flags^cflags)&REG_NOSUB? " (NOSUB)": "";
		testno++;
		cret = alarmcomp(&preg, re, flags);
		if(cret == 0) {
			if(!streq(ans, "NULL") &&
			   !streq(ans,"NOMATCH") &&
			   ans[0]!='(') {
				report("regcomp should fail and didn't", re, ans);
				printf("\n");
				continue;
			}
		} else if(streq(ans,"NULL") || ans[0]=='(') {
			report("regcomp failed: ", re, ans);
			doregerror(cret, &preg);
			goto next;
		} else if(cret==REG_BADPAT || cret==codeval(ans))
			goto next;
		else if(streq(ans, "BADPAT"))
			goto next;
		else {
			report("regcomp failed with unexpected answer: ", re, ans);
			errors--;
			doregerror(cret, &preg);
			goto next;
		}

	/* execute: */
		
		for(i=0; i<elementsof(match); i++)
			match[i] = NOMATCH;
		if(streq(s, "NULL"))
			s[0] = 0;
		eret = alarmexec(&preg, s, nmatch, match, eflags);

		if(prog >= 0) {
			if(eret == prog)
				eret = 0;
			else
				report("wrong progress mark:", re, s);
		}	
			
		if(eret != 0) {
			if(!streq(ans, "NOMATCH")) {
				report("regexec failed: ", re, s);
				doregerror(eret, &preg);
			}
		} else if(streq(ans,"NOMATCH")) {
			report("regexec should fail and didn't: " ,re, s);
			matchprint(match, nmatch, 0);
		} else if(streq(ans,"NULL") || flags&REG_NOSUB)
			matchcheck(0, match, ans, re, s);
		else
			matchcheck(nmatch, match, ans, re, s);
		regfree(&preg);

	next:
		if(nflag && (flags&REG_NOSUB)==0) {
			flags |= REG_NOSUB;
			goto nosub;
		}
		goto compile;
	}
	printf("%d lines, ", lineno);
	printf("%d tests, %d errors\n", testno, errors);
	if(MSTAT) {
		printf("%d blocks allocated", mallocblocks);
		printf(", %d bytes allocated", mallocbytes);
		printf(", %d blocks lost\n", mallocblocks-freeblocks);
	}
	return 0;
}
