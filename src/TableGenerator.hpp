#ifndef TableGenerator_HPP
#define TableGenerator_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>

#include "Operator.hpp"
#include "HOTBM/sollya.h"	// Do NOT use libsollya from user's environment

#include "HOTBM/Function.hh"
#include "HOTBM/MPPolynomial.hh"
#include "UtilSollya.hh"


namespace flopoco{

	/** The TableGenerator class.  */
	class TableGenerator : public Operator{

		public:

			 /* TODO: Doxygen parameters*/ 
			TableGenerator(Target* target, string func, int wIn, int wOut, int n,double xmin, double xmax, double scale);

			/**
			 * TableGenerator destructor
			 */
			~TableGenerator();
			
			MPPolynomial* getMPPolynomial(sollya_node_t t);

		protected:
			int wIn_;   /**< TODO: Description*/ 
			int wOut_;  /**< TODO: Description*/
			Function &f;
	};
}
#endif
