#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sed.h"

void readscript(Text*, char*);
void copyscript(Text*, uchar*);
void initinput(int, char **);
FILE *aopen(char*);

#define ustrncmp(a,b,c) (uchar*)strncmp((char*)(a), (char*)(b), c)

int recno;		/* current record number */
int nflag;		/* nonprint option */
int qflag;		/* command q executed */
int sflag;		/* substitution has occurred */
int bflag;		/* strip leading blanks from c,a,i <text> */
int options;		/* conjunction, negation */

main(int argc, char **argv)
{
	static Text script;
	static Text data;
	for(;;) {
		switch(getopt(argc, argv, "bnf:e:")) {
		case 'b':
			bflag++;
			continue;
		case 'e':
			copyscript(&data, (uchar*)optarg);
			continue;
		case 'f':
			readscript(&data, optarg);
			continue;
		case 'n':
			nflag++;
			continue;
		case '?':
			quit("usage: sed [-n] script [files]\n"
			"     sed [-n] [-f scriptfile] "
			      "[-e script] [files]");
		case -1:
			break;
		}
		break;
	}
	if(data.s == 0) {
		if(optind >= argc)
			quit("no script");
		copyscript(&data, (uchar*)argv[optind++]);
	}
	if(ustrncmp(data.s, "#n", 2) == 0)
		nflag = 1;
	copyscript(&data, (uchar*)"\n\n");  /* e.g. s/a/\ */
	compile(&script, &data);
	/* printscript(&script); /* debugging */

	initinput(argc-optind, argv+optind);
	for(;;) {
		data.w = data.s;
		if(!readline(&data))
			break;
		execute(&script, &data);
	}
	if(fclose(stdout) == EOF)
		quit(stdouterr);
	return 0;
}

void
grow(Text *t, int n)
{
	int w = t->w - t->s;
	int e = t->e - t->s + (n/BUFSIZ+1)*BUFSIZ;
	t->s = (uchar*)realloc(t->s, e);
	if(t->s == 0)
		quit("out of space");
	t->w = t->s + w;
	t->e = t->s + e;
}

/* BUG: a segment that ends with a comment whose
   last character is \ causes a diagnostic */

void
safescript(Text *t)
{
	if(t->w > t->s+1 && t->w[-2] == '\\')
		warn("script segment ends with \\");
}

void
readscript(Text *t, char *s)
{
	int n;
	FILE *f = aopen(s);
	for(;;) {
		assure(t, 4);
		n = fread(t->w, 1, t->e - t->w - 3, f);
		if(n <= 0)
			break;
		t->w += n;
	}
	fclose(f);
	if(t->w > t->s && t->w[-1] != '\n') {
		*t->w++ = '\n';
		warn("newline appended to script segment");
	}
	*t->w = 0;
	safescript(t);
}

void
copyscript(Text *t, uchar *s)
{
	do {
		assure(t, 2);
	} while(*t->w++ = *s++);
	if(--t->w > t->s && t->w[-1] != '\n') {
		*t->w++ = '\n';
		*t->w = 0;
	}
	safescript(t);
}

/* DATA INPUT */

struct {
	int iargc;		/* # of files not fully read */
	char **iargv;		/* current file */
	FILE *ifile;		/* current input file */
} input;
/* getch fetches char from current file 
   returns EOF at final end of file
   leaves iargc==0 after line $
*/
#define getch(cp) if((*(cp)=getc(input.ifile))==EOF) \
		     *(cp)=gopen(); else
int gopen(void);		/* called only by getch() */	

int
readline(Text *t)
{
	int c;
	int len = t->w - t->s;
	coda();
	if(qflag || ateof())
		return 0;
	for(;;) {
		assure(t, 2);
		getch(&c);
		if(c == '\n')
			break;
		else if(c != EOF)
			*t->w++ = c;
		else if(t->w - t->s == len)
			return 0;
		else {
			warn("newline appended");
			break;
		}
	}
	*t->w = 0;			/* for safety */
	getch(&c);			/* to identify line $ */
	if(c != EOF)
		ungetc(c, input.ifile);
	recno++;
	sflag = 0;
	return 1;
}	

int
gopen(void)
{
	int c = EOF;
	while(c==EOF && --input.iargc > 0) {
		fclose(input.ifile);
		input.ifile = aopen(*++input.iargv);
		c = getc(input.ifile);
	}
	return c;
}

int 
ateof(void)
{
	return input.iargc <= 0;
}	

void
initinput(int argc, char **argv)
{
	input.iargc = argc;
	input.iargv = argv;
	if(input.iargc == 0) {
		input.iargc = 1;	/* for ateof() */
		input.ifile = stdin;
	} else
		input.ifile = aopen(*input.iargv);
}

FILE *
aopen(char *s)
{
	FILE *f = fopen(s, "r");
	if(f == 0)
		quit("cannot open %s", s);
	return f;
}

void
warn(char *format, ...)
{
	va_list args;
	fprintf(stderr,"sed warning: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr,"\n");
}

void
quit(char *format, ...)
{
	va_list args;
	fprintf(stderr,"sed error: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr,"\n");
	exit(1);
}

/* debugging code 1; compile and execute stubs.
   simply prints the already collected script and
   prints numbered input lines

void
compile(Text *script, Text *t)
{
	uchar *s = t->s;
	assure(script, 1);
	*script->w++ = 0;
	while(*s) putchar(*s++);
}

void
execute(Text *x, Text *y)
{
	x = x;		
	printf("%d: %s", recno, y->s);
}

*/
