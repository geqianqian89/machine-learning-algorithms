#include "id3_algorithm.h"
#include "Dataset.h"
#include <iostream>
using namespace std;

id3_algorithm::id3_algorithm(Data_set & data) : ds{ data }
{
	class_name = ds.get_label_name();
	class_vals = ds.attr.get_attr_values(class_name);
}

double id3_algorithm::calculate_entropy(const vector<int> & subset) {
	set<string> vals = ds.attr.get_attr_values(class_name);
	double res = 0.0;
	for (const auto &val: vals) {
		vector<int> indexes = ds.split_by_attr_val(subset, class_name, val);
		if( indexes.size() == 0 )
			continue;

		double p = double(indexes.size())/double(subset.size());
		res -= p*log2(p);
	}
	return res;
}

double id3_algorithm::calculate_gain(const string & att, const vector<int> & subset) {
	set<string> vals = ds.attr.get_attr_values(att);
	double res = 0.0;
	for (const auto &val: vals) {
		vector<int> indexes = ds.split_by_attr_val(subset, att, val);
		if( indexes.size() == 0 )
			continue;

		double p = double(indexes.size())/ds.get_size();
		res += p * calculate_entropy(indexes);
	}
	cout << att << " gain " << res << endl;
	return res;
}

string id3_algorithm::find_most_common_class(const vector<int> & subset) {
	int max = 0;
	string common;
	for (const auto & val: class_vals) {
		int num = 0;
		for (int i: subset) {
			if (ds.get_elem(i).get_value(class_name) == val) {
				num += 1;
			}
		}
		if (max < num) {
			max = num;
			common = val;
		}
	}
	return common;
}

Decision_node id3_algorithm::operator()() {
	vector<int> subset;
	subset.reserve(ds.get_size());
	for (int i=0; i< ds.get_size(); i++) {
		subset.push_back(i);
	}
	return calculate(subset, ds.attr.get_attributes_of_kind(Attribute::Attribute_usage::input));
}


Decision_node id3_algorithm::calculate(const vector<int> & subset, const vector<string> & attributes) {
	for (const auto & val: class_vals) {
		bool all_the_same= true;
		for (int i: subset) {
			if (ds.get_elem(i).get_value(class_name) !=val) {
				all_the_same = false;
				break;
			}
		}
		if (all_the_same) {
			return Decision_node{class_name, set<string>{val}, true};
		}
	}


    // check if the are any more attributes:
	if (attributes.size() == 0) {
		return Decision_node{class_name, set<string>{find_most_common_class(subset)}, true};
	}

	double min = calculate_gain(attributes[0], subset);
	string attribute = attributes[0];
	for (const auto & att: attributes) {
		// calculate info gain for curr
		// find min, choose that attr
		double et = calculate_gain(att, subset);
		if (et < min) {
			min = et;
			attribute = att;
		}
	}
	cout << "I select: " << attribute << endl;
	// remove selected attribute
	vector<string> attr;
	for (const auto & at: attributes) {
		if (at != attribute)
			attr.push_back(at);
	}
	// do the splitting
	auto vals = ds.attr.get_attr_values(attribute);
	Decision_node new_node{attribute, vals};
	for (const auto & v : vals) {
		vector<int> res = ds.split_by_attr_val(subset, attribute, v);

		if (res.size()) {
			new_node.add_a_child(v, calculate(res, attr));
		} else {
			// no val of this type found.
			new_node.add_a_child(v, Decision_node{class_name, set<string>{find_most_common_class(subset)}, true});
		}
	}
	return new_node;
}


/*******************
 * E X A M P L E S *
 *******************/


void id3_test(string file_name, string class_name, string name) {
	Data_set ds;
	ds.load_simple_db(file_name, class_name);

	Data_set train;
	Data_set test;
	ds.distribute_split(train, test, 0.65, true);

	cout << "data length: " << ds.get_size() << endl;
	id3_algorithm id3{ train };
	cout << "train data length: " << train.get_size() << endl;
	Decision_node root = id3();

	root.print();

	int count = 0;
	for (int i = 0; i< test.get_size(); i++) {
		Data d{ test.get_elem(i) };
		string res = root.classify(d);
		if (d.get_value(class_name) == res) {
			cout << d.get_value(name) << " is a " << res << endl;
			count++;
		}
		else
			cout << d.get_value(name) << " is not a " << res << endl;
	}
	cout << "Correctness: " << count / (double)test.get_size() << endl;
}

void id3_algorithm::run_examples() {
	id3_test("../data/tennis.txt", "Play", "Day");
	//id3_test("../data/zoo.txt", "type", "object");
}