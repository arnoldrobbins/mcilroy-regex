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
	int realloc(int);	// when array must grow
	int assure(int i) { return i>=size? realloc(i): 0; }
				// assure p[i] exists
};
#endif
