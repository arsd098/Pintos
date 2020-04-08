#define F (1<<14)     //fixed point 1
#define INT_MAX ((1<<31)-1)
#define INT_MIN (-(1<<31))
// x and y denote fixed_point numbers in 17.14 format
// n is an integer

int int_to_fp(int n);             /* convert integer to fixed point */
int fp_to_int_round(int x);       /* convert fixed point to integer(ROUND) */
int fp_to_int(int x);             /* convert fixed point to integer(DOWN) */
int add_fp(int x, int y);         /* Add fixed points */
int add_mixed(int x, int n);      /* Add fixed point and integer */
int sub_fp(int x, int y);         /* Subtract fixed points */
int sub_mixed(int x, int n);      /* Subtract fixed point and integer */
int mult_fp(int x, int y);        /* Multiple fixed points */
int mult_mixed(int x, int n);     /* Multiple fixed point and integer */
int div_fp(int x, int y);         /* Divide fixed points */
int div_mixed(int x, int n);      /* Divide fixed point and integer */

/* Implement Function */

int
int_to_fp(int n)
{
  return n*F;
}

int 
fp_to_int_round(int x)
{
  if(x >= 0)
    return (x + F/2)/F;   
  else
    return (x - F/2)/F;
}

int 
fp_to_int(int x)
{
  return x/F;
}             

int 
add_fp(int x, int y)
{
  return x + y;
}        

int 
add_mixed(int x, int n)
{
  return x + n*F;
}     

int 
sub_fp(int x, int y)
{
  return x - y;
}         

int 
sub_mixed(int x, int n)
{
  return x - n*F;
}     

int 
mult_fp(int x, int y)
{
  return ((int64_t)x)*y/F;
}        

int 
mult_mixed(int x, int n)    
{
  return x*n;
}

int 
div_fp(int x, int y)
{
  return ((int64_t)x)*F/y;
}     

int 
div_mixed(int x, int n)
{
  return x/n;
}

