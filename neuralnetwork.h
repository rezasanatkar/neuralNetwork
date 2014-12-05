#include "hiddenLayer.h"
#include <assert.h>

class identityFunction : public function<double, double>{
public:
	virtual double invoke(double arg) const{
		return arg;
	}
};

template <typename R, typename T>
class neuralNetwork{
	int numInputs;						// number of input values, i.e. dimension of x
	int numLayers;						// number of all layers in the network
	int * numNodesPerLayers;			// list of number of nodes for each layer
	function<R, T> * transferFunction;	// transfer function between activation and output of all nodes
	function<R, T> * derivative;		// derivative of transfer function
	hiddenLayer<R, T> ** hiddenLayers;	// array of hidden layers
	T ** activations;					// activations of all nodes
	R ** delta;							// sensitivity components of back-propagation
	T *** tempWeights;					// temporary weights used in back propagation
public:
	// Constructor
	neuralNetwork(int _numInputs, int _numLayers, int * _numNodesPerLayers, function<R, T> * _transferFunction, function<R, T> * _derivative) : numInputs(_numInputs), numLayers(_numLayers), numNodesPerLayers(_numNodesPerLayers), transferFunction(_transferFunction), derivative(_derivative){
		assert(numLayers > 1);
		// Build architecture of network
		hiddenLayers = new hiddenLayer<R, T>*[numLayers];
		// First hidden layer has numInputs input value and numNodesPerLayers[0] outputs
		hiddenLayers[0] = new hiddenLayer<R, T>(numInputs, numNodesPerLayers[0], transferFunction);
		// i-th hidden layer has numNodesPerLayers[i-1] input value and numNodesPerLayers[i] outputs, for i = 2,...,numLayers
		for (int i = 1; i < numLayers - 1; i++)
			hiddenLayers[i] = new hiddenLayer<R, T>(numNodesPerLayers[i - 1], numNodesPerLayers[i], transferFunction);
		// Last hidden layer have no transfer function
		hiddenLayers[numLayers - 1] = new hiddenLayer<R, T>(numNodesPerLayers[numLayers - 2], numNodesPerLayers[numLayers - 1], new identityFunction);

		// Build delta array to store sensitivity
		delta = new R*[numLayers];
		for (int i = 0; i < numLayers; i++)
			delta[i] = new R[numNodesPerLayers[i]];

		// Build activations array
		activations = new T*[numLayers];
		for (int i = 0; i < numLayers; i++)
			activations[i] = new T[numNodesPerLayers[i]];

		// Build array to store temporary weights
		tempWeights = new T **[numLayers];
		for (int i = 0; i < numLayers; i++){
			tempWeights[i] = new T *[numNodesPerLayers[i]];
			for (int j = 0; j < numNodesPerLayers[i]; j++)
				tempWeights[i][j] = new T[(i == 0 ? numInputs : numNodesPerLayers[i-1])];
		}
	}

	// Destructor
	virtual ~neuralNetwork(){
		for (int i = 0; i < numLayers; i++){
			delete hiddenLayers[i];
			delete[] delta[i];
			delete[] activations[i];
		}
		delete[] hiddenLayers;
		delete[] delta;
		delete[] activations;

		for (int i = 0; i < numLayers; i++){
			for (int j = 0; j < numNodesPerLayers[i]; j++)
				delete[] tempWeights[i][j];
			delete[] tempWeights[i];
		}
		delete[] tempWeights;
	}

	// Assign weights for all edges in the network
	void setWeights(T ***weights){
		for (int i = 0; i < numLayers; i++)
			hiddenLayers[i]->setWeights(weights[i]);

		for (int i = 0; i < numLayers; i++)
		for (int j = 0; j < numNodesPerLayers[i]; j++)
		for (int k = 0; k < (i == 0 ? numInputs : numNodesPerLayers[i - 1]); k++)
			tempWeights[i][j][k] = weights[i][j][k];
	}

	// Feed-forward algorithm to compute the output of network
	R * feedForward(T * inputs){
		R * tempInput = inputs;
		R * tempOutput;
		for (int i = 0; i < numLayers; i++){
			tempOutput = hiddenLayers[i]->computeOutputs(tempInput);
			if(i > 0)
				delete[] tempInput;
			tempInput = tempOutput;
		}
		return tempInput;
	}

