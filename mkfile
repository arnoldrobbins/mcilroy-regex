BUILTINS =
CFLAGS = -I.
CCFLAGS = $CFLAGS -B
CC = cc

all:V:	re1.o re2.o grep sed

retest:	testre testre.dat
	testre <testre.dat

sedtest: sed testsed.sh
	PATH=.:$PATH sh testsed.sh

greptest: grep testgrep.sh
	PATH=.:$PATH sh testgrep.sh

re%.o:	regex.h re.h array.h array.c re%.c
	CC $CCFLAGS -O -c re$stem.c

# Dre?.o are versions of re?.o augmented by regex print functions,
# tracing, and malloc accounting (to look for storage leaks)

Dre%.o: regex.h re.h array.h array.c Dre%.c
	CC $CCFLAGS -g -DDEBUG -c Dre$stem.c

Dre%.c: re%.c
	rm -f D$prereq
	ln $prereq D$prereq

# testre is a black-box script-driven testing harness.

testre:	testre.o Dre1.o Dre2.o Ddummy
	CC $CCFLAGS testre.o Dre[12].o -o testre

testre.o: regex.h testre.c
	$CC $CFLAGS -g -DDEBUG -c testre.c

sed:	sed0.o sed1.o sed2.o sed3.o re1.o re2.o dummy
	$CC $CFLAGS sed[0123].o re1.o re2.o -o sed

sed%.o:	regex.h sed.h sed%.c
	$CC $CFLAGS -O -c sed$stem.c

grep: grep.o re1.o re2.o dummy
	CC $CCFLAGS grep.o re[12].o -o grep

grep.o: regex.h re.h array.h array.c grep.c
	CC $CCFLAGS -O -c grep.c

# re is a test and tracing harness for the regex.h functions.
# usage is described in re0.c. 

re:	Dre1.o Dre2.o Dre0.o Ddummy
	CC $CCFLAGS -g $CCFLAGS Dre[012].o -o re

# making dummy forces all template instantiations needed in
# re1.o and re2.o to be instantiated there and not elsewhere

%dummy:	%re1.o %re2.o
	CC $CCFLAGS ${stem}re[12].o dummy.c -o ${stem}dummy

bundle:	regex.h sed.h sed0.c sed1.c sed2.c sed3.c re.h \
	array.h array.c re1.c re2.c re0.c testre.c testre.dat \
	dummy.c testsed.sh testgrep.sh grep.c mkfile README
	bundle $prereq >bundle



re_to_mona: mona.tab.c lex.yy.c
	cc mona.tab.c -ll -ly -o re_to_mona

lex.yy.c: mona.lex
	lex mona.lex

mona.tab.c: mona.rie
	rie mona.rie
	sed '/#line/d' mona.tab.c >junk
	mv junk mona.tab.c

clean:
	rm -f *.o *.ii sed grep testre re Dre?.c *dummy
	rm -f btestre prof.out Dre?.int.o
	rm -f mona.tab.c lex.yy.c re_to_mona
	rm -f a.out core			# just in case
	rm -f in out expect pat			# testgrep.sh
	rm -f SCRIPT INPUT OUTPUT* RESULT NOWHERE DIAG # testsed.sh


btestre: bre1.o bre2.o testre.o
	rm prof.out
	lcc -b -g bre[12].o testre.o -o btestre

bre(.)\.o:R: Ddummy
	sed 's/^Dre/bre/' Dre$stem1.ii >bre$stem1.c
	CC $CCFLAGS -c -DDEBUG -S bre$stem1.c
	mv bre$stem1.int.c bre$stem1.c
	lcc -b -N -I. -I/usr/include -c bre$stem1.c

bretest: btestre
	btestre <testre.dat
	bprint | sed -f bprint.sed \
		     -e '/\.c:$/p' \
		     -e '/bre.\.c:$/,/\.c:$/d'
