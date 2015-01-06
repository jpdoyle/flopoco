#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "FixIIR.hpp"

#include "sollya.h"

#include "ShiftReg.hpp"
#include "FixSOPC.hpp"

#include "ConstMult/FixRealKCM.hpp"


using namespace std;

namespace flopoco {

	FixIIR::FixIIR(Target* target, int p_,int leadingBit_, int H_, vector<string> coeffb_, vector<string> coeffa_, bool useBitheap_, map<string, double> inputDelays) : 
		Operator(target), p(p_),leadingBit(leadingBit_), H(H_), coeffb(coeffb_), coeffa(coeffa_), useBitheap(useBitheap_)
	{
		srcFileName="FixIIR";
		setCopyrightString ( "Louis Beseme, Florent de Dinechin (2014)" );
		useNumericStd_Unsigned();

		ostringstream name;
		name << "FixIIR_"<< p << "_uid" << getNewUId();
		setNameWithFreq( name.str() );

		n = coeffb.size();
		m = coeffa.size();



		//manage the critical path
		setCriticalPath(getMaxInputDelays(inputDelays));

		addInput("X", 1+p, true);



		// guard bits for a faithful result
		int g= intlog2(2*H*(n+m)); 
		REPORT(INFO, "g=" << g);


		for (int i=0; i< n; i++)
		{
			// parse the coeffs from the string, with Sollya parsing
			sollya_obj_t node;
			mpfr_t mpC;
			
			node = sollya_lib_parse_string(coeffb[i].c_str());
			// If conversion did not succeed (i.e. parse error)
			if(node == 0)
			{
				ostringstream error;
				error << srcFileName << ": Unable to parse string " << coeffb[i] << " as a numeric constant" << endl;
				throw error.str();
			}

			mpfr_init2(mpC, 10000);
			// setToolPrecision(10000);
			// evaluateConstantExpression(mpC, node,  getToolPrecision());
			sollya_lib_get_constant(mpC, node);
			
			mpfr_init_set(mpcoeffb[i], mpC, GMP_RNDN);
			if(mpfr_get_d(mpcoeffb[i], GMP_RNDN) < 0)
				coeffsignb[i] = 1;
			else
				coeffsignb[i] = 0;
			
			mpfr_abs(mpcoeffb[i], mpcoeffb[i], GMP_RNDN);
				
		}

		for (int i=0; i< m; i++)
		{
			// parse the coeffs from the string, with Sollya parsing
			sollya_obj_t node;
			mpfr_t mpC;
			
			node = sollya_lib_parse_string(coeffa[i].c_str());
			// If conversion did not succeed (i.e. parse error)
			if(node == 0)
			{
				ostringstream error;
				error << srcFileName << ": Unable to parse string " << coeffa[i] << " as a numeric constant" << endl;
				throw error.str();
			}

			mpfr_init2(mpC, 10000);
			// setToolPrecision(10000);
			// evaluateConstantExpression(mpC, node,  getToolPrecision());
			sollya_lib_get_constant(mpC, node);
			
			mpfr_init_set(mpcoeffa[i], mpC, GMP_RNDN);
			if(mpfr_get_d(mpcoeffa[i], GMP_RNDN) < 0)
				coeffsigna[i] = 1;
			else
				coeffsigna[i] = 0;
			
			mpfr_abs(mpcoeffa[i], mpcoeffa[i], GMP_RNDN);
				
			// Accumulate the absolute values
			// mpfr_add(sumAbsCoeffB, sumAbsCoeffB, mpcoeffb[i], GMP_RNDU);
		}
		
		// now sumAbsCoeff is the max value that the filter can take.
		// double sumAbsB = mpfr_get_d(sumAbsCoeffB, GMP_RNDU); // just to make the following loop easier
		// int leadingBit=0;
		// while(sumAbsB>=2.0)
		// {
		// 	sumAbsB*=0.5;
		// 	leadingBit++;
		// }
		// while(sumAbsB<1.0)
		// {
		// 	sumAbsB*=2.0;
		// 	leadingBit--;
		// }
		REPORT(INFO, "Worst-case weight of MSB of the result is " << leadingBit);

		wO = 1+ (leadingBit - (-p)) + 1; //1 + sign  ; 

		

		int size = 1+ (leadingBit - (-p) +1) + g ; // sign + overflow  bits on the left, guard bits on the right
		REPORT(INFO, "Sum size is: "<< size );

		
		//compute the guard bits from the KCM mulipliers
		int wInKCM_B = 1 + p;	//1 sign bit + p bit 
		int lsbOutKCM = -p-g;
		double targetUlpError = 1.0;
		int guardBitsKCM_B = FixRealKCM::neededGuardBits(target, wInKCM_B, lsbOutKCM, targetUlpError);

		int wInKCM_A = 1+1+leadingBit+p+g;
		int guardBitsKCM_A = FixRealKCM::neededGuardBits(target, wInKCM_A, lsbOutKCM, targetUlpError);

		int guardBitsKCM = max(guardBitsKCM_A, guardBitsKCM_B);

		// size += guardBitsKCM; // sign + overflow  bits on the left, guard bits + guard bits from KCMs on the right
		REPORT(INFO, "Sum size with KCM guard bits is: "<< size);


		//Pour info
		REPORT(INFO, "guardBitsKCM part B = "<< guardBitsKCM_B);
		REPORT(INFO, "guardBitsKCM part A = "<< guardBitsKCM_A);

		//Shift register for the left part
		ShiftReg *shiftRegB = new ShiftReg(target, 1+p, n, inputDelays);

		addSubComponent(shiftRegB);
		inPortMap(shiftRegB, "X", "X");

		for(int i = 0; i<n; i++) {
			outPortMap(shiftRegB, join("Xd", i), join("Yb", i));
		}

		vhdl << instance(shiftRegB, "shiftRegB");

		
		//Shift register for the right part
		setCycleFromSignal("X");

		ShiftReg *shiftRegA = new ShiftReg(target, wInKCM_A, m, inputDelays);

		addSubComponent(shiftRegA);
		inPortMap(shiftRegA, "X", declare("Rtmp", wO+g));

		for(int i = 0; i<m; i++) {
			outPortMap(shiftRegA, join("Xd", i), join("Ya", i));
		}

		vhdl << instance(shiftRegA, "shiftRegA");

		
		if (useBitheap)
		{
			//create the bitheap that computes the sum
			bitHeap = new BitHeap(this, size+guardBitsKCM);

			for (int i=0; i<n; i++) 
			{
				// Multiplication: instantiating a KCM object. It will add bits also to the right of lsbOutKCM
				FixRealKCM* mult = new FixRealKCM(this,				// the envelopping operator
														target, 	// the target FPGA
														getSignalByName(join("Yb",i)),
														true, 		// signed
														-1, 		// input MSB, but one sign bit will be added
														-p, 		// input LSB weight
														lsbOutKCM, 		// output LSB weight -- the output MSB is computed out of the constant
														coeffb[i], 	// pass the string unmodified
														bitHeap		// pass the reference to the bitheap that will accumulate the intermediary products
													);
			}


			for (int i=0; i<m; i++) 
			{
				// Multiplication: instantiating a KCM object. It will add bits also to the right of lsbOutKCM
				FixRealKCM* mult = new FixRealKCM(this,				// the envelopping operator
														target, 	// the target FPGA
														getSignalByName(join("Ya",i)),
														true, 		// signed
														leadingBit, 		// input MSB, but one sign bit will be added
														-p-g, 		// input LSB weight
														lsbOutKCM, 		// output LSB weight -- the output MSB is computed out of the constant
														coeffa[i], 	// pass the string unmodified
														bitHeap		// pass the reference to the bitheap that will accumulate the intermediary products
													);
			}


			//rounding to p+g - add 1/2 ulps
			if (guardBitsKCM>0)
				bitHeap->addConstantOneBit(guardBitsKCM-1);

			//compress the bitheap
			bitHeap -> generateCompressorVHDL();

			setCycleFromSignal("Rtmp");
			vhdl << tab << "Rtmp" << " <= " << bitHeap-> getSumName() << range(size+guardBitsKCM-1, guardBitsKCM) << ";" << endl;
		}
		else
		{
			setCycleFromSignal("Yb0");
			vhdl << tab << declare("S0", size) << " <= " << zg(size) << ";" << endl;

			for (int i=0; i<n; i++)
			{
				//manage the critical path
				setCycleFromSignal(join("Yb",i));

				// Multiplication: instantiating a KCM object. 
				FixRealKCM* mult = new FixRealKCM(target, 
												  true, // signed
												  -1, // input MSB, but one sign bit will be added
												  -p, // input LSB weight
												  lsbOutKCM, // output LSB weight -- the output MSB is computed out of the constant
												  coeffb[i] // pass the string unmodified
												  );
				addSubComponent(mult);
				inPortMap(mult,"X", join("Yb", i));
				outPortMap(mult, "R", join("Pb", i));
				vhdl << instance(mult, join("mult", i));

				//manage the critical path
				syncCycleFromSignal(join("Pb", i));
				syncCycleFromSignal(join("S", i));
				manageCriticalPath(target->adderDelay(size));

				// Addition
				int pSize = getSignalByName(join("Pb", i))->width();
				vhdl << tab << declare(join("S", i+1), size) << " <= " <<  join("S",i);
				if(coeffsignb[i] == 1)
					vhdl << " - (" ;
				else
					vhdl << " + (" ;
				if(size>pSize) 
					vhdl << "("<< size-1 << " downto " << pSize<< " => "<< join("Pb",i) << of(pSize-1) << ")" << " & " ;
				vhdl << join("Pb", i) << ");" << endl;
			}

			for (int i=0; i<m; i++)
			{
				//manage the critical path
				setCycleFromSignal(join("Ya",i));

				// Multiplication: instantiating a KCM object. 
				FixRealKCM* mult = new FixRealKCM(target, 
												  true, // signed
												  leadingBit, // input MSB, but one sign bit will be added
												  -p-g, // input LSB weight
												  lsbOutKCM, // output LSB weight -- the output MSB is computed out of the constant
												  coeffa[i] // pass the string unmodified
												  );
				addSubComponent(mult);
				inPortMap(mult,"X", join("Ya", i));
				outPortMap(mult, "R", join("Pa", i));
				vhdl << instance(mult, join("mult", n+i));

				//manage the critical path
				syncCycleFromSignal(join("Pa", i));
				syncCycleFromSignal(join("S", n+i));
				manageCriticalPath(target->adderDelay(size));

				// Addition
				int pSize = getSignalByName(join("Pa", i))->width();
				vhdl << tab << declare(join("S", n+i+1), size) << " <= " <<  join("S",n+i);
				if(coeffsigna[i] == 1)
					vhdl << " - (" ;
				else
					vhdl << " + (" ;
				if(size>pSize) 
					vhdl << "("<< size-1 << " downto " << pSize<< " => "<< join("Pa",i) << of(pSize-1) << ")" << " & " ;
				vhdl << join("Pa", i) << ");" << endl;
			}

			//manage the critical path
			syncCycleFromSignal(join("S", n+m));
			manageCriticalPath(target->adderDelay(wO+1));

			//Adding one half ulp to obtain correct rounding
			// vhdl << tab << declare("R_mid", size+1) << " <= " <<  join("S", n+m) << range(size-1, size-wO-1) << " + (" << zg(size) << " & \'1\');" << endl;
			// vhdl << tab << "Rtmp <= " <<  "R_mid" << range(size, 1) << ";" << endl;
			setCycleFromSignal("Rtmp");
			vhdl << tab << "Rtmp <= " << join("S", n+m) << ";" << endl;

		}

		addOutput("R", wO, 2); 

		//Adding one half ulp to obtain correct rounding
		vhdl << tab << declare("R_int", wO+1) << " <= " <<  "Rtmp" << range(size-1, g - 1) << " + (" << zg(wO) << " & \'1\');" << endl;
		vhdl << tab << "R <= " <<  "R_int" << range(wO, 1) << ";" << endl;
		



	};

	FixIIR::~FixIIR(){

	};


	void FixIIR::emulate(TestCase * tc){};

	void FixIIR::buildStandardTestCases(TestCaseList* tcl){};

}