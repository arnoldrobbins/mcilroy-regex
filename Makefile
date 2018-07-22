CFLAGS = -I.
CCFLAGS = $(CFLAGS) -g -DDEBUG
CC = g++

all:	re1.o re2.o grep sed

retest:	testre testre.dat
	./testre <testre.dat

sedtest: sed testsed.sh
	sh ./testsed.sh

greptest: grep testgrep.sh
	sh ./testgrep.sh

re1.o:	regex.h re.h array.h re1.c
	$(CC) $(CCFLAGS) -O -c re1.c

re2.o:	regex.h re.h array.h re2.c
	$(CC) $(CCFLAGS) -O -c re2.c

re0.o:	regex.h re.h re0.c
	$(CC) $(CCFLAGS) -O -c re0.c

# Dre?.o are versions of re?.o augmented by regex print functions,
# and tracing.

Dre1.o: regex.h re.h array.h re1.c
	$(CC) $(CCFLAGS) -g -DDEBUG -c -o Dre1.o re1.c

Dre2.o: regex.h re.h array.h re2.c
	$(CC) $(CCFLAGS) -g -DDEBUG -c -o Dre2.o re2.c

# testre is a black-box script-driven testing harness.

#testre:	testre.o Dre1.o Dre2.o Ddummy
#	$(CC) $(CCFLAGS) -o testre testre.o Dre[12].o
testre:	testre.o re1.o re2.o
	$(CC) $(CCFLAGS) -o testre testre.o re[12].o

testre.o: regex.h testre.c
	$(CC) $(CFLAGS) -g -DDEBUG -c testre.c

#sed:	sed0.o sed1.o sed2.o sed3.o re1.o re2.o dummy
#	$(CC) $(CFLAGS) sed[0123].o re1.o re2.o -o sed
sed:	sed0.o sed1.o sed2.o sed3.o re1.o re2.o
	$(CC) $(CFLAGS) $(CCFLAGS) sed[0123].o re1.o re2.o -o sed

sed1.o:	regex.h sed.h sed1.c
	$(CC) $(CFLAGS) $(CCFLAGS) -O -c sed1.c

sed2.o:	regex.h sed.h sed2.c
	$(CC) $(CFLAGS) $(CCFLAGS) -O -c sed2.c

sed3.o:	sed.h sed3.c
	$(CC) $(CFLAGS) $(CCFLAGS) -O -c sed3.c

#grep: grep.o re1.o re2.o dummy
#	$(CC) $(CCFLAGS) -o grep grep.o re[12].o
grep: grep.o re1.o re2.o
	$(CC) $(CCFLAGS) -o grep grep.o re[12].o


grep.o: regex.h re.h array.h grep.c
	$(CC) $(CCFLAGS) -O -c grep.c

# re is a test and tracing harness for the regex.h functions.
# usage is described in re0.c. 

#re:	Dre1.o Dre2.o Dre0.o Ddummy
#	$(CC) $(CCFLAGS) -g $(CCFLAGS) Dre[012].o -o re
re:	re1.o re2.o re0.o
	$(CC) $(CCFLAGS) -g $(CCFLAGS) re[012].o -o re


# making dummy forces all template instantiations needed in
# re1.o and re2.o to be instantiated there and not elsewhere

dummy:	re1.o re2.o
	$(CC) $(CCFLAGS) re[12].o dummy.c -o dummy

Ddummy:	re1.o re2.o
	$(CC) $(CCFLAGS) Dre[12].o dummy.c -o Ddummy

bundle:	regex.h sed.h sed0.c sed1.c sed2.c sed3.c re.h \
	array.h re1.c re2.c re0.c testre.c testre.dat \
	dummy.c testsed.sh testgrep.sh grep.c makefile README
	bundle regex.h sed.h sed0.c sed1.c sed2.c sed3.c re.h \
		array.h re1.c re2.c re0.c testre.c testre.dat \
		dummy.c testsed.sh testgrep.sh grep.c makefile README \
		>bundle

clean:
	rm -f *.o *.ii sed grep testre re Dre?.c *dummy
	rm -f btestre prof.out Dre?.int.o
	rm -f a.out core			# just in case
	rm -f in out expect pat			# testgrep.sh
	rm -f SCRIPT INPUT OUTPUT* RESULT NOWHERE DIAG # testsed.sh
