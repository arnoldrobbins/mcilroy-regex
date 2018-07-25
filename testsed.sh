#!/bin/sh
trap 'rm -f SCRIPT INPUT OUTPUT* RESULT NOWHERE DIAG' 0 1 2 13 15

echo
echo program under test:
which sed
echo test numbers below denote progress, not trouble
echo

# find a good awk
if awk 'func error() { }' </dev/null 2>/dev/null
then awk=awk
elif nawk 'func error() { }' </dev/null 2>/dev/null
then awk=nawk
elif gawk 'func error() { }' </dev/null 2>/dev/null
then awk=gawk
else echo cannot find good awk, good bye; exit 1
fi

rm -f SCRIPT INPUT OUTPUT* RESULT NOWHERE
$awk '
	/^TEST/ { if(phase!=0) error()
		  if(NF>=2) testno=$2; else testno++; next }
	/^SCRIPT/ { args = ""
		    for(i=2; i<=NF; i++) args = args " " $i }
	/^SCRIPT|^INPUT|^OUTPUT/ { phase=$1; printf "" >$1; next }
	/^END/ { print testno
		 command = "sed " args " -f SCRIPT <INPUT >RESULT"
		 r = system(command)
		 if(r) print "test " testno " returned " r
		 if(system("diff -u RESULT OUTPUT")) {
			print "test " testno " FAILED" }
		 close("SCRIPT"); close("INPUT"); close("OUTPUT")
		 phase=0; next }
	phase==0 { next }
		 { gsub(/;/,"\n",$0); print >phase }
' <<'FINI'

# if one of the sections (SCRIPT, INPUT, OUTPUT) of a test
# is missing the previous one is used.  tests labeled
# with the same number, e.g. 02A and 02B, are so linked
# and should be kept together in order.

# anything on the SCRIPT line becomes an argument to sed

# semicolons are replaced by newlines in each section

TEST 01A	# =, -n, blank lines
SCRIPT -n
;=;
INPUT
a;b;c
OUTPUT
1;2;3
END

TEST 01B	# comments, including #n
SCRIPT
#n;=;#comment
END

TEST 01C	# empty script
SCRIPT
OUTPUT
a;b;c
END

TEST 01D	# do-nothing script
SCRIPT
# nothing

END

TEST 01E	# substitution; regexp dot; &; substitution flag
SCRIPT
s/./&&/; s/./&x/; s/./&y/2; s/./&z/5
OUTPUT
axya;bxyb;cxyc
END

TEST 02A	# line counting, ranges, overlaps
SCRIPT
1,2d; 2,6d; 4,5d; 7,10d
INPUT
1;2;3;4;5;6;7;8
OUTPUT
3;6
END

TEST 02B	# -e
SCRIPT -e 1,2d -e 2,6d
4,5d; 7,10d
END

TEST 02C	# bypassing end of range
SCRIPT -n
2,5d; 1,3p; 1,6p
OUTPUT
1;1;6
END

TEST 02C	# negation, address $, print
SCRIPT
3,$!d; $d; p; 4,5d
OUTPUT
3;3;4;5;6;6;7;7
END

TEST 03		# regexp addresses, append, insert, change
SCRIPT
/a/a\;A\;A
/a/a\;B
/d/i\;D
/c/c\;C\;C
$i\;E
INPUT
a;b;c;d
OUTPUT
a;A;A;B;b;C;C;D;E;d
END

TEST 04		# braces
SCRIPT
3,7{
/a/s/a/&&/;/b/,/./s/./&&&/
}
INPUT
a;b;a;b;a;a;b;a;b
OUTPUT
a;b;aa;bbb;aaaa;aa;bbb;a;b
END

TEST 05A	# hold, get
SCRIPT
$!H;$!d;$G
INPUT
1;2;3;4
OUTPUT
4;;1;2;3
END

TEST 05B	# hold, exchange, get, brace without newline
SCRIPT
1{h;d;};3x;$g
OUTPUT
2;1;3
END

TEST 05D	# quit
SCRIPT
a\;x;q;=
OUTPUT
1;x
END

TEST 06		# next, regexp $
SCRIPT
s/$/x/;n;s/./&y/;N;s/.../&z/;=
INPUT
a;b;c
OUTPUT
ax;3;by;zc
END

TEST 07A	# newline, flag p
SCRIPT -n
/../s/./&\;/;p;P;s/\n//p
INPUT
a;bc;d
OUTPUT
a;a;b;c;b;bc;d;d
END

TEST 07B	# write, flag w
SCRIPT -n
/../s/./&\;/;w RESULT
s/\n/x/w RESULT
OUTPUT
a;b;c;bxc;d
END

