/* expandable array, begins pointing to preallocated
   space.  can be reallocated if space runs out */

#ifndef ARRAY_H
#define ARRAY_H

template<class T> struct Array {
	enum { SIZE = 20 };
	T *p;			// where the array is
	int size;		// how big it is
	T space[SIZE];		// initial space
	Array() { p = space; size = SIZE; }
	~Array() { if(p != space) delete(p); }
	T& operator[] (int i) { return p[i]; }
	int realloc(int i)	// when array must grow
	{
		i *= 2;
		T *q = new T[i];
		if(q == 0)
			return 1;
		memmove(q, p, size*sizeof(T));
		if(p != space)
			delete p;
		p = q;
		size = i;
		return 0;
	}
	int assure(int i) { return i>=size? realloc(i): 0; }
				// assure p[i] exists
	char *bytes() const { return (char *) p; }
};
#endif
