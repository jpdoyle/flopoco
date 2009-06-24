#ifndef __TESTCASE_HPP
#define __TESTCASE_HPP

#include <string>
#include <vector>
#include <map>

#include "Signal.hpp"
#include "FPNumber.hpp"

using namespace std;


/**
A test case is a mapping between I/O signal names and boolean values
given as mpz.

The signal names must be those of Operator->iolist_. Whether several
possible output values are possible is stored in the
numberOfPossibleValues_ attribute of the corresponding Signal stored in iolist, and
only there.

The emulate() function of Operator takes a partial test case (mapping
all the inputs) and completes it by mapping the outputs.
 * @see TestBench
 * @see Operator
 */

class Operator;
class TestCaseList;

class TestCase {
public:

	/** Creates an empty TestCase for operator op */
	TestCase(Operator* op);
	~TestCase();

	/**
	 * Adds an input for this TestCase
	 * @param s The signal which will value the given value
	 * @param v The value which will be assigned to the signal
	 */
	void addInput(string s, mpz_class v);
	/**
	 * Adds an input for this TestCase
	 * @param s The signal which will value the given value
	 * @param v The value which will be assigned to the signal
	 */
	void addInput(string s, FPNumber::SpecialValue v);

	/**
	 * Adds an input for this TestCase
	 * @param s The signal which will value the given value
	 * @param x the value which will be assigned to the signal, provided as a double.
	 * (use this method with care) 
	 */
	void addInput(string s, double x);

	/**
	 * recover the mpz associated to an input
	 * @param s The name of the input
	 */
	mpz_class getInputValue(string s);

	/**
	 * Adds an expected output for this signal
	 * @param s The signal for which to assign an expected output
	 * @param v One possible value which the signal might have
	 */
	void addExpectedOutput(string s, mpz_class v);

	/**
	 * Adds a comment to the output VHDL. "--" are automatically prepended.
	 * @param c Comment to add.
	 */
	void addComment(std::string c);

	/**
	 * Generates the VHDL code necessary for assigning the input signals.
	 * @param prepend A string to prepend to each line.
	 * @return A multi-line VHDL code.
	 */
	std::string getInputVHDL(std::string prepend = "");

	/**
	 * Generate the VHDL code necessary to assert the expected output
	 * signals.
	 * @param prepend A string to prepend to each line.
	 * @return A single-line VHDL code.
	 */
	std::string getExpectedOutputVHDL(std::string prepend = "");

	/**
	 * Return the comment string for this test case
	 * @return A single-line VHDL code.
	 */
	std::string getComment();


private:
	Operator *op_;                       /**< The operator for which this test case is being built */

	map<string, mpz_class>          inputs;
	map<string, vector<mpz_class> >   outputs;

	string comment;

};



/**
 * Represents a list of test cases that an Operator has to pass.
 */
class TestCaseList {
public:
	/**
	 * Creates an empty TestCaseList
	 * @see TestCase
	 */
	TestCaseList();
	
	/** Destructor */
	~TestCaseList();

	/**
	 * Adds a TestCase to this TestCaseList.
	 * @param t TestCase to add
	 */
	void add(TestCase* t);

	/**
	 * Get the number of TestCase-es contained in this TestCaseList.
	 * @return The number of TestCase-es
	 */
	int getNumberOfTestCases();

	/**
	 * Get a specific TestCase.
	 * @param i Which TestCase to return
	 * @return The i-th TestCase.
	 */
	TestCase * getTestCase(int i);

private:
	/** Stores the TestCase-es */
	vector<TestCase*>  v;

};


#endif
