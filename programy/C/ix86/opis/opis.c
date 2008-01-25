/////////////////////////////////////////////////////////////////
// Program opise zadany text az do chvile, nez narazi na tecku.
/////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>


int clear_buffer()
{ 
int i=0;

  while (getchar() != '\n') i++;
  return i;
}


int main()
{

int c;

  printf("Program po stisknutí Enteru opíše zadaný vstup \n");
  
 /*Prvni zpusob reseni */
  do
  {
    c=getchar();
	putchar(c);
  }
  while (c != '.');
  
  printf("\n Pocet smazanych znaku %d \n",clear_buffer()); 
  
  /* Druhy zpusob reseni*/
  while ((c=getchar()) != '.') putchar(c);
  
  printf("\n Pocet smazanych znaku %d \n",clear_buffer());
  
  /* treti zpusob reseni*/
  while(putchar(getchar()) != '.');
  
}