TEST 08		# character classes, flag g
SCRIPT
1s/[a-z][a-z]*//
2s/[[:digit:]][[:digit:]]*//
3s/[^a-z]/X/g
INPUT
AZ`abyz{
/0189:
a1b2c3d
OUTPUT
AZ`{
/:
aXbXcXd
END

TEST 09		# null matches
SCRIPT
1,2s/a*/x/g
INPUT
123
aaa
OUTPUT
x1x2x3x
xx
END

TEST 10		# longest match, unmatched subexpressions
SCRIPT
s/\(...\)*\(..\)*/:\1:\2:/
INPUT
abc
abcd
abcde
abcdef
abcdefg
OUTPUT
:abc::
::cd:
:abc:de:
:def::
:abc:fg:
END

TEST 11		# metacharacters in substition
SCRIPT
1s/$/\&/
2s/$/\b/
3s/$/\\/
4s/$/\//
5s&$&\&&
INPUT
1;2;3;4;5
OUTPUT
1&;2b;3\;4/;5
END

TEST 12A	# branches
SCRIPT
:x
/a/{;s/a/x/;bx
}
INPUT
aaa;b;abca
OUTPUT
xxx;b;xbcx
END

TEST 12B	# long labels may be truncated
SCRIPT
:longlabel
/a/s/a/x/;tlonglabel
END

TEST 12C	# jump to end of script
SCRIPT
3b
/a/s/a/x/g
OUTPUT
xxx;b;abca
END


TEST 13A	# playing with end of bracket range
SCRIPT -n
/c/d
/a/,/d/{
	/b/,/c/{
		=
	}
}
INPUT
a;b;c;d;a
OUTPUT
2;4;5
END

TEST 13B	# end of change range
SCRIPT
/a/,/b/{
	/b/,/c/c\
x
}
OUTPUT
a;c;d;x
END

TEST 13C	# end of change range
INPUT
a;b;c;a;c;b;d
OUTPUT
a;c;x;d
END

TEST 14 	# end of change range
SCRIPT
/a/,/b/c\;c
INPUT
a;b;a
OUTPUT
c;c
END

TEST 15		# weird delimiters, remembered expression (could fail)
SCRIPT
1s1\(.\)\11\11
2s.\(\.\)\..\.\1.
3s*\(.\)\**\*\1*
4s&\(.\)\&&\1\&&
\1\(\1\)\11s//\1/
\&\(\&\)\&&s//\&b&/
INPUT
a1b
abc
abc
a&b
11b
a&&
OUTPUT
1b
.ac
*c
aa&b
1b
a&b&&
END

TEST 16		# 7680-char line, backreferencing, // could fail
SCRIPT
s/.*/&&&&&&&&/;s//&&&&&&&&/;s//&&&&&&&&/;h
s/[^8]//g
s/^\(.*\)\1\1\1$/\1/;s//\1/;s//\1/;s//\1/p
g
s/^\(.*\)\1\1\1$/\1/;s//\1/;s//\1/;s//\1/;s/\(.*\)\1/\1/
INPUT
123456787654321
OUTPUT
88
123456787654321
END

TEST 17		# r from w file, nonexistent r
SCRIPT -n
r NOWHERE
r RESULT
w RESULT
INPUT
1;2;3
OUTPUT
1;1;2;1;2;3
END

TEST 18A	# eof in n and N
SCRIPT
a\;1;n
INPUT
a
OUTPUT
a
1
END

TEST 18B
SCRIPT
a\;1;N
OUTPUT
1
END

TEST 19		# transliterate
SCRIPT
y/abc/ABC/
y:/\\:.:\/.\::
INPUT
abcABCabcdef
1/:2.\
OUTPUT
ABCABCABCdef
1\.2:/
END

TEST 20		# N, D
SCRIPT
=;N;p;D;s/.*/x/
INPUT
a;b;c
OUTPUT
1;a;b;2;b;c;3
END

TEST 20A	# D, G initial states
SCRIPT
1D;G;h
OUTPUT
b;;c;b;
END

TEST 21		# interaction of a,c,r
SCRIPT
$!a\;A
$!r INPUT
$!a\;B
1,2c\;C
INPUT
a;b;c
OUTPUT
A;a;b;c;B;C;A;a;b;c;B;c
END

TEST 22A	# multiple substitutions for null string
SCRIPT
s/a*/b/g
INPUT
aaa
ccc
OUTPUT
bb
bcbcbcb
END

TEST 22B
SCRIPT
s/a*/b/2
OUTPUT
aaab;cbcc
END

TEST 22C
SCRIPT
s/a*/b/3
OUTPUT
aaa;ccbc
END


FINI

TEST=a0		# perverse semicolons

echo $TEST
echo 'a;b' | sed 's;\;;x;'| sed -n '/axb/!s/.*/test'$TEST' FAILED/p'

TEST=a1		# multiple files, script argumen
echo $TEST
(echo a; echo b; echo c) >INPUT
case "`sed -n '$=' INPUT INPUT INPUT`" in
9)	: ;;
*)	echo test $TEST FAILED
esac

