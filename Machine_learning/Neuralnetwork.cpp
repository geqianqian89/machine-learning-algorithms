/*
 * Neuralnetwork.cpp
 *
 *  Created on: May 12, 2016
 *      Author: marija
 */

#define _USE_MATH_DEFINES
#include "Neuralnetwork.h"
#include "Dataset.h"
#include <chrono>
#include <algorithm>

using namespace std;

Neural_network::Neural_network(const vector<int> & init, double learning_rate, double acceptable_error):
		eta{learning_rate}, ok{acceptable_error}, config{init} {
	if (init.size() <= 2) throw runtime_error("Invalid initialization of neural network");
	num_layers = init.size();
	for (int i=0; i < num_layers; i++) {
		neurons_per_layer.push_back(init[i]);
	}
	/*add bias*/
	for (int i=0; i < num_layers-1; i++) {
		neurons_per_layer[i]++;
	}
	for (int i=0; i < num_layers; i++) {
		neurons_at_layer.push_back(vector<Neuron>(neurons_per_layer[i]));
	}
	build_network();
}

double Neural_network::generate_init_weight() {
	random_device rd;
	mt19937 gen(rd());
	uniform_real_distribution<> dist(0.1,1.0);
	return dist(gen);
}

void Neural_network::build_network() {
	for (int i=1; i < num_layers; i++) {
		for (int j=0; j<neurons_per_layer[i-1]; j++) {
			for (int k=0; k<neurons_per_layer[i]; k++) {
				Edge_key key{i-1, j, k};
				double w = generate_init_weight();
				neurons_at_layer[i-1][j].add_out_edge(w, key);
				neurons_at_layer[i][k].add_in_edge(w, key);
			}
		}
	}
}


double activation_func(double net) {
	return 1./(1.+exp(-net));
}
void Neural_network::calculate_outputs() {
	for (int i=1; i < num_layers; i++) {
		for (int j=0; j<neurons_per_layer[i]; j++) {
			if (is_bias(i, j)) continue;
			auto keys = neurons_at_layer[i][j].in_edges_keys();
			double res = 0.0;
			for (const auto & key: keys) {
				double out = neurons_at_layer[key.layer][key.in].get_out();
				double w =  neurons_at_layer[key.layer][key.in].out_edges_weights(key);
				res += out*w;
			}
			neurons_at_layer[i][j].set_out(activation_func(res));
		}
	}
}

double activation_func_der(double out) {
	return out*(1.-out);
}

void Neural_network::set_weight(double w, const Edge_key & k){
	neurons_at_layer[k.layer][k.in].out_edges_weights(k) = w;
	neurons_at_layer[k.layer+1][k.out].in_edges_weights(k) = w;
}

bool Neural_network::back_propagation(const vector<double> & targets) {
	bool res = true;
	map<Edge_key, double> sigmas;
	map<Edge_key, double> old_weights;
	/*check error! */
	for (int i=0; i<neurons_per_layer[num_layers-1]; i++) {
		if (abs(neurons_at_layer[num_layers-1][i].get_out() - targets[i]) >= ok) {
			res = false;
			break;
		}
	}
	/*calculate out_sigmas and set weights for output layer*/
	for (int i=0; i<neurons_per_layer[num_layers-1]; i++) {
		Neuron & n = neurons_at_layer[num_layers-1][i];
		vector<Edge_key> keys = n.in_edges_keys();
		double new_sig_val = -(targets[i] - n.get_out()) * activation_func_der(n.get_out());;
		for (const auto & key:keys) {
			Neuron & in = neurons_at_layer[key.layer][key.in];
			sigmas[key] = new_sig_val;
			double w = in.out_edges_weights(key);
			old_weights[key] = w;
			set_weight(w - sigmas[key]*eta*in.get_out(), key);
		}
	}
	for (int i=num_layers-2; i >= 1; i--) {
		for (int j=0; j<neurons_per_layer[i]; j++) {
			Neuron & n = neurons_at_layer[i][j];
			if (is_bias(i, j)) continue;
			vector<Edge_key> keys = n.in_edges_keys();
			vector<Edge_key> sigma_keys = n.out_edges_keys();
			double r = activation_func_der(n.get_out());
			for (const auto & key:keys) {
				Neuron & in = neurons_at_layer[key.layer][key.in];
				sigmas[key] = r;
				double sum = 0.0;
				for (const auto & sk: sigma_keys) {
					sum += sigmas[sk]*old_weights[sk];
				}
				sigmas[key] *= sum;
				double w = in.out_edges_weights(key);
				old_weights[key] = w;
				set_weight(w - sigmas[key]*eta*in.get_out(), key);
			}
		}
	}

	return res;
}

