#include <assert.h>

void main()
{
  int i = 0;
  while(i < 1000)
  {
    i = i + 1;
  }
  assert(false);
}