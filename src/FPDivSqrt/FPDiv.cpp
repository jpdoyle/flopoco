/*
  Floating Point Divider for FloPoCo


  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Authors: Maxime Christ, Jeremie Detrey, Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2015.
  All rights reserved.

 */


// TODO Radix 4 with digit set -2..2 should fit the iteration in one row of LUT+add

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"

#include "FPDiv.hpp"
#include "SelFunctionTable.hpp"

using namespace std;

namespace flopoco{

#define DEBUGVHDL 0


	FPDiv::FPDiv(Target* target, int wE, int wF, int radix) :
		Operator(target), wE(wE), wF(wF) {

		int i;
		ostringstream name;
		setCopyrightString("Maxime Christ, Florent de Dinechin (2015)");

		srcFileName="FPDiv";
		name<<"FPDiv_"<<wE<<"_"<<wF;
		setNameWithFreq(name.str());

		if(radix !=0  && radix!=4 && radix !=8) {
			THROWERROR("Got radix = " << radix << ". Only possible choices for radix are 0, 4 or 8" );
		}
		
		if(radix==0){ // 0 means "let FloPoCo choose"
			if( (target->lutInputs() <= 4) || (!target->isPipelined()))
				radix=4;
			else
				radix=8;
			REPORT(INFO, "Using radix " << radix << " SRT algorithm");
		}


	 
		if(radix==8)
		{
			int extraBit = 0;
			extraBit+=2; //Here we'll prescale by 5/4 => 2 right extra bits
			extraBit+=1; //The sticky bit
			extraBit+=1; //The result will be in [1/2, 2[ => 1 more bit (2^0)
			extraBit+=2; //To round correctly the result
			extraBit+=3; //floor() and the bits cut to get a result depending on wF instead of nDigit (cf. last step before normalization)

			nDigit = floor(((double)(wF + extraBit))/3);

			addFPInput ("X", wE, wF);
			addFPInput ("Y", wE, wF);
			addFPOutput("R", wE, wF);

			vhdl << tab << declare("partialFX",wF+1) << " <= \"1\" & X(" << wF-1 << " downto 0);" << endl; //IEEE norm, the first 1 is implicit
			vhdl << tab << declare("partialFY",wF+1) << " <= \"1\" & Y(" << wF-1 << " downto 0);" << endl;

			vhdl << tab << "-- exponent difference, sign and exception combination computed early, to have less bits to pipeline" << endl;

			vhdl << tab << declare("expR0", wE+2) << " <= (\"00\" & X(" << wE+wF-1 << " downto " << wF << ")) - (\"00\" & Y(" << wE+wF-1 << " downto " << wF<< "));" << endl;
			vhdl << tab << declare("sR") << " <= X(" << wE+wF << ") xor Y(" << wE+wF<< ");" << endl;
			vhdl << tab << "-- early exception handling " <<endl;
			vhdl << tab << declare("exnXY",4) << " <= X(" << wE+wF+2 << " downto " << wE+wF+1  << ") & Y(" << wE+wF+2 << " downto " << wE+wF+1 << ");" <<endl;
			vhdl << tab << "with exnXY select" <<endl;
			vhdl << tab << tab << declare("exnR0", 2) << " <= " << endl;
			vhdl << tab << tab << tab << "\"01\"  when \"0101\",                   -- normal" <<endl;
			vhdl << tab << tab << tab << "\"00\"  when \"0001\" | \"0010\" | \"0110\", -- zero" <<endl;
			vhdl << tab << tab << tab << "\"10\"  when \"0100\" | \"1000\" | \"1001\", -- overflow" <<endl;
			vhdl << tab << tab << tab << "\"11\"  when others;                   -- NaN" <<endl;



			/////////////////////////////////////////////////////////////////////////Prescaling
			//TODO : maybe we can reduce fX and fY
			setCriticalPath(0);
			manageCriticalPath(target->adderDelay(wF+3));
			vhdl << tab << " -- Prescaling" << endl;
			vhdl << tab << "with partialFY " << range(wF-1, wF-2) << " select" << endl;
			vhdl << tab << tab << declare("fY", wF+3) << " <= " << endl; //potentially *5/4 => we need 2 more bits
			vhdl << tab << tab << tab << "(\"0\" & partialFY & \"0\") + (partialFY & \"00\") when \"00\","<<endl; /////////[1/2, 5/8[ * 3/2 => [3/4, 15/16[
			vhdl << tab << tab << tab << "(\"00\" & partialFY) + (partialFY & \"00\") when \"01\","<<endl; ////////////////[5/8, 3/4[ * 5/4 => [25/32, 15/16[
			vhdl << tab << tab << tab << "partialFY &\"00\" when others;"<<endl; /////////no prescaling

			vhdl << tab << "with partialFY " << range(wF-1, wF-2) << " select" << endl;
			vhdl << tab << tab << declare("fX", wF+4) << " <= " << endl;
			vhdl << tab << tab << tab << "(\"00\" & partialFX & \"0\") + (\"0\" & partialFX & \"00\") when \"00\","<<endl; /////////[1/2, 5/[*3/2 => [3/4, 15/16[
			vhdl << tab << tab << tab << "(\"000\" & partialFX) + (\"0\" & partialFX & \"00\") when \"01\","<<endl; ////////////////[5/8, 3/4[*5/4 => [25/32, 15/16[
			vhdl << tab << tab << tab << "\"0\" & partialFX &\"00\" when others;"<<endl; /////////no prescaling

			ostringstream wInit;
			wInit << "w" << nDigit-1;
			vhdl << tab << declare(wInit.str(), wF+6) << " <=  \"00\" & fX;" << endl; //TODO : review that


			double srt8stepdelay =  3*target->lutDelay() + 2*target->localWireDelay()+ target->localWireDelay(2*wF+14) + target->adderDelay(wF+7);

			SelFunctionTable* table;
			table = new SelFunctionTable(target, 0.75, 1.0, 2, 5, 7, 8, 7, 4);
			addSubComponent(table);

			for(i=nDigit-1; i>=1; i--) {
				manageCriticalPath(srt8stepdelay);

				ostringstream wi, qi, wim1, seli, wipad, wim1full, wim1fulla, tInstance;
				wi << "w" << i;						//actual partial remainder
				qi << "q" << i;						//actual quotient digit, LUT's output
				wim1 << "w" << i-1;					//partial remainder for the next iteration, = left shifted wim1full
				seli << "sel" << i;					//constructed as the wi's first 4 digits and D's first, LUT's input
				wipad << "w" << i << "pad";			//1-left-shifted wi
				wim1full << "w" << i-1 << "full";	//partial remainder after this iteration, = wi+qi*D
				wim1fulla << "w" << i-1 << "fulla";	//partial remainder after this iteration, = wi+qi*D
				tInstance << "SelFunctionTable" << i;

				vhdl << tab << declare(seli.str(),7) << " <= " << wi.str() << range( wF+5, wF+1)<<" & fY"<< range(wF, wF-1) <<";" << endl;
				inPortMap (table , "X", seli.str());
				outPortMap(table , "Y", qi.str());
				vhdl << instance(table , tInstance.str());

				vhdl << tab << declare(wipad.str(), wF+7) << " <= " << wi.str() << " & '0';" << endl;

				vhdl << tab << "with " << qi.str() << range(1,0) << " select " << endl;
				vhdl << tab << declare(wim1fulla.str(), wF+7) << " <= " << endl;
				vhdl << tab << tab << wipad.str() << " - (\"0000\" & fY)			when \"01\"," << endl;
				vhdl << tab << tab << wipad.str() << " + (\"0000\" & fY)			when \"11\"," << endl;
				vhdl << tab << tab << wipad.str() << " + (\"000\" & fY & \"0\")	  when \"10\"," << endl;
				vhdl << tab << tab << wipad.str() << " 			   		  when others;" << endl;
#if 0 // Splitting the following logic into two levels gives better synthesis results...
				vhdl << tab << "with " << qi.str() << range(3,1) << " select " << endl;
				vhdl << tab << declare(wim1full.str(), wF+7) << " <= " << endl;
				vhdl << tab << tab << wim1fulla.str() << " - (\"00\" & fY & \"00\")			when \"001\" | \"010\"," << endl;
				vhdl << tab << tab << wim1fulla.str() << " - (\"0\" & fY & \"000\")			when \"011\"," << endl;
				vhdl << tab << tab << wim1fulla.str() << " + (\"00\" & fY & \"00\")			when \"110\" | \"101\"," << endl;
				vhdl << tab << tab << wim1fulla.str() << " + (\"0\" & fY & \"000\")			when \"100\"," << endl;
				vhdl << tab << tab << wim1fulla.str() << " 			   		  when others;" << endl;
#else
				ostringstream fYdec;
				fYdec << "fYdec" << i-1;	//
				vhdl << tab << "with " << qi.str() << range(3,1) << " select " << endl;
				vhdl << tab << declare(fYdec.str(), wF+7) << " <= " << endl;
				vhdl << tab << tab << "(\"00\" & fY & \"00\")			when \"001\" | \"010\" | \"110\"| \"101\"," << endl;
				vhdl << tab << tab << "(\"0\" & fY & \"000\")			when \"011\"| \"100\"," << endl;
				vhdl << tab << tab << rangeAssign(wF+6,0,"'0'") << "when others;" << endl;

				vhdl << tab << "with " << qi.str() << of(3) << " select" << endl; // Remark here: seli(6)==qi(3) but it we get better results using the latter.
				vhdl << tab << declare(wim1full.str(), wF+7) << " <= " << endl;
				vhdl << tab << tab << wim1fulla.str() << " - " << fYdec.str() << "			when '0'," << endl;
				vhdl << tab << tab << wim1fulla.str() << " + " << fYdec.str() << "			when others;" << endl;

#endif
				vhdl << tab << declare(wim1.str(),wF+6) << " <= " << wim1full.str()<<range(wF+3,0)<<" & \"00\";" << endl;
			}

			manageCriticalPath(srt8stepdelay);

			vhdl << tab << declare("q0",4) << "(3 downto 0) <= \"0000\" when  w0 = (" << wF+5 << " downto 0 => '0')" << endl;
			vhdl << tab << "             else w0(" << wF+5 << ") & \"010\";" << endl;

			for(i=nDigit-1; i>=1; i--) {
				ostringstream qi, qPi, qMi;
				qi << "q" << i;
				qPi << "qP" << i;
				qMi << "qM" << i;
				vhdl << tab << declare(qPi.str(), 3) <<" <=      " << qi.str() << "(2 downto 0);" << endl;
				vhdl << tab << declare(qMi.str(), 3)<<" <=      " << qi.str() << "(3) & \"00\";" << endl;
			}

			vhdl << tab << declare("qP0", 3) << " <= q0(2 downto 0);" << endl;
			vhdl << tab << declare("qM0", 3) << " <= q0(3)  & \"00\";" << endl;

			vhdl << tab << declare("qP", 3*nDigit) << " <= qP" << nDigit-1;
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qP" << i;
			vhdl << ";" << endl;

			vhdl << tab << declare("qM", 3*nDigit) << " <= qM" << nDigit-1 << range(1,0);
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qM" << i;
			vhdl << " & \"0\";" << endl;

			vhdl << tab << declare("fR0", 3*nDigit) << " <= qP - qM;" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////

			//The last +3 in computing nDigit is for this part
			vhdl << tab << declare("fR", wF+6) << " <= ";
			if (wF % 3 == 1) //2 extra bit
				vhdl << "fR0(" << 3*nDigit-1 << " downto 3) & (fR0(0) or fR0(1) or fR0(2)); " << endl;

			else if (wF % 3 == 2)// 1 extra bits
				vhdl << "fR0(" << 3*nDigit-1 << " downto 2) & (fR0(0) or fR0(1)); " << endl;

			else // 3 extra bit
				vhdl << "fR0(" << 3*nDigit-1 << " downto 4) & (fR0(0) or fR0(1) or fR0(2) or fR0(3)); " << endl;


			vhdl << tab << "-- normalisation" << endl;
			vhdl << tab << "with fR(" << wF+4 << ") select" << endl;

			vhdl << tab << tab << declare("fRn1", wF+4) << " <= fR(" << wF+4 << " downto 2) & (fR(0) or fR(1)) when '1'," << endl;
			vhdl << tab << tab << "        fR(" << wF+3 << " downto 0)          when others;" << endl;

			vhdl << tab << declare("expR1", wE+2) << " <= expR0"
				 << " + (\"000\" & (" << wE-2 << " downto 1 => '1') & fR(" << wF+4 << ")); -- add back bias" << endl;


			vhdl << tab << declare("round") << " <= fRn1(2) and (fRn1(0) or fRn1(1) or fRn1(3)); -- fRn1(0) is the sticky bit" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////
			vhdl << tab << "-- final rounding" <<endl;
			vhdl << tab <<  declare("expfrac", wE+wF+2) << " <= "
				 << "expR1 & fRn1(" << wF+2 << " downto 3) ;" << endl;
			vhdl << tab << declare("expfracR", wE+wF+2) << " <= "
				 << "expfrac + ((" << wE+wF+1 << " downto 1 => '0') & round);" << endl;
			vhdl << tab <<  declare("exnR", 2) << " <=      \"00\"  when expfracR(" << wE+wF+1 << ") = '1'   -- underflow" <<endl;
			vhdl << tab << "        else \"10\"  when  expfracR(" << wE+wF+1 << " downto " << wE+wF << ") =  \"01\" -- overflow" <<endl;
			vhdl << tab << "        else \"01\";      -- 00, normal case" <<endl;


			vhdl << tab << "with exnR0 select" <<endl;
			vhdl << tab << tab << declare("exnRfinal", 2) << " <= " <<endl;
			vhdl << tab << tab << tab << "exnR   when \"01\", -- normal" <<endl;
			vhdl << tab << tab << tab << "exnR0  when others;" <<endl;
			vhdl << tab << "R <= exnRfinal & sR & "
				 << "expfracR(" << wE+wF-1 << " downto 0);" <<endl;
			}


		else ////////////////////////// Radix 4 version  ////////////////////////
			//TODO : the old version is using 5-input's LUTs, try to fit in 4-input's LUTs (same as above : select qA and qB and make a 2-levels addition)
		{
			int alpha=3; // can be 3 or 2. At the moment, 2 doesn't work.
			// -------- Parameter set up -----------------
			nDigit = (wF+6) >> 1;

			addFPInput ("X", wE, wF);
			addFPInput ("Y", wE, wF);
			addFPOutput("R", wE, wF);

			vhdl << tab << declare("fX",wF+1) << " <= \"1\" & X(" << wF-1 << " downto 0);" << endl;
			vhdl << tab << declare("fY",wF+1) << " <= \"1\" & Y(" << wF-1 << " downto 0);" << endl;

			vhdl << tab << "-- exponent difference, sign and exception combination computed early, to have less bits to pipeline" << endl;

			vhdl << tab << declare("expR0", wE+2) << " <= (\"00\" & X(" << wE+wF-1 << " downto " << wF << ")) - (\"00\" & Y(" << wE+wF-1 << " downto " << wF<< "));" << endl;
			vhdl << tab << declare("sR") << " <= X(" << wE+wF << ") xor Y(" << wE+wF<< ");" << endl;
			vhdl << tab << "-- early exception handling " <<endl;
			vhdl << tab << declare("exnXY",4) << " <= X(" << wE+wF+2 << " downto " << wE+wF+1  << ") & Y(" << wE+wF+2 << " downto " << wE+wF+1 << ");" <<endl;
			vhdl << tab << "with exnXY select" <<endl;
			vhdl << tab << tab << declare("exnR0", 2) << " <= " << endl;
			vhdl << tab << tab << tab << "\"01\"  when \"0101\",                   -- normal" <<endl;
			vhdl << tab << tab << tab << "\"00\"  when \"0001\" | \"0010\" | \"0110\", -- zero" <<endl;
			vhdl << tab << tab << tab << "\"10\"  when \"0100\" | \"1000\" | \"1001\", -- overflow" <<endl;
			vhdl << tab << tab << tab << "\"11\"  when others;                   -- NaN" <<endl;
			vhdl << tab << " -- compute 3Y" << endl;
			vhdl << tab << declare("fYTimes3",wF+3) << " <= (\"00\" & fY) + (\"0\" & fY & \"0\");" << endl; // TODO an IntAdder here

			ostringstream wInit;
			wInit << "w"<<nDigit-1;
			vhdl << tab << declare(wInit.str(), wF+3) <<" <=  \"00\" & fX;" << endl;

			//			nextCycle();/////////////////////////////////////////////////////////////
			setCriticalPath(0);

			double srt4stepdelay =  2*target->lutDelay() + target->localWireDelay() + target->localWireDelay(wF+4) + target->adderDelay(wF+4);

			SelFunctionTable* table;
			if(alpha==3)
				table = new SelFunctionTable(target, 0.5, 1.0, 1, 4, 3, 4, 5, 3);
			else if(alpha==2)
				//				table = new SelFunctionTable(target, 0.5, 1.0, 3, 7, 2, 4, 10, 3);
				table = new SelFunctionTable(target, 0.5, 1.0, 3, 6, 2, 4, 9, 3);
			else THROWERROR("alpha="<< alpha << " is not an option");
			
			addSubComponent(table);

			for(i=nDigit-1; i>=1; i--) {
				manageCriticalPath(srt4stepdelay);

				ostringstream wi, qi, wim1, seli, qiTimesD, wipad, wim1full, tInstance;
				wi << "w" << i;						//actual partial remainder
				qi << "q" << i;						//actual quotient digit, LUT's output
				wim1 << "w" << i-1;					//partial remainder for the next iteration, = left shifted wim1full
				seli << "sel" << i;					//constructed as the wi's first 4 digits and D's first, LUT's input
				qiTimesD << "q" << i << "D";		//qi*D
				wipad << "w" << i << "pad";			//1-left-shifted wi
				wim1full << "w" << i-1 << "full";	//partial remainder after this iteration, = wi+qi*D
				tInstance << "SelFunctionTable" << i;

				/*
						Detailed algorithm for alpha=3 :
					*	building seli for the selection function
						seli = wi (25 downto 22) & fY(22), i.e. the first 4 digits of the remainder and the first useful digit of the divisor
					*	deducing the value of qi out of seli
					*	deducing the value of qi*D out of qi
						qi is bounded to [-3,3], so you can compute qi*D in 1 addition
					*	D : 24 bits, qi : 3 bits => qi*D : potentially 27 bits
						or wi is 26 bits long
						=>computing wipad = wi left shifted (27 bits)
					*	computing the remainder of this step
						wi-1full = wi-qi*D
					*	left shifting wi-1full to obtain wi-1, next partial remainder to work on
				*/
				if(alpha==3)
					vhdl << tab << declare(seli.str(),5) << " <= " << wi.str() << range( wF+2, wF-1) << " & fY" << of(wF-1)  << ";" << endl;
				else // alpha==2
					vhdl << tab << declare(seli.str(),9) << " <= " << wi.str() << range( wF+2, wF-3) << " & fY" << range(wF-1,wF-3)  << ";" << endl;
					//vhdl << tab << declare(seli.str(),10) << " <= " << wi.str() << range( wF+2, wF-4) << " & fY" << range(wF-1,wF-3)  << ";" << endl;
					
				inPortMap (table , "X", seli.str());
				outPortMap(table , "Y", qi.str());
				vhdl << instance(table , tInstance.str());
				vhdl << endl;

				if(alpha==3) {
					// Two options for radix 4. More experiments are needed 
#if 1  // The following leads to higher frequency and higher resource usage: 
					// For (8,23) on Virtex6 with ISE this gives 466Mhz, 1083 regs+ 1029 LUTs 
					vhdl << tab << "with " << qi.str() << " select" << endl;
					vhdl << tab << tab << declare(qiTimesD.str(),wF+4) << " <= "<< endl ;
					vhdl << tab << tab << tab << "\"000\" & fY            when \"001\" | \"111\"," << endl;
					vhdl << tab << tab << tab << "\"00\" & fY & \"0\"       when \"010\" | \"110\"," << endl;
					vhdl << tab << tab << tab << "\"0\" & fYTimes3        when \"011\" | \"101\"," << endl;
					vhdl << tab << tab << tab << "(" << wF+3 << " downto 0 => '0')  when others;" << endl;
					vhdl << endl;
#else // Recompute 3Y locally to save the registers: the LUT is used anyway
					// For (8,23) on Virtex6 with ISE this gives 345Mhz, 856 regs+ 1051 LUTs 
					vhdl << tab << "with " << qi.str() << " select" << endl;
					vhdl << tab << tab << declare(join("addendA",i),wF+4) << " <= "<< endl ;
					vhdl << tab << tab << tab << "\"000\" & fY            when \"001\" | \"111\" | \"011\" | \"101\"," << endl;
					vhdl << tab << tab << tab << "(" << wF+3 << " downto 0 => '0')  when others;" << endl;

					vhdl << tab << "with " << qi.str() << " select" << endl;
					vhdl << tab << tab << declare(join("addendB",i),wF+4) << " <= "<< endl ;
					vhdl << tab << tab << tab << "\"00\" & fY & \"0\"       when \"010\" | \"110\"| \"011\" | \"101\"," << endl;
					vhdl << tab << tab << tab << "(" << wF+3 << " downto 0 => '0')  when others;" << endl;
					
					vhdl << tab << tab << declare(qiTimesD.str(),wF+4) << " <= " << join("addendA",i) << " + " << join("addendB",i) << ";"<< endl ;
					vhdl << endl;
#endif				
					vhdl << tab << declare(wipad.str(), wF+4) << " <= " << wi.str() << " & \"0\";" << endl;
					vhdl << tab << "with " << qi.str() << "(2) select" << endl;
					vhdl << tab << declare(wim1full.str(), wF+4) << "<= " << wipad.str() << " - " << qiTimesD.str() << " when '0'," << endl;
					vhdl << tab << "      " << wipad.str() << " + " << qiTimesD.str() << " when others;" << endl;
					vhdl << endl;
					vhdl << tab << declare(wim1.str(),wF+3) << " <= " << wim1full.str()<<range(wF+1,0)<<" & \"0\";" << endl;
				} // end if alpha=3
				else {
					vhdl << tab << "with " << qi.str() << " select" << endl;
					vhdl << tab << tab << declare(qiTimesD.str(),wF+4) << " <= "<< endl ;
					vhdl << tab << tab << tab << "\"000\" & fY            when \"001\" | \"111\"," << endl;
					vhdl << tab << tab << tab << "\"00\" & fY & \"0\"       when \"010\" | \"110\"," << endl;
					vhdl << tab << tab << tab << "(" << wF+3 << " downto 0 => '0')  when others;" << endl;
					vhdl << endl;
					vhdl << tab << declare(wipad.str(), wF+4) << " <= " << wi.str() << " & \"0\";" << endl;
					vhdl << tab << "with " << qi.str() << "(2) select" << endl;
					vhdl << tab << declare(wim1full.str(), wF+4) << "<= " << wipad.str() << " - " << qiTimesD.str() << " when '0'," << endl;
					vhdl << tab << "      " << wipad.str() << " + " << qiTimesD.str() << " when others;" << endl;
					vhdl << endl;
					vhdl << tab << declare(wim1.str(),wF+3) << " <= " << wim1full.str()<<range(wF+1,0)<<" & \"0\";" << endl;

				}
			} // end loop

			manageCriticalPath(srt4stepdelay);

			vhdl << tab << declare("q0",3) << "(2 downto 0) <= \"000\" when  w0 = (" << wF+2 << " downto 0 => '0')" << endl;
			vhdl << tab << "             else w0(" << wF+2 << ") & \"10\";" << endl;

			for(i=nDigit-1; i>=1; i--) {
				ostringstream qi, qPi, qMi;
				qi << "q" << i;
				qPi << "qP" << i;
				qMi << "qM" << i;
				vhdl << tab << declare(qPi.str(), 2) <<" <=      " << qi.str() << "(1 downto 0);" << endl;
				vhdl << tab << declare(qMi.str(), 2)<<" <=      " << qi.str() << "(2) & \"0\";" << endl;
			}

			vhdl << tab << declare("qP0", 2) << " <= q0(1 downto 0);" << endl;
			vhdl << tab << declare("qM0", 2) << " <= q0(2)  & \"0\";" << endl;

			vhdl << tab << declare("qP", 2*nDigit) << " <= qP" << nDigit-1;
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qP" << i;
			vhdl << ";" << endl;

			vhdl << tab << declare("qM", 2*nDigit) << " <= qM" << nDigit-1 << "(0)";
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qM" << i;
			vhdl << " & \"0\";" << endl;


			// TODO an IntAdder here
			vhdl << tab << declare("fR0", 2*nDigit) << " <= qP - qM;" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////


			vhdl << tab << declare("fR", wF+4) << " <= ";
			if (1 == (wF & 1) ) // odd wF
				vhdl << "fR0(" << 2*nDigit-1 << " downto 1);  -- odd wF" << endl;
			else
				vhdl << "fR0(" << 2*nDigit-1 << " downto 3)  & (fR0(2) or fR0(1));  -- even wF, fixing the round bit" << endl;


			vhdl << tab << "-- normalisation" << endl;
			vhdl << tab << "with fR(" << wF+3 << ") select" << endl;

			vhdl << tab << tab << declare("fRn1", wF+2) << " <= fR(" << wF+2 << " downto 2) & (fR(1) or fR(0)) when '1'," << endl;
			vhdl << tab << tab << "        fR(" << wF+1 << " downto 0)                    when others;" << endl;

			vhdl << tab << declare("expR1", wE+2) << " <= expR0"
				  << " + (\"000\" & (" << wE-2 << " downto 1 => '1') & fR(" << wF+3 << ")); -- add back bias" << endl;



			vhdl << tab << declare("round") << " <= fRn1(1) and (fRn1(2) or fRn1(0)); -- fRn1(0) is the sticky bit" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////
			vhdl << tab << "-- final rounding" <<endl;
			vhdl << tab <<  declare("expfrac", wE+wF+2) << " <= "
				 << "expR1 & fRn1(" << wF+1 << " downto 2) ;" << endl;
			vhdl << tab << declare("expfracR", wE+wF+2) << " <= "
				 << "expfrac + ((" << wE+wF+1 << " downto 1 => '0') & round);" << endl;
			vhdl << tab <<  declare("exnR", 2) << " <=      \"00\"  when expfracR(" << wE+wF+1 << ") = '1'   -- underflow" <<endl;
			vhdl << tab << "        else \"10\"  when  expfracR(" << wE+wF+1 << " downto " << wE+wF << ") =  \"01\" -- overflow" <<endl;
			vhdl << tab << "        else \"01\";      -- 00, normal case" <<endl;


			vhdl << tab << "with exnR0 select" <<endl;
			vhdl << tab << tab << declare("exnRfinal", 2) << " <= " <<endl;
			vhdl << tab << tab << tab << "exnR   when \"01\", -- normal" <<endl;
			vhdl << tab << tab << tab << "exnR0  when others;" <<endl;
			vhdl << tab << "R <= exnRfinal & sR & "
				 << "expfracR(" << wE+wF-1 << " downto 0);" <<endl;
		}
	}