	// Back-propagation algorithm to update weights
	void backPropagation(T * inputs, const int label, const R epsilon){
		// compute all activations and outputs
		computeActivations(inputs);
		// compute all delta's
		computeSensitivity(tempWeights, label);
		// update all weights
		updateWeights(inputs, epsilon);
	}

	// Compute mse
	T computeMSE(T * inputs, const int label)
	{
		R * temp = feedForward(inputs);
		T J = 0;
		for (int i = 0; i < numNodesPerLayers[numLayers - 1]; i++)
		{
			T t = 2 * (T)(label == i) - 1;
			J += (temp[i] - t) *(temp[i] - t);
		}
		delete[] temp;
		return J;
	}
private:
	// Compute activation (alpha) and output (beta) for all nodes
	void computeActivations(T * inputs){
		// temp1 store outputs for each layer
		T * temp1 = hiddenLayers[0]->computeOutputs(inputs);
		//printf("output of 1 layer:\n");
		//for (int j = 0; j < numNodesPerLayers[0]; j++)
		//	printf("%.4f ", temp1[j]);
		//printf("\n");

		// temp2 store activation for each layer
		T * temp2 = hiddenLayers[0]->getActivations();
		for (int j = 0; j < numInputs; j++)
			activations[0][j] = temp2[j];
		//printf("activation of 1 layer:\n");
		//for (int j = 0; j < numNodesPerLayers[0]; j++)
		//	printf("%.4f ", temp2[j]);
		//printf("\n");
		delete[] temp2;

		for (int l = 1; l < numLayers; l++){
			// compute output of the layer
			temp2 = hiddenLayers[l]->computeOutputs(temp1);
			//printf("output of %d layer:\n",l+1);
			//for (int j = 0; j < numNodesPerLayers[l]; j++)
			//	printf("%.4f ", temp2[j]);
			//printf("\n");

			delete[] temp1;
			temp1 = temp2;

			// compute activation and store
			temp2 = hiddenLayers[l]->getActivations();
			for (int j = 0; j < numNodesPerLayers[l]; j++)
				activations[l][j] = temp2[j];
			delete[] temp2;

			//printf("activation of %d layer:\n", l + 1);
			//for (int j = 0; j < numNodesPerLayers[l]; j++)
			//	printf("%.4f ", activations[l][j]);
			//printf("\n");
		}
	}

	// Compute delta's
	void computeSensitivity(T ***weights, const int label){
		for (int l = numLayers - 1; l >= 0; l--)
		for (int i = 0; i < numNodesPerLayers[l]; i++)
		if (l == numLayers - 1)
			delta[l][i] = 2 * (activations[l][i] - (2 * (T)(label == i) - 1));
		else{
			delta[l][i] = 0;
			for (int j = 0; j < numNodesPerLayers[l + 1]; j++)
				delta[l][i] += weights[l + 1][j][i] * delta[l + 1][j];
			delta[l][i] *= derivative->invoke(activations[l][i]);
		}

		//printf("delta:\n");
		//for (int l = numLayers - 1; l >= 0; l--){
		//	for (int i = 0; i < numNodesPerLayers[l]; i++)
		//		printf("%.4f ", delta[l][i]);
		//	printf("\n");
		//}
	}

	// Update weights
	void updateWeights(T * inputs, T epsilon){
		for (int l = 0; l < numLayers; l++)
		for (int j = 0; j < numNodesPerLayers[l]; j++){
			if (l == 0)
			for (int i = 0; i < numInputs; i++)
				tempWeights[l][j][i] -= epsilon * delta[l][i] * inputs[j];
			else
			for (int i = 0; i < numNodesPerLayers[l - 1]; i++)
				tempWeights[l][j][i] -= epsilon * delta[l][j] * transferFunction->invoke(activations[l - 1][i]);
		}

		//printf("new weights:\n");
		//for (int l = 0; l < numLayers; l++){
		//	for (int j = 0; j < numNodesPerLayers[l]; j++){
		//		if (l == 0)
		//		for (int i = 0; i < numInputs; i++)
		//			printf("%.4f ", tempWeights[l][j][i]);
		//		else
		//		for (int i = 0; i < numNodesPerLayers[l - 1]; i++)
		//			printf("%.4f ", tempWeights[l][j][i]);
		//		printf("\n");
		//	}
		//	printf("\n");
		//}

		for (int i = 0; i < numLayers; i++)
			hiddenLayers[i]->setWeights(tempWeights[i]);
	}
};