#include <iostream>
#include <math.h>
#include <cstdlib>
#include "../utils.hpp"
#include "OtherLogTable.hpp"
using namespace std;


// A table of log 
// -log(1-x) where x < 2^-p and x on a+1 bits.
// the log is then smaller than 2^-p+1
//  outputPrecision is the weight of the last bit in the real domain
OtherLogTable::OtherLogTable(Target* target, int wIn, int outputPrecision, int p, int which) : 
	Table(target, wIn, outputPrecision),  p(p),  which(which)
 {
   //   cout<<"**** "<<outputPrecision<<"   p="<<p<<" wIn="<<wIn<<endl;
	 setOperatorName();
}

OtherLogTable::~OtherLogTable() {}

void OtherLogTable::setOperatorName(){
	ostringstream name; 
	name <<"LogTable_"<<which<<"_"<<wIn<<"_"<<wOut;
	uniqueName_=name.str();
}


int    OtherLogTable::double2input(double x){
  int result;
  cerr << "??? OtherLogTable::double2input not yet implemented ";
  exit(1);
  return result;
}


// uses log1p ! 
double OtherLogTable::input2double(int x) {
  double d; 
  // computation in double is exact as long as we don't want a quad
  // operator...

  if(x==0) 
    d = 0.;
  else{
    d = (double) (-(x<<1));
    if(which==1) {
      if(x>>(wIn-1)) // MSB of x
	d+=2.;  // MARCHE SEULEMENT SI a[1]=a[0]+1 !!!!!
      else 
	d+=1.; 
    } 
    else 
      d += 1.;
  
  }
  d = d / ((double) (1<<(p+wIn+1)));
  return d; 
}





mpz_class    OtherLogTable::double2output(double y){
  mpz_class z; 
  z =(mpz_class) (  y *  ((double)(((int64_t)1)<< outputPrecision)) );
  if (0 != z>>wOut) {
    cerr<<"OtherLogTable::double2output: output does not fit output format"<<endl; 
  }
  return z; 
}



double OtherLogTable::output2double(mpz_class x) {
  cerr<<" OtherLogTable::output2double TODO"; exit(1);
  //double y=((long double)x) /  ((long double)(1<<outputPrecision));
  //return(y);
}





mpz_class OtherLogTable::function(int x) {
  //  return  double2output(-log1p(input2double(x)));
  mpz_class result;
  double apprinv;
  mpfr_t i,l;
  mpz_t r;
  
  mpfr_init(i);
  mpfr_init(l);
  mpz_init2(r,400);

  apprinv = input2double(x);
  mpfr_set_d(i, apprinv, GMP_RNDN);
  mpfr_log1p(l, i, GMP_RNDN);
  mpfr_neg(l, l, GMP_RNDN);
  //mpfr_shift_left(l, p+wOut); // TODO CHECK IT WAS EQUIVALENT 
  mpfr_mul_2si(l, l, p+wOut, GMP_RNDN);
  mpfr_get_z(r, l, GMP_RNDD);
  result=mpz_class(r);
  mpfr_clear(i);
  mpfr_clear(l);
  mpz_clear(r);
  return  result;
}
