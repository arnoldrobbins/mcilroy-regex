#!/bin/sh
# this test script for grep assumes that the regular expression
# package has been tested independently

echo
echo program under test:
which grep
echo test numbers below denote progress, not trouble
echo

# for tests that produce a file "out", given the answer in "expect"
# usage: compare testnumber
compare() {
	if cmp out expect >/dev/null 2>&1
	then :
	else echo test $1 failed
	fi
}

# for tests that produce a single line of output
# usage: check 'expected value' testnumber
check() {
	grep -x -q "$1" || echo $2 failed
}

# for tests that are expected to produce an empty "out" file
# usage: empty testnumber
empty() {
	if test -s out
	then echo test $1 failed
	fi
}

trap "rm -f in out expect pat; exit" 0 1 2 13 15

#---------------------------------------------
TEST=00		# -q, needed by check()
echo $TEST

grep -q . /dev/null > out && echo ${TEST}A failed
empty ${TEST}B
grep -q -v . /dev/null >out && echo ${TEST}C failed
empty ${TEST}D
(echo x | grep -q . >out) || echo ${TEST}E failed
empty ${TEST}F
(echo x | grep -v -q . >out) && echo ${TEST}G failed
empty ${TEST}H

#---------------------------------------------
TEST=01		# basic sanity of BRE, ERE, -x, -v, -e, 
echo $TEST

awk 'BEGIN{ for(i=0;i<=10000;i++) print i }' >in </dev/null
cat >expect <<!
1
10
100
1000
10000
!
grep '^10*$' in >out
compare ${TEST}A

grep -x '10*' in >out
compare ${TEST}B

grep -x -e '10*' in >out
compare ${TEST}C

grep -E '^1(00)*0?$' in >out
compare ${TEST}D

grep -x '[^[:digit:]]*[[=one=]][[.zero.]]\{0,\}' in >out
compare ${TEST}E

grep -x -E '[^[:digit:]]*[[=one=]][[.zero.]]{0,}' in >out
compare ${TEST}F

grep -e '^1\(\(0\)\2\)*$
^10\(\(0\)\2\)*$' in >out
compare ${TEST}G

grep -e '^1\(\(0\)\2\)*$' -e '^10\(\(0\)\2\)*$' in >out
compare ${TEST}H

grep -e '1\(\(0\)\2\)*' -e '10\(\(0\)\2\)*' -x in >out
compare ${TEST}I

grep -v -E '[2-9]|1.*1|^0' in >out
compare ${TEST}J

grep -E -x '1(0{0,2}){1,2}' in >out
compare ${TEST}K

grep -E '1*^10*$' in >out
compare ${TEST}L

#---------------------------------------------
TEST=02			# character classes, -c, -i
echo $TEST

# make a file of one-char lines, omitting NUL(0) and newline(10)
# assumes that no byte bigger than 127 is in any char class
awk 'BEGIN { for(i=1; i<256; i++) if(i!=10) printf "%c\n", i }' >in </dev/null
cat >expect <<!
62
52
2
31
10
94
26
95
32
5
26
22
62
52
52
52
!

grep -c . in | check '254' ${TEST}A

>out
grep -c '[[:alnum:]]' in >>out
grep -c '[[:alpha:]]' in >>out
grep -c '[[:blank:]]' in >>out
grep -c '[[:cntrl:]]' in >>out
grep -c '[[:digit:]]' in >>out
grep -c '[[:graph:]]' in >>out
grep -c '[[:lower:]]' in >>out
grep -c '[[:print:]]' in >>out
grep -c '[[:punct:]]' in >>out
grep -c '[[:space:]]' in >>out
grep -c '[[:upper:]]' in >>out
grep -c '[[:xdigit:]]' in >>out
grep -c -i '[[:alnum:]]' in >>out
grep -c -i '[[:alpha:]]' in >>out
grep -c -i '[[:lower:]]' in >>out
grep -c -i '[[:upper:]]' in >>out

compare ${TEST}B

#---------------------------------------------
TEST=03		# null expressions, dot
echo $TEST

# make a file of one-char lines, omitting NUL(0) and newline(10)
awk 'BEGIN { for(i=1; i<256; i++) if(i!=10) printf "%c\n", i }' >in </dev/null
cat >expect <<!
254
0
0
1
254
254
1
!

