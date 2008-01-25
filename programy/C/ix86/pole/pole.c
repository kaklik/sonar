/////////////////////////////////////////////////////////////////
// Program spocita pocet malych, velkych pismen a cislic v napsane radce.
/////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>

#define TRUE 1
#define FALSE 0

int rnd (int max)   // generuje nahodna cela cisla od 0 do zadane hodnoty max.
{
  return ( (int) (max * (rand()/(RAND_MAX+1.0))));
}

void init_rnd(void)  // inicializuje generator nahodnych cisel
{
  time_t t;
  unsigned int seed;
  time(&t);
  seed=(unsigned int) t;
  srand(seed);
}


int napln_pole(int *adresa,int velikost)  // naplni pole hodnotami
{
int i;
  for(i=0;i<velikost;i++) adresa[i]=rnd('Z'-'A'+1)+'A';
}

int vypis_pole(int *adresa,int velikost,int strana)
{
int i;
  if(strana) for(i=0; i<velikost;i++) printf("%c",*(adresa+i));
  else  for(i=velikost;i>=0;i--) printf("%c",*(adresa+i));
}

int setrid_pole(int *adresa,int velikost)
{
int pismeno,vetsipismeno;
int a,b;

  for(a=0; a<velikost;a++)
  {
    pismeno = *(adresa+a);

    for(b=a; b<velikost;b++)
    {
      if (*(adresa+b) > pismeno)
      {
        vetsipismeno = *(adresa+b);
        break;
      }
    }
    adresa[a] = vetsipismeno;
    adresa[b] = pismeno;
  }
}

int main()
{
int *pole;
int velikost;
 
  init_rnd();  

  printf("Zadejte velikost pole:");
  scanf("%d", &velikost);

  pole=(int*)malloc( sizeof(int) * velikost);
  if (NULL==pole) printf("pamet nebyla pridelena");

  napln_pole(pole,velikost);
  vypis_pole(pole,velikost,TRUE);
  setrid_pole(pole,velikost);
  vypis_pole(pole,velikost,FALSE);
  
  free (pole);
}
