#include <assert.h>

int non_det();

int fib(int num)
{
  if(num > 2)
  {
    return fib(num - 1) + 1;
  }
  return num;
}

void main()
{
  int b = fib(non_det());
  assert(b);
}