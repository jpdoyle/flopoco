#include <iostream>
#include <math.h>
#include <cstdlib>
#include "../utils.hpp"
#include "SecondInvTable.hpp"
using namespace std;



SecondInvTable::SecondInvTable(Target* target, int a1, int p1) : 
	Table(target, a1, a1+1) , a1(a1), p1(p1) 
 {
   if(2*a1!=(p1+a1)) {
     cerr << "??? SecondInvTable::SecondInvTable()  2*a1!=(p1+a1), exiting"<<endl;
     exit(1);
   }
 }

SecondInvTable::~SecondInvTable() {}

void SecondInvTable::setOperatorName(){
	ostringstream name; 
	name <<"InvTable_1_"<<a1<<"_"<<wOut;
	uniqueName_=name.str();
}


// int    SecondInvTable::double2input(double x){
//   int result;
// //   result = (int) floor(x*((double)(1<<(wIn-1))));
// //   if( result < minIn || result > maxIn) {
// //     cerr << "??? SecondInvTable::double2input:  Input "<< result <<" out of range ["<<minIn<<","<<maxIn<<"]";
// //     exit(1);
// //  }
//   return result;
// }

// double SecondInvTable::input2double(int x) {
//   double y;
//   cerr << "??? SecondInvTable::input2double not really verified ";
//   y = ((double)((1<<b)+x)) / ((double)(1<<b)); // exact division
//   return(y);
// }

// mpz_class    SecondInvTable::double2output(double x){
//   return (mpz_class) floor(-(x-1)*((double)(1<<(wOut-1))));
// }

//  double SecondInvTable::output2double(mpz_class x) {
//   double y=(1-(double)x) /  ((double)(1<<(wOut-1)));
//   return(y);
// }


mpz_class SecondInvTable::function(int A)
{
  mpz_class y;

  if(A<0 || A>=(1<<a1)) {
    cerr << "??? SecondInvTable::function, input out of range: "<< A<< " ... exiting";
    exit(1);
   }

  y = mpz_class( A << 1 );
  if(A!=0) {
    if(A>>(a1-1)){ // MSB of A
      y -= 2;
    }
    else
      {
	y -= 1;
      }
  }

  return  y;
}

