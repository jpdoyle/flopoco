#ifndef CORDICSINCOS_HPP
#define CORDICSINCOS_HPP

#include "../Operator.hpp"
#include "../utils.hpp"
#include "../IntMultiplier.hpp"
#include "../IntAdder.hpp"

#include "CordicSinCosClassic.hpp"
#include "CordicSinCosRedIter.hpp"

namespace flopoco{ 


	class CordicSinCos : public Operator {
	  
	  public:
		int w, reducedIterations;
		gmp_randstate_t state;
	  
		// constructor, defined there with two parameters (default value 0 for each)
		CordicSinCos(Target* target, int w, int reducedIterations = 0, map<string, double> inputDelays = emptyDelayMap);

		// destructor
		~CordicSinCos() {};
		
		void changeName(std::string operatorName);


		// Below all the functions needed to test the operator
		/* the emulate function is used to simulate in software the operator
		  in order to compare this result with those outputed by the vhdl opertator */
		void emulate(TestCase * tc);

		/* function used to create Standard testCase defined by the developper */
		void buildStandardTestCases(TestCaseList* tcl);
	};

}

#endif
