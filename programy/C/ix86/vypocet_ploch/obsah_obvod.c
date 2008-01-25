#include <math.h>
#include <stdio.h>
#include "./vypocty.h"

float obvod_ctverce(float strana);	//vypocita obvod ctverce o strane a
float obsah_ctverce(float strana);	// vypocita obsah crverce o strane a
float obvod_kruznice(float polomer);	// spocita obvod kruznice z polomeru
float obsah_kruznice(float polomer);	// spocit obsah kruznice z polomeru 

int main( void )
{
float r;
float r2;
float a;
float a2;

  printf("Zadejte polomer r=");
  scanf("%f",&r);

  printf("obvod male kruznice Omk=%25.2f \n",obvod_kruznice(r));
  printf("obsah male kruznice Smk=%25.2f \n",obsah_kruznice(r));

  a=2*r;		// a je strana vnitrniho ctverce
  printf("obvod vnitrniho ctverce Oc=%25.2f \n",obvod_ctverce(a));
  printf("obsah vnitrniho ctverce Sc=%25.2f \n",obsah_ctverce(a));
  
  r2=M_SQRT2*r; 		// polomer kruznice
  printf("obvod kruznice Ok=%25.2f \n",obvod_kruznice(r2));
  printf("obsah kruznice Sk=%25.2f \n",obsah_kruznice(r2));
  
  a2=2*r2;		// strana vetsiho ctverce
  printf("obvod velkeho ctverce Ovc=%25.2f \n",obvod_ctverce(a2));
  printf("obsah velkeho ctverce Svc=%25.2f \n",obsah_ctverce(a2));

}

