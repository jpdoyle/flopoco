#ifndef IntMultiplierS_HPP
#define IntMultiplierS_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "Table.hpp"
#include "BitHeap.hpp"

namespace flopoco {


		


/** The IntMultiplier class, getting rid of Bogdan's mess.
*/
class IntMultiplier : public Operator {
public:
		/** An elementary LUT-based multiplier, written as a table so that synthesis tools don't infer DSP blocks for it*/
		class SmallMultTable: public Table {
		public:
			int dx, dy, wO;
			bool signedIO;
			SmallMultTable(Target* target, int dx, int dy, int wO, bool signedIO=false );
			mpz_class function(int x);
		};

    /**
    * The IntMultiplier constructor
    * @param[in] target           the target device
    * @param[in] wX             X multiplier size (including sign bit if any)
    * @param[in] wY             Y multiplier size (including sign bit if any)
    * @param[in] wOut         wOut size for a truncated multiplier (0 means full multiplier)
    * @param[in] signedIO     false=unsigned, true=signed
    * @param[in] ratio            DSP block use ratio
    **/
	IntMultiplier(Target* target, int wX, int wY, int wOut=0, bool signedIO = false, float ratio = 1.0, map<string, double> inputDelays = emptyDelayMap);


    /**
    * The virtual IntMultiplier constructor adds all the multiplier bits to some bitHeap, but no more.
    * @param[in] parentOp      the Operator to which VHDL code will be added
    * @param[in] bitHeap       the BitHeap to which bits will be added
    * @param[in] x            a Signal from which the x input should be taken
    * @param[in] y            a Signal from which the y input should be taken
    * @param[in] wX             X multiplier size (including sign bit if any)
    * @param[in] wY             Y multiplier size (including sign bit if any)
    * @param[in] wOut         wOut size for a truncated multiplier (0 means full multiplier)
    * @param[in] lsbWeight     the weight, within this BitHeap, corresponding to the LSB of the multiplier output. Note that there should be enough bits below for guard bits in case of truncation.
    * @param[in] signedIO     false=unsigned, true=signed
    * @param[in] ratio            DSP block use ratio
    **/
	IntMultiplier (Operator* parentOp, BitHeap* bitHeap,  Signal* x, Signal* y, int wX, int wY, int wOut, int lsbWeight, bool signedIO, float ratio);


    /**
    *  Destructor
    */
    ~IntMultiplier();

    /**
    * The emulate function.
    * @param[in] tc               a list of test-cases
    */
    void emulate ( TestCase* tc );

		void buildStandardTestCases(TestCaseList* tcl);



protected:
	
	string PP(int i, int j, int nr=-1);
	string PPTbl( int i, int j, int nr=-1);
	string XY(int i, int j, int nr=-1);
	string heap( int i, int j);

	void buildLogicOnly();
	void buildTiling();

	void manageSignBeforeMult();            /*< to be called before buildHeap* **/
	//void buildHeap();                      /*< Checks special size cases, then calls either buildHeapLogicOnly or buildHeapTiling **/
	void buildHeapLogicOnly();
	void buildHeapTiling();
	void manageSignAfterMult();            /*< to be called after either buildHeapLogicOnly or buildHeapTiling **/
	void smallTiling(int nr, int topX, int topY, int botX, int botY, SmallMultTable *t);
	int wxDSP, wyDSP;               /**< the width for X/Y in DSP*/
	int wXdecl;                     /**< the width for X as declared*/
	int wYdecl;                     /**< the width for Y  as declared*/
	int wX;                         /**< the width for X after possible swap such that wX>wY */
	int wY;                         /**< the width for Y after possible swap such that wX>wY */
	int wOut;
	int wFull;                      /**< size of the full product: wX+wY-1 if signed, wX+wY if unsigned */
	int wTruncated;                 /**< The number of truncated bits, wFull - wOut*/
	int g ;                         /**< the number of guard bits*/
	int maxWeight;                  /**< The max weight for the bit heap of this multiplier, wOut + g*/
	int weightShift;                /**< the shift in weight for a truncated multiplier compared to a full one,  wFull - maxWeight*/
	int signedIO;
	double ratio;
	double maxError;     /**< the max absolute value error of this multiplier, in ulps of the result. Should be 0 for untruncated, 1 or a bit less for truncated.*/  
	double initialCP;     /**< the initial delay, getMaxInputDelays ( inputDelays_ ).*/  
	
private:
	bool useDSP;
	Operator* parentOp;  /**<  For a virtual multiplier, adding bits to some BitHeap, this is a pointer to the Operator that will provide the actual vhdl stream etc. */
	BitHeap* bitHeap;    /**<  The heap of weighted bits that will be used to do the additions */
	Signal* x;
	Signal* y; 
	string xname;
	string yname;

	void initialize();   /**< initialization stuff common to both constructors*/
};

}
#endif
