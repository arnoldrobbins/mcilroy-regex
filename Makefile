CFLAGS = -I.
OPTFLAGS ?= -O2
CXXFLAGS = $(CFLAGS) $(OPTFLAGS) -std=c++11 -g -DDEBUG
CXX ?= g++

all:	re1.o re2.o grep sed

retest:	testre testre.dat
	./testre <testre.dat

sedtest: sed testsed.sh
	PATH=.:$$PATH sh ./testsed.sh

greptest: grep testgrep.sh
	PATH=.:$$PATH sh ./testgrep.sh

re1.o:	regex.h re.h array.h re1.cpp
	$(CXX) $(CXXFLAGS) -c re1.cpp

re2.o:	regex.h re.h array.h re2.cpp
	$(CXX) $(CXXFLAGS) -c re2.cpp

re0.o:	regex.h re.h re0.cpp
	$(CXX) $(CXXFLAGS) -c re0.cpp

# Dre?.o are versions of re?.o augmented by regex print functions,
# and tracing.

Dre1.o: regex.h re.h array.h re1.cpp
	$(CXX) $(CXXFLAGS) -g -DDEBUG -c -o Dre1.o re1.cpp

Dre2.o: regex.h re.h array.h re2.cpp
	$(CXX) $(CXXFLAGS) -g -DDEBUG -c -o Dre2.o re2.cpp

# testre is a black-box script-driven testing harness.

#testre:	testre.o Dre1.o Dre2.o Ddummy
#	$(CXX) $(CXXFLAGS) -o testre testre.o Dre[12].o
testre:	testre.o re1.o re2.o
	$(CXX) $(CXXFLAGS) -o testre testre.o re[12].o

testre.o: regex.h testre.cpp
	$(CXX) $(CXXFLAGS) -g -DDEBUG -c testre.cpp

#sed:	sed0.o sed1.o sed2.o sed3.o re1.o re2.o dummy
#	$(CXX) $(CFLAGS) sed[0123].o re1.o re2.o -o sed
sed:	sed0.o sed1.o sed2.o sed3.o re1.o re2.o
	$(CXX) $(CXXFLAGS) sed[0123].o re1.o re2.o -o sed

sed0.o:	regex.h sed.h sed1.cpp
	$(CXX) $(CXXFLAGS) -c sed0.cpp

sed1.o:	regex.h sed.h sed1.cpp
	$(CXX) $(CXXFLAGS) -c sed1.cpp

sed2.o:	regex.h sed.h sed2.cpp
	$(CXX) $(CXXFLAGS) -c sed2.cpp

sed3.o:	sed.h sed3.cpp
	$(CXX) $(CXXFLAGS) -c sed3.cpp

#grep: grep.o re1.o re2.o dummy
#	$(CXX) $(CXXFLAGS) -o grep grep.o re[12].o
grep: grep.o re1.o re2.o
	$(CXX) $(CXXFLAGS) -o grep grep.o re[12].o


grep.o: regex.h re.h array.h grep.cpp
	$(CXX) $(CXXFLAGS) -c grep.cpp

# re is a test and tracing harness for the regex.h functions.
# usage is described in re0.cpp. 

#re:	Dre1.o Dre2.o Dre0.o Ddummy
#	$(CXX) $(CXXFLAGS) -g $(CCFLAGS) Dre[012].o -o re
re:	re1.o re2.o re0.o
	$(CXX) $(CXXFLAGS) -g $(CCFLAGS) re[012].o -o re


# making dummy forces all template instantiations needed in
# re1.o and re2.o to be instantiated there and not elsewhere

dummy:	re1.o re2.o
	$(CXX) $(CXXFLAGS) re[12].o dummy.cpp -o dummy

Ddummy:	re1.o re2.o
	$(CXX) $(CXXFLAGS) Dre[12].o dummy.cpp -o Ddummy

bundle:	regex.h sed.h sed0.cpp sed1.cpp sed2.cpp sed3.cpp re.h \
	array.h re1.cpp re2.cpp re0.cpp testre.cpp testre.dat \
	dummy.cpp testsed.sh testgrep.sh grep.cpp makefile README
	bundle regex.h sed.h sed0.cpp sed1.cpp sed2.cpp sed3.cpp re.h \
		array.h re1.cpp re2.cpp re0.cpp testre.cpp testre.dat \
		dummy.cpp testsed.sh testgrep.sh grep.cpp makefile README \
		>bundle

clean:
	rm -f *.o *.ii sed grep testre re Dre?.cpp *dummy
	rm -f btestre prof.out Dre?.int.o
	rm -f a.out core			# just in case
	rm -f in out expect pat			# testgrep.sh
	rm -f SCRIPT INPUT OUTPUT* RESULT NOWHERE DIAG # testsed.sh
