/*
  A class to manage heaps of weighted bits in FloPoCo
  
  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Author : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2012.
  All rights reserved.

*/
#include "BitHeap.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>	

#include "utils.hpp"
#include "IntAdder.hpp"
#include "IntAddition/BasicCompressor.hpp"
#include<vector>
#include<list>

using namespace std;

/*

*/

namespace flopoco{

	BitHeap::WeightedBit::WeightedBit(BitHeap* bh, int weight_, int cycle_, double criticalPath_) : 
		bh(bh), weight(weight_)
	{
		srcFileName=bh->getOp()->getSrcFileName() + ":BitHeap:WeightedBit";
		if(cycle_==-1)
			cycle=bh->getOp()->getCurrentCycle();
		else
			cycle=cycle_;
		if(criticalPath_==-1)
			criticalPath=bh->getOp()->getCriticalPath();
		else
			criticalPath=criticalPath_;
		std::ostringstream p;
		p  << "heap_w" << weight << "_" << bh->getUid(weight);
		name=p.str();

	}

	
	/* which bit was defined earlier */
	bool BitHeap::WeightedBit::operator< (const WeightedBit& b){
		if ((cycle<b.cycle) || (cycle==b.cycle && criticalPath<b.criticalPath)) 
			return true;
		else
			return false;
	} 

	bool BitHeap::WeightedBit::operator<= (const WeightedBit& b){
		if ((cycle<b.cycle) || (cycle==b.cycle && criticalPath<=b.criticalPath)) 
			return true;
		else
			return false;
	} 
	
	double BitHeap::WeightedBit::getCriticalPath(int atCycle){
		if (cycle>atCycle){
			THROWERROR("For bit " << name << ", getCriticalPath() called for cycle "<<atCycle<< " but this bit is defined only at cycle "<< cycle);
		}
		if (cycle==atCycle)
			return criticalPath;
		if (cycle<atCycle)
			return 0.0;
		
		return 0.0;  //because it returned no value on this branch
	}


	BitHeap::BitHeap(Operator* op, int maxWeight) :
		op(op), maxWeight(maxWeight)
	{
		// Set up the vector of lists of weighted bits, and the vector of uids
		srcFileName=op->getSrcFileName() + ":BitHeap"; // for REPORT to work
		REPORT(DEBUG, "Creating BitHeap of size " << maxWeight);
		for (int i=0; i< maxWeight; i++) {
			uid.push_back(0);
			list<WeightedBit*> t;
			bits.push_back(t);
		}
	}	


	BitHeap::~BitHeap()
	{
		for(unsigned i=0; i<bits.size(); i++){
			list<WeightedBit*>& l = bits[i];
			list<WeightedBit*>::iterator it;
			for(it=l.begin(); it!=l.end(); it++){
				delete (*it);
			}
		}
	}	


	int BitHeap::getUid(unsigned w){
		return uid[w]++;
	}	



	void  BitHeap::addBit(unsigned w, string rhs, string comment){
		WeightedBit* bit= new WeightedBit(this, w) ;
		list<WeightedBit*>& l=bits[w];

		//insert so that the list is sorted by bit cycle/delay
		list<WeightedBit*>::iterator it=l.begin(); 
		bool proceed=true;
		while(proceed) {
			if (it==l.end() || (**it <= *bit)){ // test in this order to avoid segfault!
				l.insert(it, bit);
				proceed=false;
			}
			else 
				it++;
		}
		// now generate VHDL
		op->vhdl << tab << op->declare(bit->getName()) << " <= " << rhs << ";";
		if(comment.size())
			op->vhdl <<  " -- " << comment;
		op->vhdl <<  endl;			
	};





	string adderBitName(int c, int bit, int h) {
		ostringstream p;
		p  << "adderBit" << c << "_" << bit << "_" << h;
		return p.str();
	};



	unsigned BitHeap::currentHeight(unsigned w) {
		int h=0;
		list<WeightedBit*>& l = bits[w];
#if 0
		list<WeightedBit*>::iterator it;
		for(it=l.begin(); it!=l.end(); it++){
			if ((*it)->todo())
				h++;
		}
#else
		h=l.size();
#endif
		return h;
	}
	