TEST=a2		# l command: weird chars, line folding
		# script argument, stdin
echo $TEST

awk 'BEGIN{printf "\ta%c\\\n", 1}' /dev/null |
sed -n '
	s/.*/&&&&&&&&&&/
	H
	H
	H
	G
	l
' | sed '
	:x
	/\\$/{
		$bz
		N
		s/\\\n//
		tx
		bz
	}
	s/\$$//
	s/\\n//
	s/\\n//
	s/\\n//
	s/\\n//
	tw
	:z
	s/.*/test '$TEST' FAILED/
	q
	:w
	s/\\ta\\001\\\\//g
	/^$/!bz
	d
'

TEST=a3		# multiple w files, multiple inputs, OUTPUT1 empty

echo $TEST

rm -f OUTPUT*
awk 'BEGIN{for(i=1;i<=9;i++) print i}' /dev/null |
sed -n '
	9,$d
	w OUTPUT9
	8,$d
	w OUTPUT8
	7,$d
	w OUTPUT7
	6,$d
	w OUTPUT6
	5,$d
	w OUTPUT5
	4,$d
	w OUTPUT4
	3,$d
	w OUTPUT3
	2,$d
	w OUTPUT2
	1,$d
	w OUTPUT1
'
sed -n '$=' OUTPUT1 OUTPUT2 OUTPUT3 OUTPUT4 OUTPUT5 \
	OUTPUT6 OUTPUT7 OUTPUT8 OUTPUT9 | 
sed -n '/36/!s/.*/test '$TEST' FAILED/p'

TEST=a4		# assorted errors; each field is an argument

echo $TEST
y=0
rm -f NOWHERE
while read x
do 	if sed $x </dev/null 2>DIAG && test ! -s DIAG
	then	case $y in 
		0)	echo bad or dubious usage not diagnosed:
			y=1
		esac
		echo \"$x\"
	fi
done <<'END'
= NOWHERE
-e :x -e :x
r/dev/null
-f NOWHERE

0p
//p
bx
/
/a
/\\
/a/
=;=
1,c
1,/
1,2,3p
/\\(/p
/\\1/p
s/a/b
s/a/b/q
s/a/b/g3
s/a/b/gg
s/a/b/pp
s/a/b/wNOWHERE
y/a
y/a/
y/a/b
y/aa/bb/
y/aa/ab/
y/a/bb/
y/aa/b/
1,2=
1,2q
:
\\
a
c
e
f
i
j
k
m
o
r
s
u
v
w
y
z
{
}
1
pq
dq
aq
!!p
1#
a\\
END

TEST=a5		# assorted errors; each line is a script

echo $TEST
y=0
while read x
do 	if sed "$x" </dev/null 2>DIAG && test ! -s DIAG
	then	case $y in 
		0)	echo bad or dubious usage not diagnosed:
			y=1
		esac
		echo \"$x\"
	fi
done <<'END'
w .
s /a/b/
s/a/b/w .
1, 2p
END

echo
echo Checking some customary extensions.
echo

echo 'Is semicolon usable as newline?'
if sed '=;=' </dev/null 2>/dev/null
then	echo Yes.
	echo 'Does semicolon terminate a label?'
	echo No. | sed ':x;s/No./Yes./'
else	echo No.
fi

echo 'Can previous regular expression be abbreviated as //'?
if sed '/a/s///' </dev/null 2>/dev/null
then	echo Yes.
	echo 'Is the meaning of // static or dynamic?'
	echo ab | sed '
		/a/bx
	     	/b/=
		:x
		s///
		/a/s/.*/Static./
		/b/s/.*/Dynamic./
	'
else	echo No.
fi

echo 'Is space optional in r and w commands?'
if sed 'w/dev/null' </dev/null 2>/dev/null
then	echo Yes.
else	echo No.
fi

echo 'Can \ precede a non-special character in regular expression?'
if sed '/\y/b' </dev/null 2>/dev/null
then	echo Yes.
else	echo No.
fi

echo 'Can \ precede a non-special character in substitution text?'
if sed 's/x/\y/' </dev/null 2>/dev/null
then	echo Yes.
else	echo No.
fi

echo 'Are spaces allowed between addresses?'
if sed '1, 2p' </dev/null 2>/dev/null
then	echo Yes.
else	echo No.
fi

echo 'Does ^ in \(^a\) denote anchoring?'
echo Yes. | sed '/\(^Yes.\)/!s/Yes/No/'
