#include <stdio.h>
#include <math.h>

int main (void)
{
  int a,b,c,d=0;
  printf("program vyresi rovnici ve tvaru Ax^2 + Bx + C = 0");
  				// zadej hodnoty
  printf("\n Zadej hodnotu A= ");
  scanf("%d",&a);
  printf("\n Zadej hodnotu B= ");
  scanf("%d",&b);
  printf("\n Zadej hodnotu C= ");
  scanf("%d",&c);
  
  if(a==0)
  {			
	if(b==0) printf("rovnice nema reseni."); 	// rovnice je neresitelna 
	else printf("X=%f",-(float)c/b );	// jedna se o linearni rovnici	  
  }
  else
  { 		// rovnice ma reseni
    d=((b*b)-(4*a*c));
	printf("\n discriminant rovnice D=%d",d);
	
	if (d>0)
	{
	  printf("\n rovnice ma reseni X1=%f X2=%f",( ((float)-b)+sqrt(d))/2*a,((float)-b)-sqrt(d))/2*a;
	}
	if (d==0)
	{
	  printf("\n rovnice ma jeden koren X=%f",(((float)-b)/2*a));
	}
	if (d<0)
	{
	  printf("\n reseni rovnice lezi v rovine komplexnich cisel.");
	  printf("\n X1=%f X2=%f");
	}
  }
}
