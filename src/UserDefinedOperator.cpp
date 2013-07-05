// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "UserDefinedOperator.hpp"

using namespace std;
namespace flopoco {




	UserDefinedOperator::UserDefinedOperator(Target* target, int param0_, int param1_) : Operator(target), param0(param0_), param1(param1_) {
		/* constructor of the UserDefinedOperator
		   Target is the targeted FPGA : Stratix, Virtex ... (see Target.hpp for more informations)
		   param0 and param1 are some parameters declared by this Operator developpers, 
		   any number can be declared, you will have to modify 
		   -> this function,  
		   -> the prototype of this function (available in UserDefinedOperator.hpp)
		   -> the lines in main.cpp where the command line arguments are parsed in order to generate this UserDefinedOperator
		*/
		/* In this constructor we are going to generate an operator that takes as input three bit vectors X,Y,Z of lenght param0, treats them as unsigned integers, sums them and then output the last param1 bit of the sum adding the first bit of the sum (most significant) in front of this output, all the vhdl code needed by this operator has to be generated in this function */

		// definition of the source file name, used for info and error reporting using REPORT 
		srcFileName="UserDefinedOperator";

		// definition of the name of the operator
		ostringstream name;
		name << "UserDefinedOperator_" << param0 << "_" << param1;
		setName(name.str());
		// Copyright 
		setCopyrightString("ACME and Co 2010");

		/* SET UP THE IO SIGNALS
		   Each IO signal is declared by addInput(name,n) or addOutput(name,n) 
		   where name is a string that stands for the name of the variable and 
		   n is an integer (int)   that stands for the length of the corresponding 
		   input/output */

		// declaring inputs
		addInput ("X" , param0);
		addInput ("Y" , param0);
		addInput ("Z" , param0);
		addFullComment(" addFullComment for a large comment ");
		addComment("addComment for small left-aligned comment");

		// declaring output
		addOutput("S" , param1);


		/* Some piece of information can be delivered to the flopoco user if  the -verbose option is set
		   [eg: flopoco -verbose=0 UserDefinedOperator 10 5 ]
		   , by using the REPORT function.
		   There is three level of details
		   -> INFO for basic information ( -verbose=1 )
		   -> DETAILED for complete information, includes the INFO level ( -verbose=2 )
		   -> DEBUG for complete and debug information, to be used for getting information 
		   during the debug step of operator development, includes the INFO and DETAILED levels ( -verbose=3 )
		*/
		// basic message
		REPORT(INFO,"Declaration of UserDefinedOperator \n");

		// more detailed message
		ostringstream detailedMsg;
		detailedMsg  << "this operator has received two parameters " << param0 << " and " << param1;
		REPORT(DETAILED,detailedMsg.str());
  
		// debug message for developper
		REPORT(DEBUG,"debug of UserDefinedOperator");



		/* vhdl is the stream which receives all the vhdl code, some special functions are
		   available to smooth variable declaration and use ... 
		   -> when first using a variable (Eg: T), declare("T",64) will create a vhdl
		   definition of the variable : signal T and includes it it the header of the architecture definition of the operator

		   Each code transmited to vhdl will be parsed and the variables previously declared in a previous cycle will be delayed automatically by a pipelined register.
		*/
		vhdl << declare("T", param0+1) << " <= ('0' & X) + ('O' & Y);" << endl;


		/* declaring a new cycle, each variable used after this line will be delayed 
		   with respect to his state in the precedent cycle
		*/
		nextCycle();
		vhdl << declare("R",param0+2) << " <=  ('0' & T) + (\"00\" & Z);" << endl;

		/* the use(variable) is a deprecated function, that can still be encoutered 
		   in old Flopoco's operators, simply use vhdl << "S" (flopoco will generate the correct delayed value as soon as a previous declare("S") exists */
		// we first put the most significant bit of the result into R
		vhdl << "S <= (R" << of(param0 +1) << " & ";
		// and then we place the last param1 bits
		vhdl << "R" << range(param1 - 1,0) << ");" << endl;
	};

	
	void UserDefinedOperator::emulate(TestCase * tc) {
		/* This function will be used when the TestBench command is used in the command line
		   we have to provide a complete and correct emulation of the operator, in order to compare correct output generated by this function with the test input generated by the vhdl code */
		/* first we are going to format the entries */
		mpz_class sx = tc->getInputValue("X");
		mpz_class sy = tc->getInputValue("Y");
		mpz_class sz = tc->getInputValue("Z");

		/* then we are going to manipulate our bit vectors in order to get the correct output*/
		mpz_class sr;
		mpz_class stmp;
		stmp = sx + sy + sz;
		sr = (stmp % mpzpow2(param1)); // we delete all the bits that do not fit in the range (param1 - 1 downto 0);
		sr += (stmp / mpzpow2 (param0+1)); // we add the first bit

		/* at the end, we indicate to the TestCase object what is the expected
		   output corresponding to the inputs */
		tc->addExpectedOutput("R",sr);
	}


	void UserDefinedOperator::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}
}//namespace