double Neural_network::test(Neural_network & nn, const vector<double> & inputs, const vector<double> & targets) {
	vector<double> res = nn.calculate(inputs);
	double coumpound_error = 0.0;
	cout << "Neural network test with inputs:" << endl;
	for (size_t i=0; i<inputs.size(); i++) {
		cout << "input[" << i << "] = " << inputs[i] << endl;
	}
	for (size_t i=0; i<res.size(); i++) {
		cout << "output[" << i << "] = " << res[i] << endl;
		cout << "expected output: " << targets[i] << endl;
		coumpound_error += abs(targets[i] - res[i]);
		cout << "ERROR = " <<  abs(targets[i] -res[i]) << endl;
	}
	cout << endl;
	return coumpound_error / res.size();
}

double Neural_network::test(Neural_network & nn, const Data_set & test_set)
{
	vector<vector<double>> inputs;
	vector<vector<double>> outputs;
	test_set.normalized_data(inputs, outputs);
	auto normalizer = test_set.attr.get_normalizer();
	auto attr_outputs = test_set.attr.get_attributes_of_kind(Attribute::Attribute_usage::output);

	int test_size = test_set.get_size();
	double total_error = 0.0;
	double misslabel_error = 0.0;
	int misslabel_count = 0;
	for (int test_idx = 0; test_idx < test_size; test_idx++) {
		vector<double> res = nn.calculate(inputs[test_idx]);
		vector<double> target = outputs[test_idx];

		auto res_labels = normalizer.undo_normalize(attr_outputs, res);
		auto target_labels = normalizer.undo_normalize(attr_outputs, target);

		double coumpound_error = 0.0;
		for (size_t i = 0; i<res.size(); i++) {
			if (res_labels[i].second != target_labels[i].second) {
				misslabel_count++;
				misslabel_error += res_labels[i].first;
			}

			cout << "output[" << i << "] = " << res[i] << " (" << res_labels[i].second << "," << res_labels[i].first << ")" << endl;
			cout << "expected output: " << target[i] << " (" << target_labels[i].second << ")" << endl;
			coumpound_error += abs(target[i] - res[i]);
			cout << "ERROR = " << abs(target[i] - res[i]) << endl;
		}
		total_error += coumpound_error / res.size();
	}
	total_error /= test_size;
	cout << "Total error: " << total_error << endl;
	cout << "Misslabel count: " << misslabel_count << " (" << double(misslabel_count)/test_size << ")" << endl;
	cout << "Only misslabel error: " << misslabel_error / test_size << endl;
	
	return double(misslabel_count) / test_size;
}

vector<double> Neural_network::calculate(const vector<double> & inputs){
	vector<double> out(neurons_per_layer[num_layers-1]);
	for (size_t i=0; i < inputs.size(); i++) {
		neurons_at_layer[0][i].set_out(inputs[i]);
	}
	calculate_outputs();
	for (size_t i=0; i < out.size(); i++) {
		out[i] = neurons_at_layer[num_layers-1][i].get_out();
	}
	return out;
}

void Neural_network::operator() (const vector<vector<double>> & inputs, const vector<vector<double>> & outputs, int epoch_count){
	size_t data_size = inputs.size();
	assert (data_size > 0);
	assert(data_size == outputs.size());
	assert(inputs[0].size() == neurons_at_layer[0].size()-1);
	assert(outputs[0].size() == neurons_at_layer[num_layers-1].size());

	vector <double> targets(outputs[0].size()); // target outputs

	std::chrono::time_point<std::chrono::high_resolution_clock> start, end, total_start;
	double calculation = 0.0;
	double backpropagation = 0.0;
	total_start = std::chrono::high_resolution_clock::now();

	while(epoch_count--) {
		//cout << epoch_count << endl;
		for (size_t current=0; current < data_size; current++) {			
			/*init inputs*/
			for (size_t i=0; i < inputs[0].size(); i++) {
				neurons_at_layer[0][i].set_out(inputs[current][i]);
			}
			/*init target*/
			for (size_t i=0; i < outputs[i].size(); i++) {
				targets[i] = outputs[current][i];
			}

			start = std::chrono::high_resolution_clock::now();

			/*calculate outputs*/
			calculate_outputs();

			end = std::chrono::high_resolution_clock::now();
			calculation += (std::chrono::duration<double>(end - start)).count();
			start = end;
			
			/*set new weights*/
			back_propagation(targets);

			end = std::chrono::high_resolution_clock::now();
			backpropagation += (std::chrono::duration<double>(end - start)).count();
		}
	}

	std::chrono::duration<double> total_duration = std::chrono::high_resolution_clock::now() - total_start;
	cout << "total duration " << total_duration.count() << "s" << endl;
	cout << "calc " << calculation << "s (" << 100.0 * calculation / total_duration.count() << "%)" << endl;
	cout << "back " << backpropagation << "s (" << 100.0 *  backpropagation / total_duration.count() << "%)" << endl;

}


