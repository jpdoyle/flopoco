/*
 * LNS square root operator
 *
 * Author : Sylvain Collange
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

#include "LNSSqrt.hpp"
#include "../utils.hpp"
#include <cmath>

using namespace std;

namespace flopoco{

	LNSSqrt::LNSSqrt(Target * target, int wE, int wF) :
		wE(wE), wF(wF)
	{
		addInput ("nA", wE + wF + 3);
		addInput ("nB", wE + wF + 3);

		addOutput("nR", wE + wF + 3);
	
		setOperatorName();
	setCombinatorial();	

	}

	LNSSqrt::~LNSSqrt()
	{
	}

	void LNSSqrt::setOperatorName(){
		ostringstream name;
		/* The name has the format: LNSSqrt_wE_wF where: 
			wE = width of the integral part of the exponent
			wF = width of the fractional part of the exponent */
		name << "LNSSqrt_" << wE << "_" << wF; 
		uniqueName_ = name.str(); 
	}

	void LNSSqrt::outputVHDL(std::ostream& o, std::string name)
	{
		licence(o,"Jérémie Detrey, Florent de Dinechin (2003-2004), Sylvain Collange (2008)");
		Operator::stdLibs(o);
		outputVHDLEntity(o);
		newArchitecture(o,name);

		o
			<< tab << "constant wE : positive := " << wE <<";\n"
			<< tab << "constant wF : positive := " << wF <<";\n"
			<< tab << "\n"
			<< tab << "signal sRn : std_logic;\n"
			<< tab << "signal eRn : std_logic_vector(wE+wF-1 downto 0);\n"
			<< tab << "signal xRn : std_logic_vector(1 downto 0);\n"
			<< tab << "signal nRn : std_logic_vector(wE+wF+2 downto 0);\n"
			<< tab << "signal nRx : std_logic_vector(wE+wF+2 downto 0);\n"
			<< tab << "\n"
			<< tab << "signal xsA  : std_logic_vector(2 downto 0);\n";
	
		beginArchitecture(o);

		o
			<< tab << "eRn <= nA(wE+wF-1) & nA(wE+wF-1 downto 1);\n"
			<< tab << "\n"
			<< tab << "sRn <= nA(wE+wF);\n"
			<< tab << "xRn <= \"01\";\n"
			<< tab << "nRn <= xRn & sRn & eRn;\n"
			<< tab << "\n"
			<< tab << "xsA <= nA(wE+wF+2 downto wE+wF);\n"
			<< tab << "\n"
			<< tab << "with xsA select\n"
			<< tab << "nR(wE+wF+2 downto wE+wF+1) <= xsA(2 downto 1) when \"001\" | \"000\" | \"010\" | \"100\",\n"
			<< tab << "	                                    \"11\"            when others;\n"
			<< tab << "\n"
			<< tab << "nR(wE+wF downto 0) <= nRn(wE+wF downto 0);\n"
			<< "end architecture;\n";

	}
}
