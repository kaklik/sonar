#include <stdio.h>


float nejvetsi (float a,float b,float c)
{
  if (a>b)
  {
    if (a>c) return (a);
  }
  else
  {
    if (b>c) return (b);
	else return (c);
  }
}

int main (void)
{
  float a,b,c=0;
  				// zadej hodnoty
  printf("\n Zadej hodnotu a= ");
  scanf("%f",&a);
  printf("\n Zadej hodnotu b= ");
  scanf("%f",&b);
  printf("\n Zadej hodnotu c= ");
  scanf("%f",&c);
  
  printf("nejvetsi hodnota je %f",nejvetsi(a,b,c));
  
}