	FPDiv::~FPDiv() {
	}



	// Various functions that used to be in NbBitsMin
	void FPDiv::computeNbBit (int radix, int digitSet)
	{

		double ro = (double)digitSet/(radix-1);
		cout<<"Rendundancy coefficiant is rho="<<ro<<endl<<endl;

		// eq. 5.82 p. 293 of Digital Arithmetic
		double exactDeltaMin = 1-log2((2*ro - 1)/(2*(digitSet-1)));

		int deltaMinMinus = exactDeltaMin;
		int deltaMinPlus = deltaMinMinus + 1;

		double nbBitMinus = -log2((2*ro-1)/2 - (digitSet-ro)*pow(2, 0-deltaMinMinus))+deltaMinMinus+log2(radix);
		int nbBitM = ceil(nbBitMinus);

		double nbBitPlus = -log2((2*ro-1)/2 - (digitSet-ro)*pow(2, 0-deltaMinPlus))+deltaMinPlus+log2(radix);
		int nbBitP = ceil(nbBitPlus);

		cout<<"There are 2 possibilities :"<<endl;
		cout<<"-Delta = "<<deltaMinPlus<<", nbBits = "<<nbBitP<<endl;
		cout<<"-Delta = "<<deltaMinMinus<<", nbBits = "<<nbBitM<<endl<<endl;

		int delta = (nbBitP-nbBitM < 0 ? deltaMinPlus : deltaMinMinus);
		int nbBit = (nbBitP-nbBitM < 0 ? nbBitP : nbBitM);

		cout<<"Therefore, the min is for Delta = "<<delta<<endl;
		cout<<"You'll need "<<nbBit<<" bits : ";
		cout<<delta-1<<" bits for D and ";
		cout<<nbBit-delta+1;
		cout<<" bits for the partial remainder"<<endl;

		cout<<"Checking for better configurations..."<<endl<<endl;

		//Optimized parameters for a digit set [-7,7] with a radix 8
		//delta = 3;
		//nbBit = 7;
		//radix = 8;
		//digitSet = 7;

		if(checkDistrib(delta, nbBit-delta+1, radix, digitSet))
		{
			int gain = 0;
			cout<<"Optimization found!"<<endl;
			while(checkDistrib(delta, nbBit-delta, radix, digitSet))
			{
				nbBit--;
				gain++;
			}
			while(checkDistrib(delta-1, nbBit-delta+1, radix, digitSet))
			{
				delta--;
				nbBit--;
				gain++;
			}

			cout<<"You lost "<<gain<<" extra bits"<<endl;
		}
		else
		{
			cout<<"No better configuration found"<<endl;
		}


		cout<<"Final values are : ";
		cout<<delta-1<<" bits for D and ";
		cout<<nbBit-delta+1;
		cout<<" bits for the partial remainder"<<endl;

		plotPDDiagram(delta-1, nbBit-delta+1, radix, digitSet);
		//plotPDDiagram(3, 5, 8, 7);

		cout<<"An implementation of this configuration is approximately "<<estimateCost(nbBit, radix, digitSet)<<" times larger than the actual implementation"<<endl;

	}