	int BitHeap::count(list<WeightedBit*> wb)
	{
		int bitsToCompress=0;
		for (list<WeightedBit*>::iterator iter = wb.begin(), end = wb.end(); iter != end; ++iter)
		{
			if ((*iter)->todo()==true) // & have minimum cycle value
    			{
    				bitsToCompress++;
    			}
		}
		return bitsToCompress;
	}

	void BitHeap::reduce(int c, int red)
	{
		list<WeightedBit*>::iterator iter = bits[c].begin(), end=bits[c].end();
		
		for (list<WeightedBit*>::iterator iter = bits[c].begin(), end = bits[c].end(); iter != end; ++iter)
		{
			if ((*iter)->todo()==true) // & have minimum cycle value
    			{
    				if(red>0)
    				{
    					(*iter)->done();
    					red--;
    				}
    			}
		}
	}


	// There are two distinct notions: compression stages, and compression cycles. Several stages can fit in one cycle.

	void BitHeap::generateCompressorVHDL()
	{
		Target* target=op->getTarget();

		op->vhdl << tab << "-- Beginning of code generated by BitHeap::generateCompressorVHDL" << endl;
		
		unsigned n = target->lutInputs();
		unsigned w;
		
		vector<BasicCompressor*> compressors (n+1, (BasicCompressor*) 0);
		
		unsigned chunkDoneIndex=0;
		//unsigned stage=0;
		//bool isPipelined=target->isPipelined();

		REPORT(DEBUG, "maxWeight="<< maxWeight);
		REPORT(DEBUG, "Column height before compression");
		for (w=0; w<bits.size(); w++) {
			REPORT(DEBUG, "   w=" << w << ":\t height=" << bits[w].size()); 
		}		


		/*
		bool moreToCompress=false; // Here "compress" means "compress or add" ie height >=2
		// find the cycle at which the compressor must begin to work
		
		
		int minCycle=1<<30; // should be enough for anybody
		for (w=0; w<bits.size(); w++) {
			if(bits[w].size()!=0) {
				// Since the list is ordered, it is enough to consider the first bit of the list
				WeightedBit* b= *(bits[w].begin());
				int cycle=b->getCycle();
				if (cycle<minCycle)
					minCycle = cycle;
			}
		}
		op->setCycle(minCycle);
		REPORT(DEBUG, "Compressor begins work at cycle "<<minCycle);
		*/
		// is there a rightmost part that is already compressed?
		
		REPORT(DEBUG, "Checking whether rightmost columns need compressing");
		bool alreadyCompressed=true;
		w=0;
		while(w<bits.size() && alreadyCompressed) 
		{
			//REPORT(DEBUG, "Current height for level " << w << " is " << currentHeight(w));
			
			if(currentHeight(w)>1)
			{
				alreadyCompressed=false;
			}
			else
			{
				REPORT(DEBUG, "Level " << w << " is already compressed; will go directly to the final result");
				w++;
			}
		}
		
		
		
		//FIXME Two poorly done concatenations here, must be checked after the compression is fully done
		if(w!=0)
		{
			op->vhdl << tab << op->declare(join("tempR", chunkDoneIndex), w, true) << " <= " ;
			for(int i=w-1; i>=0; i--) {
				REPORT(DEBUG, "Current height " << currentHeight(i)<<" of " << i);
				if(currentHeight(i)==1) 
				{
					op->vhdl << (bits[i].front())->getName()   << " & ";
					bits[i].pop_front();
				}
				else
				{
					op->vhdl << "'0' & ";
				}	
			}
			op->vhdl << " \"\"; -- already compressed" << endl; 
			chunkDoneIndex++;			
		}
		
		//TODO what if the column has only 2 bits? 
		
		minWeight=w;

		
		//Build a vector of basic compressors that are fit for this target
		//Size 10 should cover all possible compressors at this time
		vector<BasicCompressor *> possibleCompressors (0, (BasicCompressor*) 0);
		
		//Not very elegant, but should work
		int maxCompressibleBits, col0, col1;
		maxCompressibleBits = target->lutInputs();
		REPORT(DEBUG, maxCompressibleBits);
		
		//Generate all "workable" compressors for 2 columns, on this target, descending on fitness function
		for(col0=maxCompressibleBits; col0>=3; col0--)
			for(col1=col0; col1>=0; col1--)
				if(col0 + 2*col1<=maxCompressibleBits)
				{
					vector<int> newVect;
					newVect.push_back(col1);
					newVect.push_back(col0);
					possibleCompressors.push_back(new BasicCompressor(target, newVect));
				}
		

		
		
		
		//Remaining structure must be compressed
		unsigned j;

		for(unsigned i=minWeight; i<maxWeight; i++)
		{

			j=0;
			//REPORT(DEBUG,"Good until here");

			//REPORT(DEBUG, possibleCompressors.size());

			//REPORT(DEBUG, "count bits " << count(bits[i]));

			while(count(bits[i]) > possibleCompressors[j]->getColumn(0))
			{
				//REPORT(DEBUG, "In while loop 1");
				
				compressors.push_back(possibleCompressors[j]) ;
				REPORT(DEBUG,"Using Compressor " << j <<", reduce column "<<i);
				reduce(i, possibleCompressors[j]->getColumn(0));

				
			}

			++j;

			int remainingBits=count(bits[i]);
            
			while((j<possibleCompressors.size())&&(count(bits[i])>2))
			{
				//REPORT(DEBUG, "In while loop 2");
				
				//REPORT(DEBUG, "remainingBits " << remainingBits);
				//REPORT(DEBUG, "compressing with " << j << ", column0 = " << possibleCompressors[j]->getColumn(0) << 
				//	", column1 = " << possibleCompressors[j]->getColumn(1));

				if(count(bits[i])==possibleCompressors[j]->getColumn(0))
				{
					if(possibleCompressors[j]->getColumn(1)!=0)
					{
						if((count(bits[i+1])>=possibleCompressors[j]->getColumn(1))&&(i<bits.size()-1))
						{
							REPORT(DEBUG,"Using Compressor " << j <<", reduce columns "<<i<<" and "<<i+1);
							compressors.push_back(possibleCompressors[j]);
							reduce(i,possibleCompressors[j]->getColumn(0));
							reduce(i+1,possibleCompressors[j]->getColumn(1));
						}
						else 
							++j;
					}
					else
					{
						REPORT(DEBUG,"Using Compressor " << j <<", reduce column "<<i);
						compressors.push_back(possibleCompressors[j]);
						reduce(i,possibleCompressors[j]->getColumn(0));
						++j;
					}

				}
				else
					++j;
			}

			
			







			//while(bits[i].)
		}  
		// end while(moreToCompress)
		
		/*
#if 0 // TODO redo from scratch
		while(moreToCompress){
			REPORT(DETAILED, "Stage "<< stage << ", cycle " << op->getCurrentCycle() << ": Bits smaller than "<<minWeight<< " are already compressed");
			// The rightmost weights (weight<minWeight) are already of height 1 or 0

			///////////////////////////////////////////////////////////////////////////////////////////
			// First consume as many bits as possible on the right:
			// All those bits which are of height 2 or less go to an adder

			double adderOutCP =  target->adderDelay(0); // the critical path below this bit will be initialized below.
			double targetCP;
			if(isPipelined)
				targetCP=1.0/target->frequency() - target->ffDelay()- target->localWireDelay(); //   
			else
				targetCP=3600; // 1 hour, should enable even milion-bit precision on Zuse's machine

			bool adderBusiness=true;
			w=minWeight-1; // -1 because it is ++ed only once, at the beginning of the loop 

			REPORT(DETAILED, "Stage "<< stage << ", cycle " << op->getCurrentCycle() << ": looking for an LSB adder for targetCP=" << targetCP);
			string carryBitName;

			while(adderBusiness){
				w++;
				unsigned carryBit;
				if(w==minWeight)
					carryBit=1; // The first addition bit may input a carry 
				else
					carryBit=0;  

				if(currentHeight(w) > 2+carryBit ||  w>=maxWeight )
					adderBusiness=false;
				else { // we consider adding this bit to an adder
					// find the max CP among the 0, 1 or two bits
					double maxCP=0; 
					for(h=0; h<cycle[w].size(); h++) {
						// The bits of cycles < currentCycle have CP=0 anyway
						if( (cycle[w][h] == op->getCurrentCycle()) && (cpd[w][h] > maxCP) ) 
							maxCP=cpd[w][h];
					}
					if(carryBit==1)
						adderOutCP+=maxCP; // finish its initialization properly
					// now check that adding that bit to the current adder doesn't overshoot the allowable CP.
					// The critical path is either to add 1 bit of carry propagation to the previous adder CP,
					// or to add one LUT delay to the CPs at input of this bit
					double cp1 = adderOutCP + target->adderDelay(2)-target->adderDelay(1);
					double cp2 = maxCP +  target->lutDelay() ; //  +target->localWireDelay()
					double cp = max(cp1, cp2);
					
					if(cp<targetCP) {
						adderOutCP=cp;
						REPORT(DEBUG, "w=" << w << ": maxCP="<< maxCP << " adderOutCP=" << adderOutCP);
						int i=0; // should go up to 1 max
						// look for the bits to add
						for(h=0; h<cycle[w].size(); h++) {
							if( (cycle[w][h] <= op->getCurrentCycle()) && (cycle[w][h] >= 0) ) { // there should be only two of these
								op->vhdl << tab << op->declare(adderBitName(op->getCurrentCycle(), w-minWeight, i)) << " <= " << bit(w, h) << ";";
								if(i==2) {
									op->vhdl << " -- carry in bit"; 
									carryBitName = adderBitName(op->getCurrentCycle(), w-minWeight, i);
								}
								op->vhdl << endl;
								i++;
							}
						}
						// If there was only one bit, add the second
						if(i==1)
							op->vhdl << tab << op->declare(adderBitName(op->getCurrentCycle(), w-minWeight, 1)) << " <= '0';" << endl;
						
						// if this is the first bit of the adder and there was no carry in, add it
						if(carryBit==1 && i<3) {
							op->vhdl << tab << op->declare(adderBitName(op->getCurrentCycle(), w-minWeight, 2)) << " <= '0'; -- carry in bit" << endl;
							carryBitName = adderBitName(op->getCurrentCycle(), w-minWeight, 2);
						}
					} // if(cp<targetCP)
					else {
						adderBusiness=false; // for a CP reason: there might be more bits we could add at the same cycle behind (à la IntAdderAlternative)
						// TODO An optimization here ?
					}
				} // end else (currentHeight>2)
			} // end while(adderBusiness)
			
			int adderSize = w-minWeight;
			if(adderSize!=0){

				REPORT(DETAILED, "Stage "<< stage << ", cycle " << op->getCurrentCycle() << ": found adder from bit " << minWeight << " to bit " << w 
				       <<", adderSize=" << adderSize);

				// Now actually build the adder, and update minWeight	
				op->vhdl << tab << op->declare(join("addInput0_", op->getCurrentCycle()), adderSize+1) << " <= '0'";  
				for (int i=adderSize-1; i>=0; i--)
					op->vhdl << " & " << adderBitName(op->getCurrentCycle(), i, 0);
				op->vhdl << ";" << endl;
				
				op->vhdl << tab << op->declare(join("addInput1_", op->getCurrentCycle()), adderSize+1) << " <= '0'";  
				for (int i=adderSize-1; i>=0; i--)
					op->vhdl << " & " << adderBitName(op->getCurrentCycle(), i, 1);
				op->vhdl << ";" << endl;
			
				op->vhdl << tab << op->declare(join("addRes_", op->getCurrentCycle()), adderSize+1) << " <= " 
				         << join("addInput0_", op->getCurrentCycle()) << " + " << join("addInput1_", op->getCurrentCycle()) << " + " << carryBitName << ";" << endl;
				// add the result bits to the output, and the carry out to the heap
				op->vhdl << tab << op->declare(join("tempR", chunkDoneIndex), adderSize, true) << " <= " 
				         << join("addRes_", op->getCurrentCycle()) << range(adderSize-1, 0) << "; -- compression done for these bits" << endl; 
				chunkDoneIndex++;
				ostringstream rhs;
				rhs << join("addRes_", op->getCurrentCycle()) << of(adderSize); 
				if(adderSize+minWeight < maxWeight)
					addBit(adderSize+minWeight, rhs.str(), "add the carry out to the heap");
				minWeight = w;
				if(minWeight>=cycle.size())
					moreToCompress=false;
			}

			REPORT(DETAILED, "Next Cycle");
			op->nextCycle(); // There should be a loop to determine if we have to call nextCycle()
			stage++;
		} // end while(moreToCompress)
#endif
			
			
			
	*/
		op->vhdl << tab << "-- concatenate all the compressed chunks" << endl;
		op->vhdl << tab << op->declare("CompressionResult", maxWeight) << " <= " ;
		for(int i=chunkDoneIndex-1; i>=0; i--)
			op->vhdl << join("tempR", i) << " & ";
		op->vhdl << " \"\" ;" << endl;
		op->vhdl << tab << "-- End of code generated by BitHeap::generateCompressorVHDL" << endl;
	};


}

	