void Neural_network::operator()(const Data_set & data, int epoch_count)
{
	vector<vector<double>> inputs;
	vector<vector<double>> outputs;

	data.normalized_data(inputs, outputs);
	this->operator()(inputs, outputs, epoch_count);
}

/*******************
 * E X A M P L E S *
 *******************/

void transform_values(vector<double> & values, double offset, double scale) {
	for (size_t i = 0; i < values.size(); i++) {
		values[i] = values[i] * scale + offset;
	}
}

void generate_sin(vector<double> & values, vector<double> & res) {
	size_t size = values.size();
	for (size_t i = 0; i < size; i++) {
		values[i] = i*M_PI*2.0 / size;
		res[i] = sin(values[i]);
	}
}

void nn_sine_example(int iterations = 1000) {
	vector<int> init{ 1, 3, 1 };
	vector<vector<double>> inputs(1);
	vector<vector<double>> outputs(1);
	Neural_network nn{ init, 0.6 };

	inputs[0] = vector<double>(50);
	outputs[0] = vector<double>(50);
	generate_sin(inputs[0], outputs[0]);
	transform_values(inputs[0], 0.0, 1 / (M_PI*2.0));
	transform_values(outputs[0], 0.5, 0.5);

	nn(inputs, outputs, iterations);

	vector<vector<double>> t_inputs(1);
	vector<vector<double>> t_outputs(1);
	t_inputs[0] = vector<double>(200);
	t_outputs[0] = vector<double>(200);
	generate_sin(t_inputs[0], t_outputs[0]);
	transform_values(t_inputs[0], 0.0, 1 / (M_PI*2.0));
	transform_values(t_outputs[0], 0.5, 0.5);

	double total_error = 0.0;
	double min_error = 1.0;
	double max_error = 0.0;
	for (size_t i = 0; i < t_inputs[0].size(); i++) {
		double error = Neural_network::test(nn, vector<double>{t_inputs[0][i]}, vector<double>{t_outputs[0][i]});
		total_error += error;
		min_error = min(min_error, error);
		max_error = max(max_error, error);
	}
	total_error /= t_inputs[0].size();
	cout << "total error: " << total_error << endl;
	cout << "min error: " << min_error << endl;
	cout << "max error: " << max_error << endl;
	cout << "avg span: " << (max_error - min_error) / 2.0 << endl;
}

void nn_xor_example(int iterations = 1000) {
	vector<int> init{ 2, 3, 1 };
	vector<vector<double>> inputs(2);
	vector<vector<double>> outputs(1);
	inputs[0] = vector<double>{ 0, 0, 1, 1 };
	inputs[1] = vector<double>{ 0, 1, 0, 1 };
	outputs[0] = vector<double>{ 0, 1, 1, 0 };
	Neural_network nn{ init, 0.1 };
	nn(inputs, outputs, iterations);

	Neural_network::test(nn, vector<double>{1, 1}, vector<double>{0});
	Neural_network::test(nn, vector<double>{1., 0}, vector<double>{1});
	Neural_network::test(nn, vector<double>{0, 1.}, vector<double>{1});
	Neural_network::test(nn, vector<double>{0, 0}, vector<double>{0});
}

void nn_tennis_example(int iterations = 1000) {
	//Data_set::_test_normalize_columns();

	string file_name = "../data/tennis.txt";
	string class_name = "Play";
	Data_set ds;
	ds.load_simple_db(file_name, class_name);

	Data_set train, test;
	ds.distribute_split(train, test, 0.8);

	vector<int> init{ train.attr.count_attr_by_usage(Attribute::Attribute_usage::input), 3, 1 };
	Neural_network nn{ init, 0.1 };
	nn(train, iterations);

	Neural_network::test(nn, test);
}

void nn_zoo_example(int iterations = 1000) {
	//Data_set::_test_normalize_columns();

	string file_name = "../data/zoo.txt";
	string class_name = "type";
	Data_set ds;
	ds.load_simple_db(file_name, class_name);

	Data_set train, test;
	ds.distribute_split(train, test, 0.8);

	vector<int> init{ train.attr.count_attr_by_usage(Attribute::Attribute_usage::input), 5, 1 };
	Neural_network nn{ init, 0.21 };
	nn(train, iterations);

	Neural_network::test(nn, test);
}


void Neural_network::run_examples() {
	/* xor example:*/
	//nn_xor_example(100000);

	/*sine example:*/
	//nn_sine_example(1000);

	//nn_tennis_example( 1000 );
	nn_zoo_example(1000);
}