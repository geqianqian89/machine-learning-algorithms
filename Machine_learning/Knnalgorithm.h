/*
 * Knnalgorithm.h
 *
 *  Created on: May 23, 2016
 *      Author: marija
 */

#ifndef KNNALGORITHM_H_
#define KNNALGORITHM_H_

#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include "Algorithm.h"
#include "Dataset.h"

typedef double(*distance_function)(const std::vector<double> &, const std::vector<double> &);

class Data_set;
class Attribute_normalizer;

class Knn_algorithm : public Algorithm {
public:
	Knn_algorithm(const std::vector<std::vector<double>> & in,
		      const std::vector<std::vector<double>> & out,
		      int nn, distance_function d, bool v=false);
	Knn_algorithm(const Data_set & data, int nn, distance_function d, bool v=false);
	void test(const Data_set & test);
	std::vector<double> operator()(const std::vector<double> & query_point);
	static constexpr double tolerance=1e-10;

	void classify(const Data & d, std::vector<double> & out);
	void setup(const Algorithm_parameters& parameters);
	void learn(const Data_set & data_set, const Attribute_normalizer& normalizer);
	using Algorithm::learn;


	static void run_examples();
private:
	std::vector<std::vector<double>> inputs;
	std::vector<std::vector<double>> outputs;
	Attribute_normalizer normalizer;
	int k;
	distance_function distance_func;
	bool vote;
	std::vector<std::pair<double, int>> calculate_distances(const std::vector<double> & query_point);
	std::vector<double> get_average(const std::vector<std::pair<double, int>> & knn);
	std::vector<double> find_most_common(const std::vector<std::pair<double, int>> & knn);
	bool check_database(const std::vector<double> & query_point, std::vector<double> &);
};

#endif /* KNNALGORITHM_H_ */
