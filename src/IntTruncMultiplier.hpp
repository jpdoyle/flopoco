#ifndef IntTruncMultiplier_HPP
#define IntTruncMultiplier_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "IntAdder.hpp"
#include "IntNAdder.hpp"


namespace flopoco{
	class IntTruncMultiplier : public Operator
	{
	public:
		IntTruncMultiplier(Target* target, DSP** configuration, int wX, int wY, int k);
	
		/** IntTruncMultiplier destructor */
		~IntTruncMultiplier();

	protected:

		int wX; /**< the width (in bits) of the input  X  */
		int wY; /**< the width (in bits) of the input  Y  */
		int wt; /**< the width (in bits) of the output R  */


		void printConfiguration(DSP** configuration);
	private:

	};

}
#endif