	//Produces the P-D Diagram corresponding to the previous analysis in svg format
	void FPDiv::plotPDDiagram(int delta, int t, int radix, int digitSet)
	{
		double ro = (double)digitSet/((double)radix-1);

		ofstream svg("PDDiagram.svg", ios::out|ios::trunc);
		const int width = 1024;
		const int height = 750;
		int maxH = (-digitSet-ro)*(height/2)/(digitSet+1); //The height doesn't always match with the extrema of Uk and Lk,
		int minH = (digitSet+ro)*(height/2)/(digitSet+1);  //so those value are used to draw a better graph

		//Doc info
		svg<<"<?xml version=\"1.0\"?>"<<endl;
		svg<<"<!DOCTYPE  svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">"<<endl;
		svg<<"<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\" >"<<endl;
		svg<<"<text y=\"15\">Les cases incluent leurs bords bas et gauche</text>"<<endl;
		svg<<"<text y=\"40\">		radix"<<radix<<", [-"<<digitSet<<","<<digitSet<<"]"<<"</text>"<<endl;

		//Lk-Uk
		for(int i = -digitSet; i <= digitSet; i++)
		{
			//Lk
			svg<<"	<line x1=\""<<50<<"\" y1=\""<<50+height/2<<"\" x2=\""<<50+width<<"\" y2=\""<<50+height/2-(i-ro)*(height/2)/(digitSet+1);
			svg<<"\" style=\"stroke:rgb(0,200,200);stroke-width:2\"/>"<<endl;
			//Uk
			svg<<"	<line x1=\""<<50<<"\" y1=\""<<50+height/2<<"\" x2=\""<<50+width<<"\" y2=\""<<50+height/2-(i+ro)*(height/2)/(digitSet+1);
			svg<<"\" style=\"stroke:rgb(0,200,200);stroke-width:2\"/>"<<endl;
			//Overlapping
			svg<<"	<line x1=\""<<50+width+(i+digitSet+1)*10<<"\" y1=\""<<50+height/2-(i-ro)*(height/2)/(digitSet+1);
			svg<<"\" x2=\""<<50+width+(i+digitSet+1)*10<<"\" y2=\""<<50+height/2-(i+ro)*(height/2)/(digitSet+1);
			svg<<"\" style=\"stroke:rgb(0,0,200);stroke-width:2\"/>"<<endl;
		}


		//Horizontal axis subdiv
		int nbSubdiv = pow(2, delta);
		int pitch = width/nbSubdiv;

		for(int i = 0; i <= nbSubdiv; i++)
		{
			svg<<"	<line x1=\""<<50+width/2+i*pitch/2<<"\" y1=\""<<50+height/2+maxH;//50+3*height/4-((delta%2==0?i:i-1)*(-digitSet-ro)*(height/2)/(digitSet+1)/2/nbSubdiv);
			svg<<"\" x2=\""<<50+width/2+i*pitch/2<<"\" y2=\""<<50+height/2+minH;// /4-((delta%2==0?i:i-1)*(digitSet+ro)*(height/2)/(digitSet+1)/2/nbSubdiv);
			svg<<"\" style=\"stroke:rgb(100,100,100);stroke-width:2\"/>"<<endl;
		}


		//Vertical axis subdiv
		nbSubdiv = pow(2, t);

		for(int i = -nbSubdiv/2; i <= nbSubdiv/2; i++)
		{
			svg<<"	<line x1=\""<<50+width/2<<"\" y1=\""<<50+height/2+i*2*minH/nbSubdiv;
			svg<<"\" x2=\""<<50+width<<"\" y2=\""<<50+height/2+i*2*minH/nbSubdiv;
			svg<<"\" style=\"stroke:rgb(100,100,100);stroke-width:1\"/>"<<endl;
		}


		//Axis
		svg<<"	<line x1=\""<<50<<"\" y1=\""<<50<<"\" x2=\""<<50<<"\" y2=\""<<50+height<<"\" style=\"stroke:rgb(0,0,0);stroke-width:2\"/>"<<endl;//vertical 0
		svg<<"	<line x1=\""<<50<<"\" y1=\""<<50+height/2<<"\" x2=\""<<50+width<<"\" y2=\""<<50+height/2<<"\" style=\"stroke:rgb(0,0,0);stroke-width:2\"/>"<<endl;//horizontal

		svg<<"	<line x1=\""<<50+width<<"\" y1=\""<<50+height/2+minH;
		svg<<"\" x2=\""<<50+width<<"\" y2=\""<<50+height/2+maxH<<"\" style=\"stroke:rgb(255,0,0);stroke-width:2\"/>"<<endl;//vertical 1

		svg<<"	<line x1=\""<<50+width/2<<"\" y1=\""<<50+height/2+minH/2;
		svg<<"\" x2=\""<<50+width/2<<"\" y2=\""<<50+height/2+maxH/2<<"\" style=\"stroke:rgb(255,0,0);stroke-width:2\"/>"<<endl;//vertical 0.5

		//Prescaling (cas particulier base 8, digitSet 7)
	//	svg<<"	<line x1=\""<<50+width/2<<"\" y1=\""<<50+height/2+minH/2;
	//	svg<<"\" x2=\""<<50+width/2<<"\" y2=\""<<50+height/2+maxH/2<<"\" style=\"stroke:rgb(0,100,0);stroke-width:5\"/>"<<endl;//vertical 0.5
	//
	//	svg<<"	<line x1=\""<<50+5*width/8-3<<"\" y1=\""<<50+height/2+5*minH/8;
	//	svg<<"\" x2=\""<<50+5*width/8-3<<"\" y2=\""<<50+height/2+5*maxH/8<<"\" style=\"stroke:rgb(0,100,0);stroke-width:5\"/>"<<endl;//vertical 0.625-
	//
	//	svg<<"	<line x1=\""<<50+5*width/8+3<<"\" y1=\""<<50+height/2+5*minH/8;
	//	svg<<"\" x2=\""<<50+5*width/8+3<<"\" y2=\""<<50+height/2+5*maxH/8<<"\" style=\"stroke:rgb(0,200,0);stroke-width:5\"/>"<<endl;//vertical 0.625+
	//
	//	svg<<"	<line x1=\""<<50+3*width/4+3<<"\" y1=\""<<50+height/2+3*minH/4;
	//	svg<<"\" x2=\""<<50+3*width/4+3<<"\" y2=\""<<50+height/2+3*maxH/4<<"\" style=\"stroke:rgb(0,100,0);stroke-width:5\"/>"<<endl;//vertical 0.75+
	//
	//	svg<<"	<line x1=\""<<50+3*width/4-3<<"\" y1=\""<<50+height/2+3*minH/4;
	//	svg<<"\" x2=\""<<50+3*width/4-3<<"\" y2=\""<<50+height/2+3*maxH/4<<"\" style=\"stroke:rgb(0,200,0);stroke-width:5\"/>"<<endl;//vertical 0.75-
	//
	//	svg<<"	<line x1=\""<<50+25*width/32<<"\" y1=\""<<50+height/2+25*minH/32;
	//	svg<<"\" x2=\""<<50+25*width/32<<"\" y2=\""<<50+height/2+25*maxH/32<<"\" style=\"stroke:rgb(0,200,0);stroke-width:5\"/>"<<endl;//vertical 25/32
	//
	//	svg<<"	<line x1=\""<<50+15*width/16-3<<"\" y1=\""<<50+height/2+15*minH/16;
	//	svg<<"\" x2=\""<<50+15*width/16-3<<"\" y2=\""<<50+height/2+15*maxH/16<<"\" style=\"stroke:rgb(0,100,0);stroke-width:5\"/>"<<endl;//vertical 15/16-
	//
	//	svg<<"	<line x1=\""<<50+15*width/16+3<<"\" y1=\""<<50+height/2+15*minH/16;
	//	svg<<"\" x2=\""<<50+15*width/16+3<<"\" y2=\""<<50+height/2+15*maxH/16<<"\" style=\"stroke:rgb(0,200,0);stroke-width:5\"/>"<<endl;//vertical 15/16+


		//Case pb, cas particulier base 8 digitSet 7
	//	svg<<"	<rect x=\""<<50+5*width/8<<"\" y=\""<<50+3*minH/2<<"\" width=\""<<width/16<<"\" height=\""<<minH/16<<"\" style=\"fill:rgb(255,0,0);fill-opacity:0.3;\" />"<<endl;
	//	svg<<"	<rect x=\""<<50+5*width/8<<"\" y=\""<<50+minH/2-minH/16<<"\" width=\""<<width/16<<"\" height=\""<<minH/16<<"\" style=\"fill:rgb(255,0,0);fill-opacity:0.3;\" />"<<endl;
	//	svg<<"	<rect x=\""<<50+9*width/16<<"\" y=\""<<50+minH/2<<"\" width=\""<<width/16<<"\" height=\""<<minH/16<<"\" style=\"fill:rgb(255,0,0);fill-opacity:0.3;\" />"<<endl;
	//	svg<<"	<rect x=\""<<50+9*width/16<<"\" y=\""<<50+3*minH/2-minH/16<<"\" width=\""<<width/16<<"\" height=\""<<minH/16<<"\" style=\"fill:rgb(255,0,0);fill-opacity:0.3;\" />"<<endl;
	//	svg<<"	<rect x=\""<<50+9*width/16<<"\" y=\""<<50+3*minH/2-minH/8<<"\" width=\""<<width/16<<"\" height=\""<<minH/16<<"\" style=\"fill:rgb(255,0,0);fill-opacity:0.3;\" />"<<endl;
	//	svg<<"	<rect x=\""<<50+9*width/16<<"\" y=\""<<50+9*minH/16+1<<"\" width=\""<<width/16<<"\" height=\""<<minH/16<<"\" style=\"fill:rgb(255,0,0);fill-opacity:0.3;\" />"<<endl;

		//Fin
		svg<<"</svg>"<<endl;
	}


