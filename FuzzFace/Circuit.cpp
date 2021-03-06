#include "Circuit.h"


/*Default Constructor*/
//Circuit::Circuit() { std::cout << "Circuit Created!" << std::endl; };

/*Calls the default constructor with default 44.1k Hz sample rate */
Circuit::Circuit() : Circuit(44100.) {};

/*Constructor which takes sampleRate as an arguement and initialises the sampling period T to 1./sampleRate */
Circuit::Circuit(double sampleRate) :T(1./sampleRate){

		std::cout << "Circuit Created" << std::endl;

		//Perform initial setup of the circuit
		setup();

}

/* Initial setup of default circuit values and parmaters*/
void Circuit::setup() {
	/* Setup */
	//Initialise controllable paramaters
	setVol(defaultVol);
	setFuzz(defaultFuzz);
	//Populate circuit matrices
	populateCircuitMatrices();
	//Initialise the incident matrices
	initialiseIncidentMatrices();
	//Initialise the system matrices
	refreshAll();
}

/* Initial setup of circuit matrices */
void Circuit::populateCircuitMatrices() {
	//Update the resistor values
	r4 = (1 - vol)*500e3;
	r5 = vol*500e3;
	r7 = fuzz*1e3;
	r8 = (1 - fuzz)*1e3;

	//prep the resistor values for input into the diagonal matrix
	resMatrix << 1 / r1, 1 / r2, 1 / r3, 1 / r4, 1 / r5, 1 / r6, 1 / r7, 1 / r8;

	//prep the capacitor values for input into diagonal matrix
	capMatrix << c1, c2, c3;
	capMatrix = (2 * capMatrix) / T;

	//Convert the matrices to diagonal matrices
	diagResMatrix = resMatrix.asDiagonal();
	diagCapMatrix = capMatrix.asDiagonal();

}

/*One time setup of incident matrices, this is performed in the constructor and sets up the incident matrices*/
void Circuit::initialiseIncidentMatrices() {

	//The incident matrix for the resistors
	incidentResistors <<
		0, 0, -1, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, -1, 1, 0, 0, 0, 0,
		0, 0, 0, 1, 0, -1, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 1, -1,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
		0, 1, 0, 0, 0, 0, -1, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 1, -1, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 0, 0;

	//The incident matrix for the Capacitors
	incidentCapacitors <<
		1, -1, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
		0, 0, 0, 0, 0, 1, 0, 0, -1, 0;

	//The incident matrix for Voltage
	incidentVoltage <<
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, -1, 0, 0, 0, 0, 0, 0;

	//The incident matrix for the nonlinearities
	incidentNonlinearities <<
		0, -1, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, -1, 0, 0, 0, 0, 0, 0, 0,
		0, 0, -1, 0, 0, 0, 1, 0, 0, 0,
		0, 0, 0, 0, -1, 0, 1, 0, 0, 0;

	//The incident matrix for the output
	incidentOutput <<
		0, 0, 0, 0, 0, 0, 0, 0, 0, 1;
}

/* Refresh all matrices in the system when fuzz or vol changes*/
void Circuit::refreshAll() {
	refreshCircuitMatrices(); 
	refreshSystemMatrix(); 
	refreshNonLinStateSpace(); 
	refreshNonlinearFunctions();
}

/*Function to populate the circuit matrices*/
void Circuit::refreshCircuitMatrices() {
	
	//Update the resistor values
	r4 = (1 - vol)*500e3;
	r5 = vol*500e3;
	r7 = fuzz*1e3;
	r8 = (1 - fuzz)*1e3;

	//update the variable resistor values
	resMatrix(0, 3) = 1/r4;
	resMatrix(0, 4) = 1/r5;
	resMatrix(0, 6) = 1/r7;
	resMatrix(0, 7) = 1/r8;

	//Convert the matrices to diagonal matrices
	diagResMatrix = resMatrix.asDiagonal();

}

/* Function used to refresh the system matrix, call when fuzz or vol is changed, called by refresh() */
void Circuit::refreshSystemMatrix() {
	
	//Update the matrix systemRes with the resistor element of the system Matrix
	systemRes = (incidentResistors.transpose())*diagResMatrix*incidentResistors;

	//Update the matrix systemCap with the capacitor element of the system Matrix
	systemCap = (incidentCapacitors.transpose())*diagCapMatrix*incidentCapacitors;

	//Construct system matrix
	systemMatrix.block(0, 0, numNodes, numNodes) = systemRes + systemCap;  //sets the first 10x10 top left matrix to be the sum of systemRes and systemCap matrices
	systemMatrix.block(0, numNodes, numNodes, numInputs) = incidentVoltage.transpose();  //sets the 10x2 matrix starting at (0,10) equal to the transpose of incidentVoltage matrix
	systemMatrix.block(numNodes, 0, numInputs, numNodes) = incidentVoltage;  //sets the 2x10 matrix starting at (10,0) equal to the incidentVoltage matrix
	systemMatrix.block(numNodes, numNodes, numInputs, numInputs).setZero();  //sets the last 2x2 matrix in the bottom right corner to 0

}

