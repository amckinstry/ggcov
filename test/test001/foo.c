#include <stdio.h>
#include <stdlib.h>

int function_one(int x);

int
function_two(int x)
{
    return function_one(x>>1);
}

int
function_three(int x)
{
    x += 4;
    x -= 5;
    x++;
    return x>>1;
}

int
function_one(int x)
{
    if (x & 1)
    	x += function_two(x);
    else
    	x += function_three(x);
    return x;
}

int
unused_function(int x)
{
    x += 24;
    x -= 56;
    return x;
}

int
main(int argc, char **argv)
{
    int x, y;
    int i;
    
    for (i = 1 ; i < argc ; i++)
    {
    	x = atoi(argv[i]);
	if (x == 42)
	    unused_function(x);
    	y = function_one(x);
	printf("%d -> %d\n", x, y);
    }
    return 0;
}
