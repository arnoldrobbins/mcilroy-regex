#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "array.h"
#include "regex.h"


/* this grep is based on the Posix re package.
   unfortunately it has to have a nonstandard interface.
   1. fgrep does not have usual operators. REG_LITERAL
   caters for this.
   2. grep allows null expressions, hence REG_NULL.
   3. it may be possible to combine the multiple 
   patterns of grep into single patterns.  important
   special cases are handled by regcomb().
   4. anchoring by -x has to be done separately from
   compilation (remember that fgrep has no ^ or $ operator),
   hence REG_ANCH.  (An honest, but slow
   alternative: run regexec with REG_NOSUB off and nmatch=1
   and check whether the match is full length)
*/

int Eflag;	// like egrep
int Fflag;	// like fgrep
int cflag;	// count number of hits
int lflag;	// list files with hits
int qflag;	// quiet; return status but no output
int nflag;	// line numbers
int sflag;	// no messages for unopenable files
int vflag;	// reverse sense; seek nonmatches
int hflag;	// do not print file-name headers

/* the Array<> definitions allow for a quantity of patterns,
   or a length of input line that is unbounded except by
   the amount of memory available */

Array<char*> argpat;
int nargpat;
Array<char*> filepat;
int nfilepat;
Array<regex_t> re;
int nre;
Array<char> line;

int hits, anyhits;
int retval = 1;	// what to return for no hits
int options = REG_NOSUB | REG_NULL;
int nfiles;

void grepcomp();
void docomp(char *s);
int getline(FILE *input, const char *name);
void execute(FILE *input, const char *name);
void doregerror(int result, const char *name, int lineno);
void warn(const char *s, const char *t);
void error(const char *s, const char *t);

int
main(int argc, char **argv)
{
	for(;;) {
		switch(getopt(argc, argv, "AEFclqinsvxe:f:h")) {
		case 'A':
			options |= REG_AUGMENTED;
			if(REG_AUGMENTED)
				continue;
			
		case '?':
			fprintf(stderr,
			  "usage: grep -EFclqinsvxh pattern [file] ...\n"
			  "       grep -EFclqinsvxh -ef pattern-or-file ... [file] ...\n");
			exit(2);
		case 'E':
			Eflag = 1;
			options |= REG_EXTENDED;
			continue;
		case 'F':
			Fflag = 1;
			options |= REG_LITERAL;
			continue;
		case 'c':
			cflag = 1;
			continue;
		case 'l':
			lflag = 1;
			continue;
		case 'q':
			qflag = 1;
			continue;
		case 'i':
			options |= REG_ICASE;
			continue;
		case 'n':
			nflag = 1;
			continue;
		case 's':
			sflag = 1;
			continue;
		case 'v':
			vflag = 1;
			continue;
		case 'x':
			options |= REG_ANCH;
			continue;
		case 'h':
			hflag = 1;
			continue;
		case 'e':
			argpat.assure(nargpat);
			argpat[nargpat++] = optarg;
			continue;
		case 'f':
			filepat.assure(nfilepat);
			filepat[nfilepat++] = optarg;
			continue;
		case -1:
			break;
		}
		break;
	}
	if(nargpat + nfilepat == 0) {
		if(optind >= argc)
			error("no pattern", "");
		else
			argpat[nargpat++] = argv[optind++];
	}
	if(Fflag+Eflag > 1)
		error("-E and -F are incompatible", "");
	grepcomp();
	nfiles = argc - optind;
	if(nfiles <= 0)
		execute(stdin, "(standard input)");
	else for( ; optind<argc; optind++) {
		FILE *input = fopen(argv[optind], "r");
		if(input)
			execute(input, argv[optind]);
		else if(!sflag)
			error("cannot open", argv[optind]);
		else
			retval = 2;
		fclose(input);
		if(qflag && anyhits)
			break;
	}
	return anyhits? 0: retval;
}

/* the update s = t+1 flagged below is formally illegal when
   t==0, but what run-time system will catch it? */

void
grepcomp()
{
	int i;
	char *s, *t;	
	for(i=0; i<nargpat; i++) {
		for(t=s=argpat[i]; t; s = t+1) {	/*see above*/
			t = strchr(s, '\n');
			if(t)
				*t = 0;
			docomp(s);
		}	
	}

	for(i=0; i<nfilepat; i++) {
		FILE *patfile = fopen(filepat[i], "r");
		if(patfile)
			while(getline(patfile, filepat[i]) >= 0)
				docomp(&line[0]);
		else if(!sflag)
			error("cannot open", filepat[i]);
		else
			retval = 2;
		fclose(patfile);
	}
	if(nre == 0)
		error("no pattern", "");
}

void
docomp(char *s)
{
	if(re.assure(nre))
		error("out of space at--", s);
	int result = regcomp(&re[nre], s, options);
	if(result)
		doregerror(result, s, 0);
	if(!nre || !regcomb(&re[nre-1], &re[nre]))
		nre++;
}

int
getline(FILE *input, const char *name)
{
	int c, j;
	for(j=0; ; j++) {
		if(line.assure(j))
			error("out of space reading ", name);
		switch(c = getc(input)) {
		default:
			line[j] = c;
			continue;
		case EOF:
			if(j == 0)
				return -1;
			warn("newline appended to ", name);
		case '\n':
			line[j] = 0;
			return j;
		}
	}
}

void
execute(FILE *input, const char *name)
{
	int i, lineno;
	int hits = 0;
	for(lineno=1; ; lineno++) {
		int n = getline(input, name);
		if(n < 0)
			break;
		for(i=0; i<nre; i++) {
			int result = regnexec(&re[i], &line[0], n, 0, 0, 0);
			if(result == 0) 
				break;
			if(result != REG_NOMATCH)
				doregerror(result, name, lineno);
		}
		if((i<nre) ^ vflag) {
			hits++;
			if(qflag | lflag)
				break;
			if(cflag)
				continue;
			if(nfiles>1 && !hflag)
				printf("%s:", name);
			if(nflag)
				printf("%d:", lineno);
			printf("%s\n", line.bytes());
		}
	}
	if(hits)
		anyhits = 1;
	if(qflag)
		return;
	if(lflag && hits)
		printf("%s\n", name);
	if(!lflag && cflag) {
		if(nfiles>1 && !hflag)
			printf("%s:", name);
		printf("%d\n", hits);
	}
}

void
doregerror(int result, const char *name, int lineno)
{
	char errbuf[100];
	if(result==0 || result==REG_NOMATCH)
		return;
	regerror(result, 0, errbuf, sizeof(errbuf));
	fprintf(stderr, "grep: %s: %s", errbuf, name);
	if(lineno)
		fprintf(stderr, ":%d\n", lineno);
	else
		fprintf(stderr, "\n");
	exit(2);
}

void
warn(const char *s, const char *t)
{
	fprintf(stderr, "grep: %s %s\n", s, t);
}

void
error(const char *s, const char *t)
{
	warn(s, t);
	exit(2);
}

