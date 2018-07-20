## Doug McIlroy's C++ Regular Expression Matcher

### Introduction

This is Doug McIlroy's C++ regular expression matcher.
He wrote it while still at Bell Labs, for Cfront 4.0.

This code was converted to C by Glenn Fowler for use in
David Korn's [AST](https://github.com/att/ast) package.

In 2003 Dr. McIlroy attempted to modernize it, but did not get very far.

In July of 2017, I convinced him to send me the code as-is.
I did this because:

* I wanted to see this code preserved somewhere public.
* I have an interest in regular expression matchers.
* I also wanted to try to modernize it, for at least C++ 98.

### Contents

I have attempted to construct a Git repo to represent the code's history
as close as I can reconstruct it. The initial commit should represent
the state of the code as it was in 1998 when Dr. McIlroy left Bell Labs.
The dates on the files are based on what his filesystem shows,
as sent to me in email.

I have added a `ChangeLog` file that pretends to track the work,
but of course this is my own invention.

The `additional` directory contains some additional files Dr. McIlroy
sent me that seem to be part of his work to modernize the package in 2003,
so I have dated the additional files as being from 2003.

### Branches

The **master** branch has the original files in the initial commit.
The `additional` files are in the second commit, and the third commit
contains changes sent by Dr. McIlroy to compile on a modern Linux system.

One file was accidentally left out: `array.c`. This has been added witih
a commit date matching what Dr. McIlroy shows in his file system. I
have tagged this point in the code and also saved it aside in the
`original-code` branch.

Going forward will be my own work to modernize the package and to make
it buildable using `make` instead of the Bell Labs `mk` tool.

### Plans

Here are my thoughts:

1. Rename C++ files to have a `.cpp` extension.
2. Update the `Makefile` from Dr. McIlroy.
3. Compile the code without warnings using `g++`, and
if possible, also `clang`. (DONE, against versions 5, 7, and 8 of `g++`.)
4. Make the code pass the original tests supplied by Dr. McIlroy.
5. Review the code and try to improve the use of C++.

##### Last updated:

Fri Jul 20 10:52:33 IDT 2018

Arnold Robbins
[arnold at skeeve.com](mailto:arnold@skeeve.com)
