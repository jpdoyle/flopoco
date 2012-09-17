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
#include "Plotter.hpp"

#include "IntMultipliers/MultiplierBlock.hpp"

namespace flopoco {

	/*
	  Definition of the DSP use threshold r:
	  Consider a submultiplier block, by definition smaller than (or equal to) a DSP in both dimensions
	  So: r=1 means any multiplier that does not fills a DSP goes to logic
	      r=0       any multiplier, even very small ones, go to DSP

	   (submultiplier area)/(DSP area) is between 0 and 1
	  if (submultiplier area)/(DSP area) is larger than r then use a DSP for it 
	 */

		


	/** The IntMultiplier class, getting rid of Bogdan's mess.
	 */
	class IntMultiplier : public Operator {
	
	public:
		/** An elementary LUT-based multiplier, written as a table so that synthesis tools don't infer DSP blocks for it*/
		class SmallMultTable: public Table {
		public:
			int dx, dy, wO;
			bool negate, signedX, signedY;
			SmallMultTable(Target* target, int dx, int dy, int wO, bool negate=false, bool signedX=false, bool signedY=false );
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
		IntMultiplier(Target* target, int wX, int wY, int wOut=0, bool signedIO = false, float ratio = 1.0, map<string, double> inputDelays = emptyDelayMap,bool enableSuperTiles=true);


		/**
		 * The virtual IntMultiplier constructor adds all the multiplier bits to some bitHeap, but no more.
		 * @param[in] parentOp      the Operator to which VHDL code will be added
		 * @param[in] bitHeap       the BitHeap to which bits will be added
		 * @param[in] x            a Signal from which the x input should be taken
		 * @param[in] y            a Signal from which the y input should be taken
		 * @param[in] wX             X multiplier size (including sign bit if any)
		 * @param[in] wY             Y multiplier size (including sign bit if any)
		 * @param[in] wOut         wOut size for a truncated multiplier (0 means full multiplier)
		 * @param[in] lsbWeight     the weight, within this BitHeap, corresponding to the LSB of the multiplier output. 
		 *                          Note that there should be enough bits below for guard bits in case of truncation.
		                            The method neededGuardBits() provides this information.
		 * @param[in] negate     if true, the multiplier result is subtracted from the bit heap 
		 * @param[in] signedIO     false=unsigned, true=signed
		 * @param[in] ratio            DSP block use ratio
		 **/
		IntMultiplier (Operator* parentOp, BitHeap* bitHeap,  Signal* x, Signal* y, int wX, int wY, int wOut, int lsbWeight, bool negate, bool signedIO, float ratio);

		/** How many guard bits will a truncated multiplier need? Needed to set up the BitHeap of an operator using the virtual constructor */
		static int neededGuardBits(int wX, int wY, int wOut);


		/**
		 *  Destructor
		 */
		~IntMultiplier();

		/**
		 * The emulate function.
		 * @param[in] tc               a test-case
		 */
		void emulate ( TestCase* tc );

		void buildStandardTestCases(TestCaseList* tcl);

		



	protected:
		// add a unique identifier for the multiplier, and possibly for the block inside the multiplier
		string addUID(string name, int blockUID=-1);


		string PP(int i, int j, int uid=-1);
		string PPTbl( int i, int j, int uid=-1);
		string XY(int i, int j, int uid=-1);


		/** Fill the bit heap with all the contributions from this multiplier */
		void fillBitHeap();


		void buildLogicOnly();
		void buildTiling();

	
		
		

	
		/**	builds the logic block ( smallMultTables) 
		 *@param topX, topY -top right coordinates 
		 *@param botX, botY -bottom left coordinates 
		 *@param uid is just a number which helps to form the signal names (for multiple calling of the method
		 )	*/
		void buildHeapLogicOnly(int topX, int topY, int botX, int botY, int uid=-1);

		/**	builds the heap using DSP blocks) 
		 */
		void buildHeapTiling();
		
		void buildAlteraTiling(int blockTopX, int blockTopY, int blockBottomX, int blockBottomY, int dspIndex);

		void buildFancyTiling();
		

		/** is called when no more dsp-s fit in a row, because of the truncation line
		  *	checks the ratio, if only DSPs should be used, only logic, or maybe both, and applies it **/
		bool checkThreshold(int topX, int topY, int botX, int botY,int wxDSP,int wyDSP);
		void addExtraDSPs(int topX, int topY, int botX, int botY, int wxDSP, int wyDSP);
		int checkTiling(int wxDSP, int wyDSP, int& horDSP, int& verDSP);




		int wxDSP, wyDSP;               /**< the width for X/Y in DSP*/
		int wXdecl;                     /**< the width for X as declared*/
		int wYdecl;                     /**< the width for Y  as declared*/
		int wX;                         /**< the width for X after possible swap such that wX>wY */
		int wY;                         /**< the width for Y after possible swap such that wX>wY */
		int wOut;						/**<the size of the output*/
		int wFull;                      /**< size of the full product: wX+wY-1 if signed, wX+wY if unsigned */
		int wTruncated;                 /**< The number of truncated bits, wFull - wOut*/
		int g ;                         /**< the number of guard bits */
		int maxWeight;                  /**< The max weight for the bit heap of this multiplier, wOut + g*/
 		int weightShift;                /**< the shift in weight for a truncated multiplier compared to a full one,  wFull - maxWeight*/
		double ratio;
		double maxError;     /**< the max absolute value error of this multiplier, in ulps of the result. Should be 0 for untruncated, 1 or a bit less for truncated.*/  
		double initialCP;     /**< the initial delay, getMaxInputDelays ( inputDelays_ ).*/  
	private:
		bool useDSP;
		Operator* parentOp;  /**< For a virtual multiplier, adding bits to some external BitHeap, 
							   this is a pointer to the Operator that will provide the actual vhdl stream etc. */
		BitHeap* bitHeap;    /**< The heap of weighted bits that will be used to do the additions */
		int lsbWeight;       /**< the weight in the bit heap of the lsb of the multiplier result ; equals g for standalone multipliers */
		Plotter* plotter;
		// TODO the three following variable pairs seem uglily redundant
		Signal* x;
		Signal* y; 
		bool enableSuperTiles;//if supertiles are needed
		string xname;
		string yname;
		string inputName1;
		string inputName2;
		bool isOperator;                /**< if true this multiplier is a stand-alone operator, if false it just contributes to some BitHeap of another Operator */
		bool negate;                    /**< if true this multiplier checkThresholds -xy */
		int signedIO;                   /**< true if the IOs are two's complement */
		int multiplierUid;
		void initialize();   /**< initialization stuff common to both constructors*/
		vector<MultiplierBlock*> localSplitVector;		
		//vector<DSP*> dsps;
		//ofstream fig;

	
	};

}
#endif
