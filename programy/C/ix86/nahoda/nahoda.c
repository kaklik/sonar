#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int rnd (int max)
{
  return ( (int) (max * (rand()/(RAND_MAX+1.0))));
}

void init_rnd(void)
{
  time_t t;
  unsigned int seed;
  time(&t);
  seed=(unsigned int) t;
  srand(seed);
}

int main()
{

int i=1,hledane,pokus;
  
  init_rnd();
  
  printf("program uhodne cislo od 0 do 100 \n ");
  
  hledane=rnd(10);
  
  do
  {
    printf("Pokus cislo %d   ",i);
	scanf("%d", &pokus);
	i++;
	if (pokus > hledane)  printf("Zadane cislo je vetsi nez hledane \n ");
	if (pokus < hledane)  printf("Zadane cislo je mensi nez hledane \n ");
	else  printf("Spravne cislo bylo nalezeno \n ");
  }
  while (pokus != hledane);
}
