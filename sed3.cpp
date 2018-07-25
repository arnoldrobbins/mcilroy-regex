#include <string.h>
#include "sed.h"

void docopy(uchar *where, int n);
int dosub(uchar *where, uchar *rp);

Text retemp;	/* holds a rewritten regex, without delimiter */

int
recomp(Text *rebuf, Text *t, int delim)
{
	static int lastre;
	uchar *w;
	vacate(&retemp);
	for(w=t->w; ; retemp.w++,w++) {
		assure(&retemp, 2)
		*retemp.w = *w;
		if(*w == delim)
			break;
		else if(*w==0 || *w=='\n')
			syntax("unterminated address");
		else if(*w != '\\')
			continue;
		else if(*++w==delim)
			*retemp.w = delim;
		else if(*w == 'n')
			*retemp.w = '\n';
		else if(*w==0 || *w=='\n')
			syntax("unterminated regular expression");
		else {
			assure(&retemp, 2);
			*++retemp.w = *w;
		}
	}
	*retemp.w = 0;

	assure(rebuf, sizeof(regex_t));
	if(*retemp.s != 0) {
		if(regcomp((regex_t*)rebuf->w,(char*)retemp.s,options) != 0)
			syntax("bad regular expression");
		lastre = rebuf->w - rebuf->s;
		rebuf->w += sizeof(regex_t);
	} else if(rebuf->w == rebuf->s)
		syntax("no previous regular expression");
	t->w = w + 1;
	return lastre;
}

Text gendata;

#define NMATCH 10
regmatch_t matches[NMATCH];
#define so matches[0].rm_so
#define eo matches[0].rm_eo

int
substitute(regex_t *re, Text* data, uchar *rhs, int n)
{
	Text t;
	uchar *where = data->s;
	if(regexec(re, (char*)data->s, NMATCH, matches, 0))
		return 0;
	vacate(&gendata);
	if(n == 0)
		do {
			docopy(where, so);
			if(!dosub(where, rhs))
				return 0;
			where += eo;
			if(eo == so) {
				if(where < data->w)
					docopy(where++, 1);
				else
					goto done;
			}
		} while(regexec(re, (char*)where, NMATCH, matches, REG_NOTBOL) == 0);
	else {
		while(--n > 0) {
			where += eo;
			if(eo == so) {
				if(where < data->w)
					where++;
				else
					return 0;
			}
			if(regexec(re, (char*)where, NMATCH, matches, REG_NOTBOL))
				return 0;
		}
		docopy(data->s, where-data->s+so);
		if(!dosub(where, rhs))
			return 0;
		where += eo;
	}			
	eo = so = data->w - where;
	docopy(where, so);
done:
	exch(gendata, *data, t);
	return 1;
}

void
docopy(uchar *where, int n)
{
	assure(&gendata, n+1);
	memmove(gendata.w, where, n);
	gendata.w += n;
	*gendata.w = 0;
}

	/* interpretation problem: if there is no match for \1, say,
           does the substitition occur?  dosub uses a null string.
	   a change where indicated will abort the substitution */
	
int
dosub(uchar *where, uchar *rp)
{
	int c, n;
	regmatch_t *m;

	while((c = *rp++) != 0) {
		if(c == '\\') {
			c = *rp++;
			if (c >= '1' && c <= '9') {
				m = matches + c - '0';
				if(m->rm_eo == -1)
					continue;   /* or return 0 */
				n = m->rm_eo - m->rm_so;
				assure(&gendata, n);
				memmove(gendata.w,where+m->rm_so,n);
				gendata.w += n;
				continue;
			}
		} else if(c == '&') {
				assure(&gendata, eo-so);
				memmove(gendata.w,where+so,eo-so);
				gendata.w += eo-so;
				continue;
		}
		assure(&gendata, 1);
		*gendata.w++ = c;
	}
	return 1;
}
