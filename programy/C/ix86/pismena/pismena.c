/////////////////////////////////////////////////////////////////
// Program spocita pocet malych, velkych pismen a cislic v napsane radce.
/////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>


int cti_radku(int* mala,int* velka,int* cisla)
{
int znaky = 0;
char c;

*mala=*velka=*cisla=0;

  while ((c=getchar()) != '\n') 
  {
    znaky++;
  
 //   if('a'< c <'z') (*mala)++;
 //   if('A'< c <'Z') (*velka)++;
 //   if('0'< c <'9') (*cisla)++;
 if (isdigit(c)) (*cisla)++;
 if (islower(c)) (*mala)++;
 if (isupper(c)) (*velka)++;
 
  }
  
  return znaky;
}


int main()
{

int mala=0,velka=0,cisla=0,znaky=0;

 znaky=cti_radku(&mala, &velka, &cisla);
 
 printf("Bylo napsano %d znaku z toho ",znaky);
 printf("%d malych pismen,",mala);
 printf("%d velkych pismen",velka);
 printf(" a %d cislic.",cisla);
}