	bool FPDiv::checkDistrib(int delta, int t, int radix, int digitSet)
	{
		double ro = digitSet/(radix-1);
		double wMax = U(digitSet, ro, 1);

		for(int k = -digitSet+1; k <= digitSet; k++)
		{

			//the position of the left intersection with Lk
			double leftL = L(k, ro, 0.5);
			double leftCeiledL = pow(2,-(t-log2(radix)-1))*ceil(pow(2, (t-log2(radix)-1))*leftL);
			int leftLineL = (wMax-leftCeiledL)*pow(2,t)/(2*wMax);//line number in the grid from P-D diagram
			int leftCol = 0;
			bool leftCornerL = (leftL-leftCeiledL == 0);
			vector<int> crossedBoxes;

			//the position of the left intersection with Uk-1
			double leftU = U(k, ro, 0.5);
			double leftCeiledU = pow(2,-(t-log2(radix)-1))*ceil(pow(2, (t-log2(radix)-1))*leftU);
			int leftLineU = (wMax-leftCeiledU)*pow(2,t)/(2*wMax);//line number in the grid from P-D diagram
			bool leftCornerU = (leftU-leftCeiledU == 0);

			int index;

			for(double i = 0.5; i < 1 ; i += pow(2, -delta))
			{
				//the position of the right intersection with Lk
				double rightL = L(k, ro, i+pow(2, -delta));//value of Lk for d=i
				double rightCeiledL = pow(2,-(t-log2(radix)-1))*ceil(pow(2, (t-log2(radix)-1))*rightL);
				int rightLineL = (wMax-rightCeiledL)*pow(2,t)/(2*wMax);//line number in the grid from P-D diagram
				int rightCol = (i-0.5+pow(2, -delta))*pow(2, delta-1)/0.5;

				//the position of the right intersection with Uk-1
				double rightU = U(k-1, ro, i+pow(2, -delta));//value of Uk-1 for d=i
				double rightCeiledU = pow(2,-(t-log2(radix)-1))*ceil(pow(2, (t-log2(radix)-1))*rightU);
				int rightLineU = (wMax-rightCeiledU)*pow(2,t)/(2*wMax);//line number in the grid from P-D diagram

				bool rightCornerL = (rightL-rightCeiledL == 0);
				bool rightCornerU = (rightU-rightCeiledU == 0);

				if(leftCornerL && k-ro>0)
				{						//positive coeff =>the bottom-left corner, the box crossed is above the computed-one
										//       _______________
										//	    | /				|   <====== crossed box
					leftLineL--;		//		|/______________|
										//		/               |   <====== computed crossed box
										//	   /|_______________|
				}
				if(rightCornerL && k-ro<0)
				{
					rightLineL--;
				}


				if(leftCornerU && k-ro>0)
				{
					leftLineU--;
				}
				if(rightCornerU && k-ro<0)
				{
					rightLineU--;
				}




				for(int c = (k-ro<0 ? leftLineL : rightLineL); c <= (k-ro<0 ? rightLineL : leftLineL); c++)
				{
					index = leftCol + pow(2,delta-1)*c;
					crossedBoxes.push_back(index);
				}

				for(int c = (k-ro<0 ? leftLineU : rightLineU); c <= (k-ro<0 ? rightLineU : leftLineU); c++)
				{
					index = leftCol + pow(2,delta-1)*c;
					for(vector<int>::iterator it = crossedBoxes.begin(); it != crossedBoxes.end(); ++it)
					{
						if (index == *it)
						{
							return false;
						}
					}
				}



				rightLineL+=(rightCornerL && k-ro<0 ? 1 : 0);
				rightLineU+=(rightCornerU && k-ro<0 ? 1 : 0);

				leftL = rightL;
				leftCeiledL = rightCeiledL;
				leftLineL = rightLineL;
				leftU = rightU;
				leftCeiledU = rightCeiledU;
				leftLineU = rightLineU;
				leftCol = rightCol;

				crossedBoxes.clear();
			}

		}

		return true;
	}

