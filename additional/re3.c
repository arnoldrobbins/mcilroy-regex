void
printnew(Rex *rex)
{
	int t;
	printf("new %s\n",
		rex==0? "ESPACE":
		(t=rex->type)==OK? "OK":
		t==ANCHOR? "ANCHOR":
		t==END? "END":
		t==DOT? "DOT":
		t==ONECHAR? "ONECHAR":
		t==STRING? "STRING":
		t==KMP? "KMP":
		t==TRIE? "TRIE":
		t==CLASS? "CLASS":
		t==BACK? "BACK":
		t==SUBEXP? "SUBEXP":
		t==ALT? "ALT":
		t==REP? "REP":
		t==TEMP? "TEMP":
		"HUH");
}