grep -c -e '' in >out
grep -c -x '' in >>out
grep -c -x -E '' in >>out
grep -c -e 'a' in >>out
grep -c -e '
a' in >>out
grep -c -e 'a
' in >>out
grep -c -x 'a
' in >>out

compare $TEST

#---------------------------------------------
TEST=04		# -f, -F, big pattern
echo $TEST

awk 'BEGIN{ for(i=0;i<10000;i++) print i }' >in </dev/null
awk 'BEGIN{ for(i=900; i<1000; i++) print i }' >pat </dev/null

cat >expect <<!
1900
8100
1900
8100
100
9900
100
9900
1900
8100
100
9900
!

grep -c -f pat <in >out
grep -c -f pat -v <in >>out
grep -c -F -fpat <in >>out
grep -c -F -fpat -v <in >>out
grep -c -x -fpat in >>out
grep -c -x -fpat -v in >>out
grep -c -x -F -f pat in >>out
grep -c -x -F -f pat -v in >>out
grep -c -E -fpat <in >>out
grep -c -E -fpat -v <in >>out
grep -c -x -E -f pat in >>out
grep -c -x -E -f pat -v in >>out

compare ${TEST}A

#---------------------------------------------
TEST=05			# -n, -c, -q, -l		
echo $TEST

awk 'BEGIN{ for(i=1;i<10000;i++) print i }' >in </dev/null

grep -n '\(.\)\(.\)\2\1' in >out
grep -v -q '^\(.*\):\1$' out && echo ${TEST}A failed
grep -c . out | check '90' ${TEST}B
grep -l . out | check 'out' ${TEST}C
grep -l . <out | check '(standard input)' ${TEST}D

cat >expect <<!
in
in
!

grep -l . in in >out
compare ${TEST}E

grep -l . /dev/null in in /dev/null >out
compare ${TEST}F

grep -l -v . in in >out
empty ${TEST}G

grep -l -q . in in >out
empty ${TEST}I

grep -c . /dev/null | check '0' ${TEST}J

cat >expect <<!
in:9999
in:9999
!

grep -c . in in >out
compare ${TEST}K

#---------------------------------------------
TEST=06			# exit status, -s		
echo $TEST

for q in '' -q
do
	for opt in -e -c -l
	do
		
		grep $q $opt . /dev/null >/dev/null
		case $? in
		0)	echo test ${TEST}A$q$opt failed ;;
		1)	: ;;
		*)	echo test ${TEST}B$q$opt failed
		esac
		
		echo x | grep $q $opt . >/dev/null
		case $? in
		0)	: ;;
		*)	echo test ${TEST}C$q$opt failed
		esac
		
		grep $q $opt . nonexistent 2>/dev/null
		case $? in
		0|1)	echo test ${TEST}D$q$opt failed
		esac
		
		grep -s $q $opt . nonexistent 2>out
		case $? in
		0|1)	echo test ${TEST}E$q$opt failed
		esac
		empty ${TEST}F
		
		echo x >in
		grep -s $q $opt . in nonexistent 2>out >/dev/null
		case $? in
		0)	: ;;
		*)	echo test ${TEST}G$q$opt failed
		esac
		empty ${TEST}H
		
	done
done

#---------------------------------------------
TEST=07			# -F, metacharacters, null scripts		
echo $TEST

# make a file of one-char lines, omitting NUL(0) and newline(10)
awk 'BEGIN { for(i=1; i<256; i++) if(i!=10) printf "%c\n", i }' >in </dev/null

grep -c -F '.
*
\' in 2>/dev/null | check '3' ${TEST}A

grep -c -F '' in 2>/dev/null | check '254' ${TEST}B
grep -c -x -F '' in 2>/dev/null | check '0' ${TEST}C

cat <<! >pat

x
!
cat <<! >expect
in:1
pat:2
!

grep -c -F -f pat in | check '254' ${TEST}D
grep -c -F -x -f pat in | check '1' ${TEST}E
grep -c -F -x -f pat in pat >out
compare ${TEST}F

#---------------------------------------------
TEST=08			# -x
echo $TEST

cat <<! >in
a
b
ab
ba
!

cat <<! >expect
b
ab
!

grep -x -E 'a.|b' in >out
compare ${TEST}A