/* Function used to refrseh the nonlinear state space terms, called by refresh()*/
void Circuit::refreshNonLinStateSpace() {
	/* Padded matrices used in state space term calculations*/
	//Padded Capacitor Matrix
	padC.block(0, 0, numCap, numNodes) = incidentCapacitors;
	padC.block(0, numNodes, numCap, numInputs).setZero();
	
	//Padded NonLinearity Matrix
	padNL.block(0, 0, numNonLin, numNodes) = incidentNonlinearities;
	padNL.block(0, numNodes, numNonLin, numInputs).setZero();

	//Padded output Matrix
	padO.block(0, 0, numOutputs, numNodes) = incidentOutput;
	padO.block(0, numNodes, numOutputs, numInputs).setZero();

	//Padded identity matrix
	padI.block(0, 0, numInputs, numNodes).setZero();
	padI.block(0, numNodes, numInputs, numInputs).setIdentity();

	//Calculate State Space Matrices
	stateSpaceA = 2 * diagCapMatrix*padC*systemMatrix.partialPivLu().solve(padC.transpose()) - Eigen::MatrixXd::Identity(3, 3);  //populate
	stateSpaceB = 2 * diagCapMatrix*padC*systemMatrix.partialPivLu().solve(padI.transpose()); //populate
	stateSpaceC = -2 *diagCapMatrix*padC*systemMatrix.partialPivLu().solve(padNL.transpose()); //populate
	stateSpaceD = padO*systemMatrix.partialPivLu().solve(padC.transpose()); //populate
	stateSpaceE = padO*systemMatrix.partialPivLu().solve(padI.transpose()); //populate
	stateSpaceF = -padO*systemMatrix.partialPivLu().solve(padNL.transpose()); //populate
	stateSpaceG = padNL*systemMatrix.partialPivLu().solve(padC.transpose()); //populate
	stateSpaceH = padNL*systemMatrix.partialPivLu().solve(padI.transpose()); //populate
	stateSpaceK = -padNL*systemMatrix.partialPivLu().solve(padNL.transpose()); //populate
}

//Return the specified state space matrix, takes capital letters only A-K
Eigen::MatrixXd Circuit::getStateSpaceMatrix(std::string input) {

	if (input == "A") { return stateSpaceA; }
	if (input == "B") { return stateSpaceB; }
	if (input == "C") { return stateSpaceC; }
	if (input == "D") { return stateSpaceD; }
	if (input == "E") { return stateSpaceE; }
	if (input == "F") { return stateSpaceF; }
	if (input == "G") { return stateSpaceG; }
	if (input == "H") { return stateSpaceH; }
	if (input == "K") { return stateSpaceK; }
	else {
		std::cout << "Input \"" << input << "\" not recognised, defaulted output is matrix A";
		return stateSpaceA;
	}

}

/* Function used to refresh the nonlinear function matrices, called by refresh() */
void Circuit::refreshNonlinearFunctions() {
	//Input the psi values
	psi << 1 / forwardGain, 1 / reverseGain, 0, 0,
		1, -(reverseGain + 1) / reverseGain, 0, 0,
		0, 0, 1 / forwardGain, 1 / reverseGain,
		0, 0, 1, -(reverseGain + 1) / reverseGain;

	//Input the phi values
	phi << 1, 0, 0, 0,
		1, -1, 0, 0,
		0, 0, 1, 0,
		0, 0, 1, -1;

	//Input the Kd values
	alteredStateSpaceK << -stateSpaceK*psi;

	//Input the M values
	nonLinEquationMatrix = alteredStateSpaceK.inverse()*phi.inverse();
}

//Return the specified nonlinear function matrix
Eigen::MatrixXd Circuit::getNonlinearFunctionMatrix(std::string input) {
	if (input == "psi") { return psi; }
	if (input == "phi") { return phi; }
	if (input == "nonLinEquationMatrix") { return nonLinEquationMatrix; }
	if (input == "alteredStateSpaceK") { return alteredStateSpaceK; }
	else {
		std::cout << "Input \"" << input << "\" not recognised, defaulted output is matrix PSI" << std::endl;
		return psi;
	}
}

/* Create a setter for the Fuzz parameter, when input is outside the allowable range 0 > fuzzVal > 1, default to 0.6 */
void Circuit::setFuzz(double _fuzz) {
	//Checks fuzzVal is within allowable range
	if (_fuzz > 0 && _fuzz < 1) {
		fuzz = _fuzz;
	}
	else {
		//Defaults value to 0.6 and prints a message
		std::cout << "Fuzz out of range, defaulted to " << defaultFuzz << std::endl;
		fuzz = defaultFuzz;
	}
}

/* Returns the current value for the fuzz setting */
double Circuit::getFuzz() 
{
	return fuzz;
}

/* Create a setter for the Vol parameter, when input is outside the allowable range 0 > volVal > 1, default to 0.4 */
void Circuit::setVol(double _vol) {
	//Checks volVal is within allowable range
	if (_vol > 0 && _vol < 1) {
		vol = _vol;
	}
	else {
		//Defaults value to 0.4 and prints a message
		std::cout << "Vol out of range, defaulted to " << defaultVol << std::endl;
		vol = defaultVol;
	}
}

/* Returns the current value for the vol setting*/
double Circuit::getVol() 
{
	return vol;
}

/* Returns the circuit saturation current IS */
double Circuit::getSaturationCurrent()
{
	return saturationCurrent;
}

/* Returns the circuit thermal voltage VT*/
double Circuit::getThermalVoltage()
{
	return thermalVoltage;
}

/*Default Destructor */
Circuit::~Circuit() {
	//Cleanup
	std::cout << "Circuit Destroyed"<< std::endl;
}
