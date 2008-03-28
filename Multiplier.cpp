/*
 * Floating Point Multiplier for FloPoCo
 *
 * Author : Bogdan Pasca
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

#include "Multiplier.hpp"

using namespace std;
extern vector<Operator*> oplist;

Multiplier::Multiplier(Target* target, int wEX, int wFX, int wEY, int wFY, int wER, int wFR, int norm) :
  Operator(target), wEX(wEX), wFX(wFX), wEY(wEY), wFY(wFY), wER(wER), wFR(wFR) {
  
  int i;
  ostringstream name, synch, synch2;

  //set the normalized attribute  
  if (norm==0) 
    normalized=false;	
  else
    normalized=true;	
	
  // Name Setup procedure
  //============================================================================
  //The name has the format: Multiplier_wEX_wFX_wEY_wFY_wER_WFR
  // wEX = width of X exponenet
  // wFX = width for the fractional part of X
  name.str("");
  name <<"Multiplier_"; 
  name<<wEX<<"_"<<wFX<<"_"<<wEY<<"_"<<wFY<<"_"<<wER<<"_"<<wFR;
  unique_name = name.str(); //set the attribute
  //============================================================================  

  // Set up the IO signals
  //============================================================================
  add_input ("X", wEX + wFX + 3);	// format: 2b(Exception) + 1b(Sign)
  add_input ("Y", wEY + wFY + 3);	// + wEX bits (Exponent) + wFX bits(Fraction)
   
  add_output ("ResultExponent", wER);  
  add_output ("ResultSignificand", wFR + 1);
  add_output ("ResultException", 2);
  add_output ("ResultSign",1); 		
  //============================================================================

  //Set up if we are in the sequential or combinational case
  if (target->is_pipelined()) 
   set_sequential();
  else
   set_combinatorial(); 
    
  //Make an IntMultiplier -> multiply the significands
  intmult = new IntMultiplier(target, wFX+1, wFY+1);
  
  //Setup the attibute cocerning the IntMultiplier pipeline 
  IntMultPipelineDepth = intmult->pipeline_depth();
  cout<<endl<<"INT multipler depth  = "<<IntMultPipelineDepth <<endl;
  
  // Unregistered signals
  //============================================================================  

  if (normalized)  {
      add_signal("mult_selector",1);
      add_signal("exponent_p",1+wEX);
      add_signal("exception_upd",2);
      add_signal("check_string",wFX+wFY+2 - (1+wFR) );
      add_signal("reunion_signal",2+wEX+wFR);
      add_signal("significand_synch_tb2",1); 
      add_signal("reunion_signal_p",2+wEX+wFR);  
      add_signal("middle_signal",2+wEX+wFR);
      add_signal("reunion_signal_p_upd",2+wEX+wFR);
  }	

  add_signal("significandX", wFX+1);   //The Significands for the input signals X and Y
  add_signal("significandY", wFY+1);
  add_signal("significand_multiplication", wFX +wFY +2);   
  
  add_signal("exponentX", wEX);	//The signals for the exponents of X and Y
  add_signal("exponentY", wEY);
  add_signal("bias",wEX+1); //bias to be substracted from the sum of exponents  
  add_signal("exponents_sum_minus_bias_ext",wEX+1); //extended with 1 bit
   
  add_signal("exception_selector",4);//signal that selects the case for the exception
  
  if (is_sequential())
  {
      add_signal("exponent_synch", wEX);
      add_signal("significand_synch", wFX + wFY + 2); 
      add_signal("sign_synch", 1); 
      add_signal("exception_synch",2);
      add_signal("temp_exp_concat_fract", 1 + wEX + wFX + wFY + 2);
  }
  
 

  if (is_sequential())  {
  // Registered Signals
  // ===========================================================================
      
      if (normalized){ 
         add_registered_signal_with_reset("middle_output",1);
         add_registered_signal_with_reset("exponent_synch2",wEX);
			add_registered_signal_with_reset("exception_synch2",2);
		   add_registered_signal_with_reset("significand_synch2",wFX + wFY + 2);
   		add_registered_signal_with_reset("sign_synch2",1);
   		add_registered_signal_with_reset("mult_selector2",1);
      }

      add_registered_signal_with_reset("Exponents_Sum", wEX+1 );
      add_registered_signal_with_reset("Exponents_Sum_Minus_Bias", wEX );

      //registers for the case when the integer multiplication requires 
      //more than two pipeline levels
      for (i=0;i<=IntMultPipelineDepth-3;i++)	{
      synch.str(""); synch2.str("");
      synch<<"Exponent_Synchronization_"<<i;
      add_registered_signal_with_reset(synch.str(), wEX );
      synch2<<"Exception_Synchronization_"<<i;
      add_registered_signal_with_reset(synch2.str(), 2 );	
      }

      //registers for the case when the int mult has less than two levels
      for (i=0;i<=1-IntMultPipelineDepth;i++)	{
      synch.str("");
      synch<<"Int_Synchronization_"<<i;
      add_registered_signal_with_reset(synch.str(), wFX+wFY+2);		
      }	

      add_delay_signal("Sign_Synchronization", 1, max(2,IntMultPipelineDepth));

      add_registered_signal_with_reset("Result_Exception",2); 
      add_registered_signal_with_reset("Result_Exception_With_EA",2);

  // ===========================================================================
  }//end if sequential
  else
  {//if combinational
      add_signal("middle_output",1);
      add_signal("exponent_p2",wEX);
      add_signal("Exponents_Sum", wEX+1 );
      add_signal("Exponents_Sum_Minus_Bias", wEX );   
      add_signal("Result_Exception",2); 
      add_signal("Result_Exception_With_EA",2);
   }
  

  oplist.push_back(intmult);
}






Multiplier::~Multiplier() {
}







void Multiplier::output_vhdl(std::ostream& o, std::string name) {
  
  ostringstream signame, synch1, synch2, xname,zeros, zeros1, zeros2, str1, str2;
  
  int bias_val=int(pow(double(2),double(wEX-1)))-1;
  int i; 
  
  Licence(o,"Bogdan Pasca (2008)");
  Operator::StdLibs(o);
  o<<endl<<endl;	
  output_vhdl_entity(o);
  
  
  o << "architecture arch of " << name  << " is" << endl;
  
  intmult->output_vhdl_component(o);
  output_vhdl_signal_declarations(o);	  
  
  o<<endl;

  o << "begin" << endl;
  
   if (is_sequential()){
   	//common code for both normalized and non-normalized version
   	output_vhdl_registers(o); o<<endl;
					
	   o<<tab<<"exponentX <= X("<<wEX + wFX -1<<" downto "<<wFX<<");"<<endl; 
	   o<<tab<<"exponentY <= Y("<<wEY + wFY -1<<" downto "<<wFY<<");"<<endl<<endl;
	
	   //Add exponents -> put exponents sum into register
	   o<<tab<<"Exponents_Sum <= (\"0\" & exponentX) + (\"0\" & exponentY);"<<endl; //wEX+1 bits
	   o<<tab<<"bias <= CONV_STD_LOGIC_VECTOR("<<bias_val<<","<<wEX+1<<");"<<endl; 
		
      //substract the bias value from the exponent sum
      o<<tab<<"exponents_sum_minus_bias_ext <= Exponents_Sum_d - bias;"<<endl;//wEX + 1   
	   o<<tab<<"Exponents_Sum_Minus_Bias <= exponents_sum_minus_bias_ext("<<wEX-1<<" downto 0);"<<endl; //wEX
   
      //build significands 	 	
      o<<tab<< "significandX <= \"1\" & X("<<(wFX-1)<<" downto 0);"<<endl;
  	   o<<tab<< "significandY <= \"1\" & Y("<<(wFY-1)<<" downto 0);"<<endl<<endl;
	 	
	   //multiply significands 
		o<<tab<< "int_multiplier_component: " << intmult->unique_name << endl;
		o<<tab<< "      port map ( X => significandX, " << endl; //wFX+1 bits (1 bit is the hidden 1)
		o<<tab<< "                 Y => significandY, " << endl; //wFY+1 bits
		o<<tab<< "                 R => significand_multiplication, " << endl; //wFX+wFY+2 bits
		o<<tab<< "                 clk => clk, " << endl;
		o<<tab<< "                 rst => rst " << endl;
		o<<tab<< "               );" << endl<<endl;
	
	   //Concatenate the exception bits of the two input numbers
		   o<<tab<<"exception_selector <= X("<<wEX + wFX +2<<" downto "<<wEX + wFX + 1<<") & Y("<<wEY + wFY +2<<" downto "<<wEY + wFY +1<<");"<<endl<<endl;
		
      //select proper exception with selector			
      o<<tab<<"process (exception_selector)"<<endl;
	   o<<tab<<"begin"<<endl;
	   o<<tab<<"case (exception_selector) is"<< endl;
	   o<<tab<<tab<<"when \"0000\" => Result_Exception <= \"00\";   "<<endl;
	   o<<tab<<tab<<"when \"0001\" => Result_Exception <= \"00\";   "<<endl;
	   o<<tab<<tab<<"when \"0010\" => Result_Exception <= \"11\";   "<<endl;
	   o<<tab<<tab<<"when \"0011\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"0100\" => Result_Exception <= \"00\";   "<<endl;
		o<<tab<<tab<<"when \"0101\" => Result_Exception <= \"01\";   "<<endl;//to see
		o<<tab<<tab<<"when \"0110\" => Result_Exception <= \"10\";   "<<endl;
		o<<tab<<tab<<"when \"0111\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1000\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1001\" => Result_Exception <= \"10\";   "<<endl;
		o<<tab<<tab<<"when \"1010\" => Result_Exception <= \"10\";   "<<endl;
		o<<tab<<tab<<"when \"1011\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1100\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1101\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1110\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1111\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when others =>   Result_Exception <= \"11\";   "<<endl;    
		o<<tab<<"end case;"<<endl;
		o<<tab<<"end process;"<<endl<<endl;

      //use MSB of exponents_sum_minus_bias_ext to siagnal overflow		
      o<<tab<<"Result_Exception_With_EA <= Result_Exception_d when exponents_sum_minus_bias_ext("<< wEX<<")='0' else"<<endl;
	   o<<tab<<"				                    \"11\";"<<endl<<endl;
										
      // synchronization barrier (IntMultiplication | Exponent Addition | Exception Computation | Sign Computation
      // 0. when IntMultiplication | Exponent Addition are allready synchronized	
      // 1. when registers are added to exponent addition
      // 2. when registers are added to integer multiplication
	  
      if (IntMultPipelineDepth==2){ 
	   o<<tab<<"exponent_synch    <= Exponents_Sum_Minus_Bias_d;"<<endl;
	   o<<tab<<"significand_synch <= significand_multiplication;"<<endl;
	   o<<tab<<"exception_synch   <= Result_Exception_With_EA_d;"<<endl;
	   }
	   else{	
		   if (IntMultPipelineDepth > 2){
			   //connect the first synchronization reg to the output of the last reg from exponent addition 
				o<<tab<<"Exponent_Synchronization_0  <= Exponents_Sum_Minus_Bias_d;"<<endl;
			   o<<tab<<"Exception_Synchronization_0 <= Result_Exception_With_EA_d;"<<endl; 									
			   
            for (i=1;i<=IntMultPipelineDepth-3;i++){
				  o<<tab<<"Exponent_Synchronization_"<<i<<"   <= "<<"Exponent_Synchronization_"<<i-1<<"_d;"<<endl;		
				  o<<tab<<"Exception_Synchronization_"<<i<<" <= "<<"Exception_Synchronization_"<<i-1<<"_d;"<<endl;
			   }
			   o<<tab<<"exponent_synch    <= Exponent_Synchronization_"<<	IntMultPipelineDepth-3 <<"_d;"<<endl;
			   o<<tab<<"exception_synch   <= Exception_Synchronization_"<<IntMultPipelineDepth-3 <<"_d;"<<endl;
			   o<<tab<<"significand_synch <= significand_multiplication;"<<endl;			
		   }
		   else{
         //add registers to the output of IntMultiplier so that exponent addition and 
		   //significand multiplication become synchronized
			   o<<tab<<"Int_Synchronization_0 <= significand_multiplication;"<<endl;
				for (i=1;i<=1-IntMultPipelineDepth;i++){
				   synch1.str(""); synch2.str("");
				   synch2<<"Int_Synchronization_"<<i;
				   synch1<<"Int_Synchronization_"<<i-1<<"_d";
				   o<<tab<<synch2.str()<<"<="<<synch1.str()<<";"<<endl;		
			   }
            o<<tab<<"exponent_synch    <= Exponents_Sum_Minus_Bias_d;"<<endl;
				o<<tab<<"exception_synch   <= Result_Exception_With_EA_d;"<<endl;
				o<<tab<<"significand_synch <= Int_Synchronization_"<< 1-IntMultPipelineDepth<<"_d;"<<endl;
		   }
	   }
		
	   //sign synchronization
	   o<<tab<<"Sign_Synchronization <= X("<<wEX + wFX<<") xor Y("<<wEY + wFY<<");"<<endl;
	   xname.str("");	 xname << "Sign_Synchronization";		
	   for (i=0; i<max(2,IntMultPipelineDepth); i++)
         xname << "_d";
	   o<<tab<<"sign_synch <= "<<xname.str()<<";"<<endl<<endl;

      
      
           
      if (normalized==true){	
		   
		   //normalize result
		      //assign selector signal to MSB of signif. multiplic 
		      o<<tab<<"mult_selector <= significand_synch ("<<wFX+wFY+1<<");"<<endl;
   		   
   		   o<<tab<<"exponent_p <= (\"0\" & exponent_synch ) + (CONV_STD_LOGIC_VECTOR(0 ,"<<wEX<<") & mult_selector);"<<endl;
   		   
   		   //signal the exception
   		   o<<tab<<"exception_upd <= exception_synch when exponent_p("<<wEX<<")='0' else"<<endl;
   		   o<<tab<<"                \"11\";"<<endl;
		   
		   //round result
		      //check is rounding is needed
		      if (1+wFR >= wFX+wFY+2) {
		         //=>no rounding needed - eventual padding
		         //generate zeros of length 1+wFR - wFX+wFY+2 
		         zeros1.str("");
		         zeros1 << zero_generator(1+wFR - wFX+wFY+2 , 0);
		         
		         //make the outputs
		          o<<tab<<"ResultExponent <= exponent_p("<<wEX - 1<<" downto 0);"<<endl;  
                o<<tab<<"ResultSignificand <= significand_synch & "<<zeros1.str()<<" when mult_selector='1' else"<<endl;
                o<<tab<<"                     significand_synch("<<wFX+wFY<<" downto 0) & "<<zeros1.str()<<" & \"0\";"<<endl;
                o<<tab<<"ResultException <= exception_upd;"<<endl;
                o<<tab<<"ResultSign <= sign_synch;"<<endl;
		         
		      }
		      else{
   		      //check if in the middle of two FP numbers
   		      //generate two xor strings
   		      str1.str(""); str2.str(""); //init
   		      str1<<"\"1"<< zero_generator( wFX+wFY+2 - (1+wFR) -1, 1);
   		      str2<<"\"01"<< zero_generator( wFX+wFY+2 - (1+wFR) -1-1, 1);
   		      
   		      
   		      zeros1.str("");
   		      zeros1<< zero_generator( wFX+wFY+2 - (1+wFR) , 0);
   		      
   		      o<<tab<<"check_string <= (significand_synch ("<< wFX+wFY+2 - (1+wFR) -1<<" downto 0) xor "<<str1.str()<<") when mult_selector='1' else"<<endl;
   		      o<<tab<<"               ( (\"0\" & significand_synch ("<< wFX+wFY+2 - (1+wFR) -1-1<<" downto 0)) xor "<<str2.str()<<");"<<endl;
   		      
   		      o<<tab<<"process(clk)"<<endl;
               o<<tab<<"begin"<<endl;
               o<<tab<<tab<<"   if (clk'event and clk='1') then "<<endl;  
               o<<tab<<tab<<"      if ( "<<zeros1.str()<<" = check_string ) then"<<endl; 
               o<<tab<<tab<<"         middle_output <= '1';"<<endl;
               o<<tab<<tab<<"      else "<<endl;
               o<<tab<<tab<<"         middle_output <= '0';"<<endl;
               o<<tab<<tab<<"      end if;"<<endl;
               o<<tab<<tab<<"   end if; "<<endl;
               o<<tab<<tab<<"end process; "<<endl;
   		      
   		      //propagate rest of signals 1 level
   		      o<<tab<<"exponent_synch2 <= exponent_p("<<wEX-1<<" downto 0);"<<endl;
				   o<<tab<<"exception_synch2 <= exception_upd;"<<endl;
				   o<<tab<<"significand_synch2 <= significand_synch;"<<endl;
   		      o<<tab<<"sign_synch2 <= sign_synch;"<<endl;
   		      o<<tab<<"mult_selector2 <= mult_selector;"<<endl;
   		      
   		      o<<tab<<"reunion_signal <= (\"0\" & exponent_synch2_d & significand_synch2_d("<< wFX+wFY<<" downto "<<wFX+wFY - (wFR) <<" )) when mult_selector2_d='1' else"<<endl;
   		      o<<tab<<"                 (\"0\" & exponent_synch2_d & significand_synch2_d("<< wFX+wFY-1<<" downto "<<wFX+wFY - (wFR) -1<<" ));"<<endl; 
   		     
                              
               o<<tab<<"significand_synch_tb2 <= significand_synch2_d("<< wFX+wFY+1 - wFR<<" ) when mult_selector2_d='1' else"<<endl;
               o<<tab<<"                        significand_synch2_d("<< wFX+wFY+1 - wFR -1<<" );"<<endl;              
                  		     
               o<<tab<<"reunion_signal_p <= reunion_signal + CONV_STD_LOGIC_VECTOR(1, "<< 2+ wEX + wFR<<");"<<endl;
                  		     
               o<<tab<<"middle_signal <= reunion_signal_p when significand_synch_tb2 = '1' else"<<endl;
               o<<tab<<"                reunion_signal;"<<endl;    		     
                  		     
               o<<tab<<"reunion_signal_p_upd <= reunion_signal_p when middle_output_d='0' else"<<endl;
   		      o<<tab<<"                       middle_signal;"<<endl;
   		                             
   		      //update exception                       
   		      o<<tab<<"ResultException <= exception_synch2 when reunion_signal_p_upd("<<wEX + wFR+1<<")='0' else"<<endl;
   		      o<<tab<<"                   \"11\";"<<endl;                                                   
   		      
   		      o<<tab<<"ResultExponent <= reunion_signal_p_upd("<<wEX + wFR<<" downto "<<wFR+1<<");"<<endl;  
               o<<tab<<"ResultSignificand <= \"1\" & reunion_signal_p_upd("<<wFR<<" downto 1);"<<endl;
               o<<tab<<"ResultSign <= sign_synch2_d;"<<endl;
		         
		      }
		     
		}
	   else { // 
		  
         if (1+wFR < wFX+wFY+2){
         //do rounding
            zeros1.str("");	zeros2.str("");		
            zeros1 << zero_generator(wEX + wFR + 2, -1)<<"1"<<zero_generator(wFX+wFY+2-(wFR+1)-1, 1);
            zeros2 << zero_generator(wEX + wFR + 3, -1)<<"1"<<zero_generator(wFX+wFY+2-(wFR+1)-2, 1);  
				
			   o<<tab<<"temp_exp_concat_fract <= (\"0\" & exponent_synch & significand_synch) + "<< zeros1.str()<<" when significand_synch("<<wEX+wEY+1<<")='1' else	"<<endl;
			   o<<tab<<"                           (\"0\" & exponent_synch & significand_synch) + "<<zeros2.str()<<";"<<endl;	
			
			   o<<tab<<"ResultSignificand <= temp_exp_concat_fract ("<<wFX+wFY+1<<" downto "<< wFX+wFY+1 - (wFR+1)+1<<");"<<endl;
			   o<<tab<<"ResultExponent    <= temp_exp_concat_fract ("<< wEX+wFX+wFY + 1 <<" downto  "<< wFX+wFY+2 <<");"<<endl; 
			   o<<tab<<"ResultException   <= exception_synch when temp_exp_concat_fract("<<wEX+wFX+wFY + 2 <<")='0' else"<<endl;
			   o<<tab<<"		                 \"11\";"<<endl;			
		   }
	      else{
			   zeros.str("");//initialization
	         zeros<< zero_generator((wFR+1) - (wFX+wFY+2), 0 );
            
            if (wFX+wFY+2 == wFR+1)
				   o<<tab<<"ResultSignificand <= significand_synch;"<<endl;
            else 	
				   o<<tab<<"ResultSignificand <= significand_synch & "<<zeros.str()<<";"<<endl;
					
            o<<tab<<"ResultExponent <= exponent_synch;"<<endl;
				o<<tab<<"ResultException <= exception_synch;"<<endl;
         }				
    
         o<<tab<< "ResultSign <= sign_synch;"<<endl;
   }//end else not normalized
       
 }//end if pipelined
 else
 {//the combinational version
 
      o<<tab<<"exponentX <= X("<<wEX + wFX -1<<" downto "<<wFX<<");"<<endl; 
	   o<<tab<<"exponentY <= Y("<<wEY + wFY -1<<" downto "<<wFY<<");"<<endl<<endl;
	
	   //Add exponents -> put exponents sum into register
	   o<<tab<<"Exponents_Sum <= (\"0\" & exponentX) + (\"0\" & exponentY);"<<endl; //wEX+1 bits
	   o<<tab<<"bias <= CONV_STD_LOGIC_VECTOR("<<bias_val<<","<<wEX+1<<");"<<endl; 
		
      //substract the bias value from the exponent sum
      o<<tab<<"exponents_sum_minus_bias_ext <= Exponents_Sum - bias;"<<endl;//wEX + 1   
	   o<<tab<<"Exponents_Sum_Minus_Bias <= exponents_sum_minus_bias_ext("<<wEX-1<<" downto 0);"<<endl; //wEX
   
      //build significands 	 	
      o<<tab<< "significandX <= \"1\" & X("<<(wFX-1)<<" downto 0);"<<endl;
  	   o<<tab<< "significandY <= \"1\" & Y("<<(wFY-1)<<" downto 0);"<<endl<<endl;
	 	
	   //multiply significands 
		o<<tab<< "int_multiplier_component: " << intmult->unique_name << endl;
		o<<tab<< "      port map ( X => significandX, " << endl; //wFX+1 bits (1 bit is the hidden 1)
		o<<tab<< "                 Y => significandY, " << endl; //wFY+1 bits
		o<<tab<< "                 R => significand_multiplication " << endl; //wFX+wFY+2 bits
		o<<tab<< "               );" << endl<<endl;
	
	   //Concatenate the exception bits of the two input numbers
		   o<<tab<<"exception_selector <= X("<<wEX + wFX +2<<" downto "<<wEX + wFX + 1<<") & Y("<<wEY + wFY +2<<" downto "<<wEY + wFY +1<<");"<<endl<<endl;
		
      //select proper exception with selector			
      o<<tab<<"process (exception_selector)"<<endl;
	   o<<tab<<"begin"<<endl;
	   o<<tab<<"case (exception_selector) is"<< endl;
	   o<<tab<<tab<<"when \"0000\" => Result_Exception <= \"00\";   "<<endl;
	   o<<tab<<tab<<"when \"0001\" => Result_Exception <= \"00\";   "<<endl;
	   o<<tab<<tab<<"when \"0010\" => Result_Exception <= \"11\";   "<<endl;
	   o<<tab<<tab<<"when \"0011\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"0100\" => Result_Exception <= \"00\";   "<<endl;
		o<<tab<<tab<<"when \"0101\" => Result_Exception <= \"01\";   "<<endl;//to see
		o<<tab<<tab<<"when \"0110\" => Result_Exception <= \"10\";   "<<endl;
		o<<tab<<tab<<"when \"0111\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1000\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1001\" => Result_Exception <= \"10\";   "<<endl;
		o<<tab<<tab<<"when \"1010\" => Result_Exception <= \"10\";   "<<endl;
		o<<tab<<tab<<"when \"1011\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1100\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1101\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1110\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when \"1111\" => Result_Exception <= \"11\";   "<<endl;
		o<<tab<<tab<<"when others =>   Result_Exception <= \"11\";   "<<endl;    
		o<<tab<<"end case;"<<endl;
		o<<tab<<"end process;"<<endl<<endl;

      //use MSB of exponents_sum_minus_bias_ext to siagnal overflow		
      o<<tab<<"Result_Exception_With_EA <= Result_Exception when exponents_sum_minus_bias_ext("<< wEX<<")='0' else"<<endl;
	   o<<tab<<"				                    \"11\";"<<endl<<endl;
										
		//normalize result
      //assign selector signal to MSB of signif. multiplic 
      o<<tab<<"mult_selector <= significand_multiplication ("<<wFX+wFY+1<<");"<<endl;
	   
	   o<<tab<<"exponent_p <= (\"0\" & Exponents_Sum_Minus_Bias ) + (CONV_STD_LOGIC_VECTOR(0 ,"<<wEX<<") & mult_selector);"<<endl;
	   
	   //signal the exception
	   o<<tab<<"exception_upd <= Result_Exception_With_EA when exponent_p("<<wEX<<")='0' else"<<endl;
	   o<<tab<<"                \"11\";"<<endl;
   
      //round result
      //check is rounding is needed
      if (1+wFR >= wFX+wFY+2) 
      {
            //=>no rounding needed - eventual padding
            //generate zeros of length 1+wFR - wFX+wFY+2 
            zeros1.str("");
            zeros1 << zero_generator(1+wFR - wFX+wFY+2 , 0);
            
            //make the outputs
            o<<tab<<"ResultExponent <= exponent_p("<<wEX - 1<<" downto 0);"<<endl;  
            o<<tab<<"ResultSignificand <= significand_multiplication & "<<zeros1.str()<<" when mult_selector='1' else"<<endl;
            o<<tab<<"                     significand_multiplication("<<wFX+wFY<<" downto 0) & "<<zeros1.str()<<" & \"0\";"<<endl;
            o<<tab<<"ResultException <= exception_upd;"<<endl;
            o<<tab<<"ResultSign <= X("<<wEX + wFX<<") xor Y("<<wEY + wFY<<");"<<endl;
         
      }
      else
      {
	      //check if in the middle of two FP numbers
	      
	      //generate two xor strings
	      str1.str(""); str2.str(""); //init
	      str1<<"\"1"<< zero_generator( wFX+wFY+2 - (1+wFR) -1, 1);
	      str2<<"\"01"<< zero_generator( wFX+wFY+2 - (1+wFR) -1-1, 1);
	      
	      zeros1.str("");
	      zeros1<< zero_generator( wFX+wFY+2 - (1+wFR) , 0);
	      
	      o<<tab<<"check_string <= (significand_multiplication ("<< wFX+wFY+2 - (1+wFR) -1<<" downto 0) xor "<<str1.str()<<") when mult_selector='1' else"<<endl;
	      o<<tab<<"               ( (\"0\" & significand_multiplication ("<< wFX+wFY+2 - (1+wFR) -1-1<<" downto 0)) xor "<<str2.str()<<");"<<endl;
	      
	      o<<tab<<"process(check_string)"<<endl;
         o<<tab<<"begin"<<endl;
         o<<tab<<tab<<"      if ( "<<zeros1.str()<<" = check_string ) then"<<endl; 
         o<<tab<<tab<<"         middle_output <= '1';"<<endl;
         o<<tab<<tab<<"      else "<<endl;
         o<<tab<<tab<<"         middle_output <= '0';"<<endl;
         o<<tab<<tab<<"      end if;"<<endl;
         o<<tab<<tab<<"end process; "<<endl;
	      
	      o<<tab<<"exponent_p2 <= exponent_p("<<wEX-1<<" downto 0);"<<endl;
		  	      
	      o<<tab<<"reunion_signal <= (\"0\" & exponent_p2 & significand_multiplication("<< wFX+wFY<<" downto "<<wFX+wFY - (wFR) <<" )) when mult_selector='1' else"<<endl;
	      o<<tab<<"                  (\"0\" & exponent_p2 & significand_multiplication("<< wFX+wFY-1<<" downto "<<wFX+wFY - (wFR) -1<<" ));"<<endl; 
	     
         //               
         //significand_synch_tb2 = LSB of the wFR part of the significand_multiplication 
         o<<tab<<"significand_synch_tb2 <= significand_multiplication("<< wFX+wFY+1 - wFR<<" ) when  mult_selector='1' else"<<endl;
         o<<tab<<"                         significand_multiplication("<< wFX+wFY+1 - wFR -1<<" );"<<endl;              
            		     
         //add 1 to the reunited signal for rounding & normalize purposes
         o<<tab<<"reunion_signal_p <= reunion_signal + CONV_STD_LOGIC_VECTOR(1, "<< 2+ wEX + wFR<<");"<<endl;
         
            		     
         o<<tab<<"middle_signal <= reunion_signal_p when significand_synch_tb2 = '1' else"<<endl;
         o<<tab<<"                 reunion_signal;"<<endl;    		     
            		     
         o<<tab<<"reunion_signal_p_upd <= reunion_signal_p when middle_output='0' else"<<endl;
	      o<<tab<<"                        middle_signal;"<<endl;
	                             
	      //update exception                       
	      o<<tab<<"ResultException <= exception_upd when reunion_signal_p_upd("<<wEX + wFR+1<<")='0' else"<<endl;
	      o<<tab<<"                   \"11\";"<<endl;                                                   
	      
	      o<<tab<<"ResultExponent <= reunion_signal_p_upd("<<wEX + wFR<<" downto "<<wFR+1<<");"<<endl;  
         o<<tab<<"ResultSignificand <= \"1\" & reunion_signal_p_upd("<<wFR<<" downto 1);"<<endl;
         o<<tab<<"ResultSign <= X("<<wEX + wFX<<") xor Y("<<wEY + wFY<<");"<<endl;
         
      }

   
           
       
 }   
  
  
  
  o<< "end architecture;" << endl << endl;
}













 
string Multiplier::zero_generator(int n, int margins)
{
  ostringstream left,full, right, zeros;
  int i;
  //margins=-2 - no quotes generated on string
  //        -1 - left quote generated "XXXX
  //         0 - both quotes generated "XXXX"
  //         1 - right quote generated

  for (i=1; i<=n;i++)
  	zeros<<"0";

  left<<"\""<<zeros.str();
  full<<left.str()<<"\"";
  right<<zeros.str()<<"\"";

  switch(margins){
    case -2: return zeros.str(); break;
    case -1: return left.str(); break;
    case  0: return full.str(); break;
    case  1: return right.str(); break;
    default: return full.str();
  }
}





    /*
		  //Add the hidden bit to the fractional parts of the inputs to obtain
		  //the significands of the two numbers 
		  o <<tab << "significandX <= \"1\" & X("<<(wFX-1)<<" downto 0);"<<endl;
		  o <<tab << "significandY <= \"1\" & Y("<<(wFY-1)<<" downto 0);"<<endl<<endl;
 	 	  		
		  //multiply significands
		  o <<tab<< "int_multiplier_component: " << intmult->unique_name << endl;
  		o <<tab<< "      port map ( X => significandX, " << endl; //wFX+1 bits 
  		o <<tab<< "                 Y => significandY, " << endl; //wFY+1 bits
  		o <<tab<< "                 R => significand_multiplication, " << endl; //wFX+wFY+2 bits
  		o <<tab<< "                 clk => clk, " << endl;
  		o <<tab<< "                 rst => rst " << endl;
  		o <<tab<< "               );" << endl << endl;
	 
		  o<<"exponentX<=X("<<wEX + wFX -1<<" downto "<<wFX<<");"<<endl; 
		  o<<"exponentY<=Y("<<wEY + wFY -1<<" downto "<<wFY<<");"<<endl;
		
		  //place exponents sum into register
		  o<<"Exponents_Sum <= (\"0\" & exponentX) + (\"0\" & exponentY);"<<endl; //wEX+1 bits
		  o<<"bias<=CONV_STD_LOGIC_VECTOR("<<bias_val<<","<<wEX+1<<");"<<endl; //the bias value
		  //substract the bias value from the previous addition result
      o<<"exponents_sum_minus_bias_ext<= Exponents_Sum_d - bias;"<<endl;//wEX + 1 
        	
      //TODO signal overflow if this MSB of exponents_sum_minus_bias_ext is 1
		
      o<<"Exponents_Sum_Minus_Bias <= exponents_sum_minus_bias_ext("<<wEX-1<<" downto 0);"<<endl; //wEX
      

  		//at this level the 2 levels of registers of the IntMultiplier and the addition+substraction of the 
  		//exponents are synchronized

  		//the test bit is the most significand bit of the result
  		//it will be used to check if: 
  		// 1. it will be necessary to normalize the result 
  		// 2. to what bit of this significand_multiplication we need to add a 1 for rounding
  		o<<"significand_multiplication_test_bit <= significand_multiplication("<<wEX+wEY+1<<");"<<endl;
  	
  		//add 1 to the previous opperations over exponents
  		o<<"Exponents_Sum_Minus_Bias_plus1<=Exponents_Sum_Minus_Bias_d + 1;"<<endl; //wEX bits
  		
  		//make mux 2:1 ->> outputs of wEX bits
  		o<<"exponent_mux1 <= Exponents_Sum_Minus_Bias_d WHEN significand_multiplication_test_bit ='1' ELSE "<<endl;
          	o<<"                 Exponents_Sum_Minus_Bias_plus1;"<<endl;
  		
  		//part of the integer multiplication results  (wFR + 2 bits)
  		o<<"result_int_mult_part1 <= significand_multiplication("<<(wFX + wFY +1)<<" downto "<<(wFX + wFY +1)-(wFR + 2)+1<<" );"<<endl;
  		o<<"result_int_mult_part2 <= significand_multiplication("<<(wFX + wFY   )<<" downto "<<(wFX + wFY   )-(wFR + 2)+1<<" );"<<endl;
  	
  		//add 1 to each of the previous two signals (wFR + 3 bits)
  		o<<"result_int_mult_part1_plus1 <= (\"0\" & result_int_mult_part1) + 1;"<<endl;
  		o<<"result_int_mult_part2_plus1 <= (\"0\" & result_int_mult_part2) + 1;"<<endl;
  	
  		//make mux 2:1 to select the correct addition result out of the two (wFR + 3 bits)
  		o<<"integer_mux1  <= result_int_mult_part1_plus1 WHEN significand_multiplication_test_bit ='1' ELSE "<<endl;
          	o<<"                 result_int_mult_part2_plus1;"<<endl;
  	
  		//insert register level
  		o<<"exponent_register2 <= exponent_mux1;"<<endl;
  		o<<"integer_register2 <= integer_mux1;"<<endl; //wFR + 3 bits
  		
  		//make an adder
  		o<<"exponent_register2_plus1 <= exponent_register2_d + 1;"<<endl;
  	
          	//select test bit from MSB of integer_register2_d
  		o<<"test_bit2 <= integer_register2_d("<<wFR + 2<<");"<<endl;
  	
  		//mux 2:1 wEX bits
  		o<<"exponent_mux3 <= exponent_register2_d WHEN test_bit2 ='1' ELSE "<<endl;             
          	o<<"                 exponent_register2_plus1;"<<endl;
  	
  		//mux 2:1 for proper significand output
  		o<<"mux_significand_output <= integer_register2_d("<<wFR + 2<<" downto 2) when test_bit2 ='1' ELSE "<<endl;
          	o<<"             	      integer_register2_d("<<wFR + 1<<" downto 1);"<<endl;		 
   	
  		o<< "ResultExponent<= exponent_mux3;"<<endl;  
          	o<<"ResultSignificand<=mux_significand_output;"<<endl;  
  	
    		//compute sign
    		o<< "result_sign<=X("<<wEX + wFX<<") xor Y("<<wEY + wFY<<");"<<endl;
    		
    		//compute exception bits Result_Exception
    		o<<"exception_selector<=X("<<wEX + wFX +2<<" downto "<<wEX + wFX + 1<<") & Y("<<wEY + wFY +2<<" downto "<<wEY + wFY +1<<");"<<endl;

   		o<<" process (exception_selector)"<<endl;
    		o<<" begin"<<endl;
    		o<<" case (exception_selector) is"<< endl;
    			o<<"    when \"0000\" => Result_Exception <= \"00\";   "<<endl;
    			o<<"    when \"0001\" => Result_Exception <= \"00\";   "<<endl;
    			o<<"    when \"0010\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"0011\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"0100\" => Result_Exception <= \"00\";   "<<endl;
    			o<<"    when \"0101\" => Result_Exception <= \"01\";   "<<endl;//to see
    			o<<"    when \"0110\" => Result_Exception <= \"10\";   "<<endl;
    			o<<"    when \"0111\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"1000\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"1001\" => Result_Exception <= \"10\";   "<<endl;
    			o<<"    when \"1010\" => Result_Exception <= \"10\";   "<<endl;
    			o<<"    when \"1011\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"1100\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"1101\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"1110\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when \"1111\" => Result_Exception <= \"11\";   "<<endl;
    			o<<"    when others =>   Result_Exception <= \"11\";   "<<endl;    
    		o<<" end case;"<<endl;
    		o<<" end process;"<<endl;

  	//TODO Rewrite code above
  	//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO
  	//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO
  	//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO
    */

