/* reallocate a growable array to have room for array[i].
   return 1 on running out of space. */

template<class T> int Array<T>::realloc(int i)
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