#if 0
			// For the other ones
			bool moreToCompressInThisCycle=false;
			double maxCP = 1.0/target->frequency() - target->lutDelay()- target->localWireDelay();
			while(moreToCompressInThisCycle){
				// go from right to left;
				for (w=0; w<cycle.size(); w++) {

 				// in each column look for at least 3 bits with CP smaller than maxCP

					vector<int> stillToCompress; // pointers to the height
					for (h=0; h<cycle[w].size(); h++){
						if( (cycle[w][h] >= 0) && 
						    ( (cycle[w][h] < op->getCurrentCycle()) ||  
						      ( (cycle[w][h]== op->getCurrentCycle()) && (cpd[w][h] < minCP) ) ) )
							stillToCompress.push_back(h); 
					}

					if(stillToCompress.size()>=3) {
						bool sortInProgress=true;
						while(sortInProgress) {
							// look for the min CP
							double minCP=3600.0;
							for (h=0; h<cycle[w].size(); h++){
								if( (cycle[w][h] >= 0) && ( cycle[w][h] < op->getCurrentCycle()))
								minCP=0; // this one comes from a previous cycle
								else if ( (cycle[w][h]== op->getCurrentCycle()) && (cpd[w][h] < minCP))
									minCP=cpd[w][h];
							}
							
						
					}
				}
					// Still more to compress? Look for bits whose cycle was not set to -1
					for (w=0; w<cycle.size(); w++) {
					int actualHeight=0;
					for(h=0; h<cycle[w].size(); h++) {
						if (cycle[w][h]>=0)
							h++;
					}
					if(h>1)
						moreToCompress=true;
				}
				
				for (w=0; w<cycle.size(); w++){
					if(cycle[w].size()>1)
				moreToCompress=true;
				}
				}
			
#endif

#if 0
		// is there still a column with at least 2 bits?
		for (w=0; w<cycle.size(); w++){
			if(cycle[w].size()>=2)
				moreToCompress=true;
		}

		// find the critical path delay in this cycle at which the compressor must begin to work
		int minCPD=3600.; // 1 hour should be enough for anybody
		for (w=0; w<cycle.size(); w++) {
			for(h=0; h<cycle[w].size(); h++) {
				if (cycle[w][h]==minCycle && cpd[w][h]<minCPD)
					minCPD = cpd[w][h];
			}
		}
		op->setCriticalPath(minCPD);
		REPORT(DETAILED, "Compressor begins work at cycle "<<minCycle << " and critical path delay " << minCPD);
#endif