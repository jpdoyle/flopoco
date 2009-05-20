/*

 * An FP collision operator for FloPoCo

This is mostly an example of a coarse-grain, nonstandard operator.

A 3D collision detector inputs 3 FP coordinates X,Y and Z and
the square of a radius, R2, and computes a boolean predicate which is
true iff X^2 + Y^2 + Z^2 < R2

There are two versions, selectable by the useFPOperators parameter.
One combines existing FloPoCo floating-point operators, and the other
one is a specific datapath designed in the FloPoCo philosophy.

As this is a floating-point operator, each versions has its "grey
area", when X^2+Y^2+Z^2 is very close to R2.  In this case the
predicate may be wrong with respect to the infinitely accurate result.

The grey area of the combination of FP operators is about 2.5 units in
the last place of R2.  The pure FloPoCo version (which is a lot
smaller and faster) is more accurate, with a grey area smaller than 1
ulp of R2.

TODO: turn it into a more generally useful sum-of-square, and manage exceptions.

 
 *
 * Author : Florent de Dinechin
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/
#include <fstream>
#include <sstream>
#include <math.h>	// for NaN
#include "Collision.hpp"
#include "FPNumber.hpp"
#include "utils.hpp"


using namespace std;

extern vector<Operator *> oplist;



Collision::Collision(Target* target, int wE, int wF, int optimize)
	: Operator(target), wE(wE), wF(wF)
{
	setCopyrightString("F. de Dinechin (2009)");

	ostringstream o;
	o << "Collision_" << wE << "_" << wF;
	if(!optimize)
		o << "_FP";
	setName(o.str());

	addFPInput("X", wE, wF);
	addFPInput("Y", wE, wF);
	addFPInput("Z", wE, wF);
	addFPInput("R2", wE, wF);
	addOutput("P", 1, 2); // 2 possible values in the grey area 

	vhdl << tab << declare("R2pipe", wE+wF) << " <= R2" << range(wE+wF-1, 0) << ";" << endl; 

	if(!optimize) {
		//////////////////////////////////////////////////////////////////:
		//            A version that assembles FP operators             //
		
		FPMultiplier* mult = new FPMultiplier(target, wE, wF, wE, wF, wE, wF, 1);
		oplist.push_back(mult);
		FPAdder* add =  new FPAdder(target, wE, wF, wE, wF, wE, wF);
		oplist.push_back(add);
		
		inPortMap (mult, "X", "X");
		inPortMap (mult, "Y", "X");
		outPortMap(mult, "R", "X2");
		vhdl << instance(mult, "multx");
		
		inPortMap (mult, "X", "Y");
		inPortMap (mult, "Y", "Y");
		outPortMap(mult, "R", "Y2");
		vhdl << instance(mult, "multy");
		
		inPortMap (mult, "X", "Z");
		inPortMap (mult, "Y", "Z");
		outPortMap(mult, "R", "Z2");
		vhdl << instance(mult, "multz");
		
		syncCycleFromSignal("Z2", false);
		nextCycle(); 
		
		inPortMap (add, "X", "X2");
		inPortMap (add, "Y", "Y2");
		outPortMap(add, "R", "X2PY2");
		vhdl << instance(add, "add1");
		
		syncCycleFromSignal("X2PY2", false);
		nextCycle(); 
		
		inPortMap (add, "X", "X2PY2");
		inPortMap (add, "Y", "Z2");
		outPortMap(add, "R", "X2PY2PZ2");
		vhdl << instance(add, "add2");
		
		syncCycleFromSignal("X2PY2PZ2", false);
		nextCycle(); 
		
		// TODO An IntAdder here

		// collision if X2PY2PZ2 < R2, ie X2PY2PZ2 - R2 < 0
		vhdl << tab << declare("diff", wE+wF+1) << " <= ('0'& " << use("X2PY2PZ2") << range(wE+wF-1, 0) << ")  -  ('0'& " << use("R2pipe") << ");" << endl;
		vhdl << tab <<  "P <= " << use("diff") << "(" << wE+wF << ");"  << endl ;
	}



	else { ////////////////// here comes the FloPoCo version	//////////////////////////:

		// Error analysis
		// 3 ulps(wF+g) in the multiplier truncation
		// Again 2 ulps(wF+g) in the shifter output truncation
		// Normalisation truncation: either 0 (total 5), or 1 ulp(wF+g) but dividing the previous by 2 (total 3.5)
		// Total max 5 ulps, we're safe with 3 guard bits
		
		// guard bits for a faithful result
		int g=3; 

		// extract the three biased exponents. 
		vhdl << declare("EX", wE) << " <=  X" << range(wE+wF-1, wF)  << ";" << endl;
		vhdl << declare("EY", wE) << " <=  Y" << range(wE+wF-1, wF) << ";" << endl;
		vhdl << declare("EZ", wE) << " <=  Z" << range(wE+wF-1, wF) << ";" << endl;
		
		// determine the max of the exponents
		vhdl << declare("DEXY", wE+1) << " <=   ('0' & EX) - ('0' & EY);" << endl;
		vhdl << declare("DEYZ", wE+1) << " <=   ('0' & EY) - ('0' & EZ);" << endl;
		vhdl << declare("DEXZ", wE+1) << " <=   ('0' & EX) - ('0' & EZ);" << endl;
		vhdl << declare("XltY") << " <=   DEXY("<< wE<<");" << endl;
		vhdl << declare("YltZ") << " <=   DEYZ("<< wE<<");" << endl;
		vhdl << declare("XltZ") << " <=   DEXZ("<< wE<<");" << endl;
		
		// rename the exponents  to A,B,C with A>=(B,C)
		vhdl << tab << declare("EA", wE)  << " <= " << endl
			  << tab << tab << use("EZ") << " when (" << use("XltZ") << "='1') and (" << use("YltZ") << "='1')  else " << endl
			  << tab << tab << use("EY") << " when (" << use("XltY") << "='1') and (" << use("YltZ") << "='0')  else " << endl
			  << tab << tab << use("EX") << "; " << endl;
		vhdl << tab << declare("EB", wE)  << " <= " << endl
			  << tab << tab << use("EX") << " when (" << use("XltZ") << "='1') and (" << use("YltZ") << "='1')  else " << endl
			  << tab << tab << use("EZ") << " when (" << use("XltY") << "='1') and (" << use("YltZ") << "='0')  else " << endl
			  << tab << tab << use("EY") << "; " << endl;
		vhdl << tab << declare("EC", wE)  << " <= " << endl
			  << tab << tab << use("EY") << " when (" << use("XltZ") << "='1') and (" << use("YltZ") << "='1')  else " << endl
			  << tab << tab << use("EX") << " when (" << use("XltY") << "='1') and (" << use("YltZ") << "='0')  else " << endl
			  << tab << tab << use("EZ") << "; " << endl;
		
		nextCycle();
		
		// Now recompute our two shift values -- they were already computed at cycle 0 but it is cheaper this way.
		vhdl << declare("fullShiftValB", wE) << " <=  (" << use("EA") << range(wE-2,0) << " - " << use("EB")<< range(wE-2,0) << ") & '0' ; -- positive result, no overflow " << endl;
		vhdl << declare("fullShiftValC", wE) << " <=  (" << use("EA") << range(wE-2,0) << " - " << use("EC")<< range(wE-2,0) << ") & '0' ; -- positive result, no overflow " << endl;
	
		Shifter* rightShifter = new Shifter(target,wF+g+2, wF+g+2,Right); 
		oplist.push_back(rightShifter);

		int sizeRightShift = rightShifter->getShiftInWidth(); 
		// code cut from FPAdder

		vhdl<<tab<<declare("shiftedOutB") << " <= "; 
		if (wE>sizeRightShift){
			for (int i=wE-1;i>=sizeRightShift;i--) {
				vhdl<< "fullShiftValB("<<i<<")";
				if (i>sizeRightShift)
					vhdl<< " or ";
			}
			vhdl<<";"<<endl;
		}
		else
			vhdl<<tab<<"'0';"<<endl; 
		
		vhdl<<tab<<declare("shiftValB",sizeRightShift) << " <= " ;
		if (wE>sizeRightShift) {
			vhdl << "fullShiftValB("<< sizeRightShift-1<<" downto 0"<<")"
				  << " when shiftedOutB='0'"<<endl
				  <<tab << tab << "    else CONV_STD_LOGIC_VECTOR("<<wF+g+1<<","<<sizeRightShift<<") ;" << endl; 
		}		
		else if (wE==sizeRightShift) {
			vhdl<<tab<<"fullShiftValB;" << endl ;
		}
		else 	{ //  wE< sizeRightShift
			vhdl<<tab<<"CONV_STD_LOGIC_VECTOR(0,"<<sizeRightShift-wE <<") & fullShiftValB;" <<	endl;
		}



		vhdl<<tab<<declare("shiftedOutC") << " <= "; 
		if (wE>sizeRightShift){
			for (int i=wE-1;i>=sizeRightShift;i--) {
				vhdl<< "fullShiftValC("<<i<<")";
				if (i>sizeRightShift)
					vhdl<< " or ";
			}
			vhdl<<";"<<endl;
		}
		else
			vhdl<<tab<<"'0';"<<endl; 
		
		vhdl<<tab<<declare("shiftValC",sizeRightShift) << " <= " ;
		if (wE>sizeRightShift) {
			vhdl << "fullShiftValC("<< sizeRightShift-1<<" downto 0"<<")"
				  << " when shiftedOutC='0'"<<endl
				  <<tab << tab << "    else CONV_STD_LOGIC_VECTOR("<<wF+g+1<<","<<sizeRightShift<<") ;" << endl; 
		}		
		else if (wE==sizeRightShift) {
			vhdl<<tab<<"fullShiftValC;" << endl ;
		}
		else 	{ //  wE< sizeRightShift
			vhdl<<tab<<"CONV_STD_LOGIC_VECTOR(0,"<<sizeRightShift-wE <<") & fullShiftValC;" <<	endl;
		}



		// Back to cycle 0 for the significand datapath
		setCycle(0);
		// Square the significands 
#define USE_SQUARER 1
#if  USE_SQUARER
		IntSquarer* mult = new IntSquarer(target,  1+ wF);
#else
		IntMultiplier* mult = new IntMultiplier(target, 1+ wF, 1+ wF);
#endif
		oplist.push_back(mult);
		
		vhdl << tab << declare("mX", wF+1)  << " <= '1' & X" << range(wF-1, 0) << "; " << endl;
		
		inPortMap (mult, "X", "mX");
#if  !USE_SQUARER
		inPortMap (mult, "Y", "mX");
#endif
		outPortMap(mult, "R", "mX2");
		vhdl << instance(mult, "multx");
	
		vhdl << tab << declare("mY", wF+1)  << " <= '1' & Y" << range(wF-1, 0) << "; " << endl;

		inPortMap (mult, "X", "mY");
#if  !USE_SQUARER	
		inPortMap (mult, "Y", "mY");
#endif
		outPortMap(mult, "R", "mY2");
		vhdl << instance(mult, "multy");
		
		vhdl << tab << declare("mZ", wF+1)  << " <= '1' & Z" << range(wF-1, 0) << "; " << endl;
		
		inPortMap (mult, "X", "mZ");
#if  !USE_SQUARER	
		inPortMap (mult, "Y", "mZ");
#endif
		outPortMap(mult, "R", "mZ2");
		vhdl << instance(mult, "multz");

		syncCycleFromSignal("mZ2", false);
		
		// truncate the three results to wF+g+2
		int prodsize = 2+2*wF;
		vhdl << tab << declare("X2t", wF+g+2)  << " <= " << use("mX2") << range(prodsize-1, prodsize - wF-g-2) << "; " << endl;
		vhdl << tab << declare("Y2t", wF+g+2)  << " <= " << use("mY2") << range(prodsize-1, prodsize - wF-g-2) << "; " << endl;
		vhdl << tab << declare("Z2t", wF+g+2)  << " <= " << use("mZ2") << range(prodsize-1, prodsize - wF-g-2) << "; " << endl;
	
		nextCycle(); 

	
		// Now we have our three FP squares, we rename them to A,B,C with A>=(B,C) 
		// only 3 3-muxes
		
		vhdl << tab << declare("MA", wF+g+2)  << " <= " << endl
			  << tab << tab << use("Z2t") << " when (" << use("XltZ") << "='1') and (" << use("YltZ") << "='1')  else " << endl
			  << tab << tab << use("Y2t") << " when (" << use("XltY") << "='1') and (" << use("YltZ") << "='0')  else " << endl
			  << tab << tab << use("X2t") << "; " << endl;
		vhdl << tab << declare("MB", wF+g+2)  << " <= " << endl
			  << tab << tab << use("X2t") << " when (" << use("XltZ") << "='1') and (" << use("YltZ") << "='1')  else " << endl
			  << tab << tab << use("Z2t") << " when (" << use("XltY") << "='1') and (" << use("YltZ") << "='0')  else " << endl
			  << tab << tab << use("Y2t") << "; " << endl;
		vhdl << tab << declare("MC", wF+g+2)  << " <= " << endl
			  << tab << tab << use("Y2t") << " when (" << use("XltZ") << "='1') and (" << use("YltZ") << "='1')  else " << endl
			  << tab << tab << use("X2t") << " when (" << use("XltY") << "='1') and (" << use("YltZ") << "='0')  else " << endl
			  << tab << tab << use("Z2t") << "; " << endl;
		

		//Synchronize exponent and significand datapath
		setCycleFromSignal("MA", false); // useless here but harmless, too
		syncCycleFromSignal("shiftValB", false);
		nextCycle();
		

		inPortMap  (rightShifter, "X", "MB");
		inPortMap  (rightShifter, "S", "shiftValB");
		outPortMap (rightShifter, "R","shiftedB");
		vhdl << instance(rightShifter, "ShifterForB");

		inPortMap  (rightShifter, "X", "MC");
		inPortMap  (rightShifter, "S", "shiftValC");
		outPortMap (rightShifter, "R","shiftedC");
		vhdl << instance(rightShifter, "ShifterForC");
		
		// superbly ignore the bits that are shifted out
		syncCycleFromSignal("shiftedB", false);
		int shiftedB_size = getSignalByName("shiftedB")->width();
		vhdl << tab << declare("alignedB", wF+g+2)  << " <= " << use("shiftedB") << range(shiftedB_size-1, shiftedB_size -(wF+g+2)) << "; " << endl;
		vhdl << tab << declare("alignedC", wF+g+2)  << " <= " << use("shiftedC") << range(shiftedB_size-1, shiftedB_size -(wF+g+2)) << "; " << endl;
		
		nextCycle();
		
		vhdl << tab << declare("paddedA", wF+g+4)  << " <= \"00\" & " << use("MA") << "; " << endl;
		vhdl << tab << declare("paddedB", wF+g+4)  << " <= \"00\" & " << use("alignedB") << "; " << endl;
		vhdl << tab << declare("paddedC", wF+g+4)  << " <= \"00\" & " << use("alignedC") << "; " << endl;
		
		IntAdder* adder = new IntAdder(target,wF+g+4);
		oplist.push_back(adder);
		

		// TODO multi-op adder! using two instances of our pipelined adder is inefficient
		
		inPortMap   (adder, "X", "paddedA");
		inPortMap   (adder, "Y", "paddedB");
		inPortMapCst(adder, "Cin", "'0'"); // a 1 would compensate the two truncations in the worst case -- to explore
		outPortMap  (adder, "R","APB");
		vhdl << instance(adder, "adder1");

		syncCycleFromSignal("APB", false);
		// at least we do not need a nextCycle here, unless the adders are not pipelined
		if(adder->getPipelineDepth()==0)
			nextCycle();

		inPortMap   (adder, "X", "APB");
		inPortMap   (adder, "Y", "paddedC");
		inPortMapCst(adder, "Cin", "'0'"); 
		outPortMap  (adder, "R","sum");
		vhdl << instance(adder, "adder2");

		syncCycleFromSignal("sum", false);
		nextCycle();
		
		// Possible 3-bit normalisation, with a truncation
		vhdl << tab << declare("finalFraction", wF+g)  << " <= " << endl
			  << tab << tab << use("sum") << range(wF+g+2,3) << "   when " << use("sum") << "(" << wF+g+3 << ")='1'    else " << endl
			  << tab << tab << use("sum") << range(wF+g+1, 2) <<  "   when (" << use("sum") << range(wF+g+3, wF+g+2) << "=\"01\")     else " << endl
			  << tab << tab << use("sum") << range(wF+g, 1) <<  "   when (" << use("sum") << range(wF+g+3, wF+g+1) << "=\"001\")     else " << endl
			  << tab << tab << use("sum") << range(wF+g-1, 0) << "; " << endl;

		// Exponent datapath. We have to compute 2*EA - bias + an update corresponding to the normalisatiobn
		// since (1.m)*(1.m) = xx.xxxxxx sum is xxxx.xxxxxx
		// All the following ignores overflows, infinities, zeroes, etc for the sake of simplicity.
		int bias = (1<<(wE-1))-1;
		vhdl << tab << declare("exponentUpdate", wE+1)  << " <= " << endl
			  << tab << tab << "CONV_STD_LOGIC_VECTOR(" << bias-3 << ", "<< wE+1 <<")  when " << use("sum") << "(" << wF+g+3 << ")='1'    else " << endl
			  << tab << tab << "CONV_STD_LOGIC_VECTOR(" << bias-2 << ", "<< wE+1 <<")  when (" << use("sum") << range(wF+g+3, wF+g+2) << "=\"01\")     else " << endl
			  << tab << tab << "CONV_STD_LOGIC_VECTOR(" << bias-1 << ", "<< wE+1 <<")  when (" << use("sum") << range(wF+g+3, wF+g+1) << "=\"001\")     else " << endl
			  << tab << tab << "CONV_STD_LOGIC_VECTOR(" << bias   << ", "<< wE+1 <<")  ; " << endl;
		
		vhdl << tab << declare("finalExp", wE+1)  << " <= (" << use("EA") << " & '0') - " << use("exponentUpdate") << " ; " << endl;

		nextCycle();
		
		vhdl << tab << declare("X2PY2PZ2", wE+wF+g)  << " <= " << use("finalExp") << range(wE-1,0) << " & " << use("finalFraction") << "; " << endl;


		vhdl << tab << declare("diff", wE+wF+g+1) << " <=  ('0'& " << use("X2PY2PZ2") << ")  - ('0'& " << use("R2pipe") << " & CONV_STD_LOGIC_VECTOR(0, " << g << ") );" << endl;
		vhdl << tab <<  "P <= " << use("diff") << "(" << wE+wF+g << ");"  << endl ;

	}
	
}	


Collision::~Collision()
{
}





/* We compute the acceptable values according to the FP operator
combination, using interval arithmetic, so emulate() is not stricter
when the operator is the optimized one (TODO). All it checks is that
the optimized version is at least as accurate as the FP one. */

