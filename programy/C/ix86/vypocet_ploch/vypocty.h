/**************************************************************************
**
**	knihovna pro vypocet obsahu a ploch zakladnich obrazcu
**
***************************************************************************/

float obvod_ctverce(float strana)	//vypocita obvod ctverce o strane a
{
float obvod;

  obvod=4*strana;
  return (obvod); 
}

float obsah_ctverce(float strana)	// vypocita obsah crverce o strane a
{
float obsah;

  obsah=strana*strana;
  return (obsah);
} 

float obvod_kruznice(float polomer)	// spocita obvod kruznice z polomeru
{
float obvod;

  obvod=2*M_PI*polomer;
  return (obvod);
} 

float obsah_kruznice(float polomer)	// spocit obsah kruznice z polomeru 
{
float obsah;
  obsah=polomer*polomer*3,14;
  return (obsah);
}