	double FPDiv::L(int k, double ro, double d)
	{
		return (k-ro)*d;
	}

	double FPDiv::U(int k, double ro, double d)
	{
		return (k+ro)*d;
	}

	double FPDiv::estimateCost(int nbBit, int radix, int digitSet)
	{
		int nbIter = (23+6)>>1;
		double flopocoCost = nbIter*(2*27 + 3);
		cout<<"FlopocoCost = "<<flopocoCost<<endl;
		double res = (nbIter*2/log2(radix))*((ceil(log2(digitSet))+1)*ceil(pow(2, nbBit-6))+27*(1+ceil(pow(2, log2(digitSet)-7))));
		cout<<"Cost of this configuration = "<<res<<endl;
		return res/flopocoCost;
	}
	
	void FPDiv::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		/* Compute correct value */
		FPNumber fpx(wE, wF), fpy(wE, wF);
		fpx = svX;
		fpy = svY;
		mpfr_t x, y, r;
		mpfr_init2(x, 1+wF);
		mpfr_init2(y, 1+wF);
		mpfr_init2(r, 1+wF);
		fpx.getMPFR(x); // fake 0
		fpy.getMPFR(y);
		mpfr_div(r, x, y, GMP_RNDN);
		FPNumber  fpr(wE, wF, r);