void Collision::emulate(TestCase * tc)
{
	/* Get I/O values */
	mpz_class svX = tc->getInputValue("X");
	mpz_class svY = tc->getInputValue("Y");
	mpz_class svZ = tc->getInputValue("Z");
	mpz_class svR2 = tc->getInputValue("R2");

	/* Compute correct value */
	FPNumber fpx(wE, wF);
	fpx = svX;
	FPNumber fpy(wE, wF);
	fpy = svY;
	FPNumber fpz(wE, wF);
	fpz = svZ;
	FPNumber fpr2(wE, wF);
	fpr2 = svR2;

	mpfr_t x,y,z, r2, ru, rd;
	mpfr_init2(x,  1+wF);
	mpfr_init2(y,  1+wF);
	mpfr_init2(z,  1+wF);
	mpfr_init2(r2, 1+wF);
	mpfr_init2(ru, 1+wF);
	mpfr_init2(rd, 1+wF);


	fpr2.getMPFR(r2); 

	fpx.getMPFR(x); 
	fpy.getMPFR(y); 
	fpz.getMPFR(z);

	mpfr_mul(x,x,x, GMP_RNDU);
	mpfr_mul(y,y,y, GMP_RNDU);
	mpfr_mul(z,z,z, GMP_RNDU);
	mpfr_add(x,x,y, GMP_RNDU);
	mpfr_add(ru,x,z, GMP_RNDU); // x^2+y^2+r^2, all rounded up
 
	fpx.getMPFR(x); 
	fpy.getMPFR(y); 
	fpz.getMPFR(z);

	mpfr_mul(x,x,x, GMP_RNDD);
	mpfr_mul(y,y,y, GMP_RNDD);
	mpfr_mul(z,z,z, GMP_RNDD);
	mpfr_add(x,x,y, GMP_RNDD);
	mpfr_add(rd,x,z, GMP_RNDD); // x^2+y^2+r^2, all rounded down
 
#if 0
	mpfr_out_str (stderr, 10, 30, x, GMP_RNDN); cerr << " ";
	mpfr_out_str (stderr, 10, 30, rd, GMP_RNDN); cerr << " ";
	mpfr_out_str (stderr, 10, 30, ru, GMP_RNDN); cerr << " ";
	cerr << endl;
#endif
	mpz_class predfalse = 0;
	mpz_class predtrue =  1;

	if(mpfr_cmp(r2, rd)<0) { // R2< X^2+Y^2+Z^2
		tc->addExpectedOutput("P", predfalse);
		tc->addExpectedOutput("P", predfalse);
	}
	else if(mpfr_cmp(r2, ru)>0) {
		tc->addExpectedOutput("P", predtrue);
		tc->addExpectedOutput("P", predtrue);
	}
	else{ // grey area
		tc->addExpectedOutput("P", predtrue);
		tc->addExpectedOutput("P", predfalse);
	}

	mpfr_clears(x,y,z,r2, ru, rd, NULL);

}
 


