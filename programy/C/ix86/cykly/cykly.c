#include <stdio.h>
#include <math.h>

int main (void)
{
  float i;
  int b,x,y;

  printf("for: \n   ");
  for(i=1.1;i<10;i+=1.1)
  {
	printf("%2.1f ",i);
  }

  printf("\nwhile: \n   ");  
  i=1.1;
  while(i<10)
  {
	printf("%2.1f ",i);
	i+=1.1;
  }

  printf("\ndowhile: \n   ");  
  i=1.1;  
  do
  {
    printf("%2.1f ",i);
    i+=1.1;
  }
  while (i<10);
  
  printf("\nAbeceda: \n   ");
  for(b='a';b<='z';b++)
  {
    printf("%c ",b);  
  }
  
  printf("\nNasobilka: \n   ");
  for(y=1;y<=10;y++)
  {
    for(x=1;x<=10;x++)
    {
	  printf("%4d ",(x*y));
    }
	printf("\n   ");
  }
}