		/* Set outputs */
		mpz_class svR = fpr.getSignalValue();
		tc->addExpectedOutput("R", svR);
		mpfr_clears(x, y, r, NULL);
	}



	void FPDiv::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		// Regression tests
		tc = new TestCase(this);
		tc->addFPInput("X", 1.0);
		tc->addFPInput("Y", FPNumber::plusDirtyZero);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", FPNumber::minusDirtyZero);
		tc->addFPInput("Y", FPNumber::plusInfty);
		emulate(tc);
		tcl->add(tc);


	}

	OperatorPtr FPDiv::parseArguments(Target *target, vector<string> &args) {
		int wE;
		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE);
		int wF;
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		int radix;
		UserInterface::parsePositiveInt(args, "radix", &radix);
		return new FPDiv(target, wE, wF, radix);
	}

	void FPDiv::registerFactory(){
		UserInterface::add("FPDiv", // name
											 "A correctly rounded floating-point division.",
											 "BasicFloatingPoint", // categories
											 "http://www.cs.ucla.edu/digital_arithmetic/files/ch5.pdf",
											 "wE(int): exponent size in bits; \
wF(int): mantissa size in bits; \
radix(int)=0: Can be 0, 4 or 8. Default 0 means: let FloPoCo choose between 4 and 8. In your context, the other choice may have a better area/speed trade-offs;",
											"The algorithm used here is the division by digit recurrence (SRT). In radix 4, we use a maximally redundant digit set. In radix 8, we use split-digits in [-10,10], and a bit of prescaling.",
											FPDiv::parseArguments
											 ) ;

	}


	OperatorPtr FPDiv::NbBitsMinParseArguments(Target *target, vector<string> &args) {
		int radix, digitSet;
		UserInterface::parseStrictlyPositiveInt(args, "radix", &radix);
		UserInterface::parseStrictlyPositiveInt(args, "digitSet", &digitSet);
		computeNbBit(radix, digitSet);
		return NULL;
	}

	void FPDiv::NbBitsMinRegisterFactory(){
		UserInterface::add("NbBitsMin", // name
											 "A tool for FPDiv to compute where to truncate both partial remainder and divider.",
											 "Miscellaneous", // categories
											 "",
											 "radix(int): It has to be 2^n; \
digitSet(int): the range you allow for each digit [-digitSet, digitSet]",
											 "",
											 FPDiv::NbBitsMinParseArguments
											 ) ;

	}


}