// This method is cloned from Operator, just resetting sign and exception bits
// (because we don't have any exception support in this toy example)
// (see ***************** below )
void Collision::buildRandomTestCases(TestCaseList* tcl, int n){

	TestCase *tc;
	/* Generate test cases using random input numbers */
	for (int i = 0; i < n; i++) {
		tc = new TestCase(this); // TODO free all this memory when exiting TestBench
		/* Fill inputs */
		for (unsigned int j = 0; j < ioList_.size(); j++) {
			Signal* s = ioList_[j]; 
			if (s->type() == Signal::in) {
				// ****** Modification: positive normal numbers with small exponents 
				mpz_class m = getLargeRandom(wF);
				mpz_class bias = (mpz_class(1)<<(wE-1)) - 1; 
				mpz_class e = getLargeRandom(wE-2) - (mpz_class(1)<<(wE-3)) + bias; // ensure no overflow
				mpz_class a = (mpz_class(1)<<(wE+wF+1)) // 01 to denote a normal number
					+ (e<<wF) + m;
				tc->addInput(s->getName(), a);
			}
		}
		/* Get correct outputs */
		emulate(tc);

		//		cout << tc->getInputVHDL();
		//    cout << tc->getExpectedOutputVHDL();


		// add to the test case list
		tcl->add(tc);
	}
}