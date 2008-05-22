#ifndef SECONDINVTABLE_H
#define SECONDINVTABLE_H

#include "../Table.hpp"

class SecondInvTable : public Table
{
 public:
  SecondInvTable(int a1, int p1);
  
  ~SecondInvTable();
  

  mpz_class function(int x);

//   int    double2input(double x);

//   double input2double(int x);
  
//   mpz_class double2output(double x);

//   double output2double(mpz_class x);

//   double maxMulOut;
//   double minMulOut;

  int a1;
  int p1;
private:

  
};






#endif //SECONDINVTABLE_H

