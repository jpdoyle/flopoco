#ifndef Fix2FP_HPP
#define Fix2FP_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "FPNumber.hpp"
#include "LZOCShifterSticky.hpp"
#include "LZOC.hpp"
#include "IntAdder.hpp"



/** The FPAdder class */
class Fix2FP : public Operator
{
public:
	/**
	 * The FPAdder constructor
	 * @param[in]		target		the target device
	 * @param[in]		MSB			the MSB of the input number
	 * @param[in]		LSB			the LSB of the input number
	 * @param[in]		wER			the with of the exponent for the convertion result
	 * @param[in]		wFR			the with of the fraction for the convertion result
	 */
	Fix2FP(Target* target, int MSBI, int LSBI, int wER, int wFR);

	/**
	 * FPAdder destructor
	 */
	~Fix2FP();


	void emulate(TestCase * tc);
	void buildStandardTestCases(TestCaseList* tcl);



private:
	/** The MSB for the input */
	int MSBI; 
	/** The LSB for the input */
	int LSBI; 
	/** The width of the exponent for the output R */
	int wER; 
	/** The width of the fraction for the output R */
	int wFR;
	

	/**The leading zero counter	*/
	LZOCShifterSticky* lzocs; 
	/** The integer adder object for subtraction from the MSB the position of the leading 1, for shifting the number */
	IntAdder* fractionConvert;
	/** The integer adder object for adding 1 to the fraction part*/
	IntAdder* roundingApproximator;
	/** The integer adder object for substracting 1 from the remainder of the fraction to establish if it is zero*/
	IntAdder* oneSubstracter;
	/** The integer adder object for transforming the Count of LZO to exponent*/
	IntAdder* exponentConvertion;
	/** The integer adder object for adding the biass to the exponent*/
	IntAdder* exponentFinal;
	/** The integer adder object for zero detector of the input*/
	IntAdder* zeroD;
	/** The integer adder object for correcting the exponent*/
	IntAdder* expCorrect;
	
	
	
	
	int wF;
	int wE;
	int sizeExponentValue;
	int sizeFractionPlusOne;
	int MSB;
	int LSB;
	int inputWidth;

};

#endif