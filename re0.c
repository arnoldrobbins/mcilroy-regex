#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#ifdef DEBUG
#include "re.h"
#else
#include "regex.h"
#endif
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
extern "C" void abort();

/* regular expression tester
   usage: re [options] [pattern]
   options:
	-a	ARE, augmented regular expression
	-b	BRE, basic regular expression (default)
	-e	ERE, extended regular expression
	-a	ARE, augmented regular expression
	-i	ignore case
	-tc	trace the compilation
	-te	trace the execution
   if a pattern is present, use like grep
   if not, enter patterns and strings as alternate lines
	it echoes compiled patterns in a bastard notation
	and prints the match array in the form
	(m0,n0)(m1,n1)(m2,n2)... with trailing (-1,-1) entries
	summarized as k*(?,?) 
	an empty pattern is taken as a repeat of previous */

#define UNTOUCHED -2

void matchprint(regmatch_t *m, char *s, int n)
{
	int i;
	regoff_t so, eo;
	char *p;
	if(m == 0)
		printf("No match\n");
	else {
		while(--n >= 0)
			if(m[n].rm_so != UNTOUCHED)
				break;
		int k = 0;
		while(n>=0 && m[n].rm_so==-1) {
			n--;
			k++;
		}
		for(i=0; i<=n; i++) {
			so = m[i].rm_so;
			if(so < 0) 
				printf("(?,?)");
			else {
				eo = m[i].rm_eo;
				printf("(%d,%d) ", so, eo);
				for(p=s+so; p<s+eo; p++)
					printf("%c",*p);
			}
			printf("\n");
		}
		if(k)
			printf("%d*(?,?)\n",k);
		else if(n < 0)
			printf("nosub\n");
	}
}

#ifdef DEBUG
extern	int edebug, cdebug;
#else
static	int edebug, cdebug;
#endif


void main(int argc, char **argv)
{
	const int NMATCH = 20;
	char s[10000];
	regmatch_t pmatch[NMATCH];
	regex_t preg;
	int i;
	char *grep = 0;
	int cflags = 0;
	for(i=1; i<argc; i++) {
		if(strcmp(argv[i],"-e") == 0)
			cflags |= REG_EXTENDED;
		else if(strcmp(argv[i],"-a") == 0)
			cflags |= REG_AUGMENTED|REG_EXTENDED;
		else if(strcmp(argv[i],"-N") == 0)
			cflags |= REG_NOSUB;
		else if(strcmp(argv[i],"-i") == 0)
			cflags |= REG_ICASE;
		else if(strcmp(argv[i],"-b") == 0)
			;
		else if(strcmp(argv[i],"-tc") == 0)
			cdebug = -1;
		else if(strcmp(argv[i],"-te") == 0)
			edebug = -1;
		else if(strncmp(argv[i],"-f",2) == 0) {
			int fd = open(argv[i]+2,0);
			if(fd == -1)
				abort();
			grep = (char*)mmap(0,1<<20,PROT_READ,MAP_SHARED,fd,0);
			if(grep == (char*)-1)
				perror("mmap");
		} else
			grep = argv[i];
	}
	preg.rex = 0;
	if(grep) {
		if(regcomp(&preg, grep, cflags|REG_NOSUB))
			printf("invalid expr\n");
		else while(fgets(s,sizeof s,stdin)) {
			*strchr(s, '\n') = 0;
			if(regexec(&preg, s, preg.re_nsub+1, pmatch, 0) == 0)
				printf("%s\n",s);
		}
	}
	else while(fgets(s,sizeof s,stdin)) {
		*strchr(s, '\n') = 0;
		if(s[0] == 0) {
			if(preg.rex == 0)
				continue;
		} else {
			regfree(&preg);
			if(regcomp(&preg, s, cflags)) {
				printf("invalid expr\n");
				continue;
			}
		}
#ifdef DEBUG
		flagprint(&preg);
		preg.rex->print();
#endif
		printf("\n");
		if(fgets(s,sizeof s,stdin) == 0)
			break;
		*strchr(s, '\n') = 0;
		for(i=0; i<NMATCH; i++)
			pmatch[i].rm_so = UNTOUCHED;
		i = regexec(&preg, s, NMATCH, pmatch, 0);
		if(i != 0)
			printf("no match\n");
		matchprint(pmatch, s, NMATCH);
	}
}
