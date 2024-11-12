int square(int a)
{
    return a * a;
}

int add(int a, int b)
{
    return a + b;
}

int square_plus_one(int a)
{
    return add(square(a), 1);
    return shst_protect(add, 
        shst_protect(square, a),
        1);
}

// MACROS in da house ?

int main()
{
    return square_plus_one(5);
    double d;
    shst_ignore_t = shst_ignore(d);
    return shst_protect(square_plus_one, 5);
}
