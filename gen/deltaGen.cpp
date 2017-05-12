/*
 * main.cpp
 *
 *  Created on: Jan 11, 2016
 *      Author: tzhou
 *  Modified on Mar 17, 2017
 *      Add weight and more options
 *  Modified on April 23, 2017 by GZ
 *		generate delta graph files
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <random>
#include <functional>
#include "pl-dis.hpp"
#include <unordered_set>

using namespace std;

// ---- load the graph data and generate the delta file

struct Edge{
	int u, v;
	string w;
};

int loadGraph(const string& dir, const string& deltaPrefix, const int nPart, const int seed,
		const double rate, const double addRate, const double rmvRate, const double incRate){
	vector<ifstream*> fin;
	vector<ofstream*> fout;
	for(int i = 0; i < nPart; ++i){
		fin.push_back(new ifstream(dir + "/part" + to_string(i)));
		fout.push_back(new ofstream(deltaPrefix + "-" + to_string(i)));
		if(!fin.back()->is_open()){
			cerr << "failed in opening file: " << dir + "/part" + to_string(i) << endl;
			return -i;
		}
		if(!fout.back()->is_open()){
			cerr << "failed in opening file: " << deltaPrefix + "-" + to_string(i) << endl;
			return -i;
		}
	}

	double desRate=1-addRate-rmvRate-incRate;

	mt19937 gen(seed);
	uniform_real_distribution<double> rnd_prob(0.0, 1.0);
	uniform_int_distribution<int> ud; // 0 to numeric_limits<int>::max()
	uniform_real_distribution<float> rnd_weight(0, 1);

	double modifyProb=rate*(1-addRate);
	double addProb=rate*addRate;
	double addProbThreshold=modifyProb+addProb;

	vector<Edge> mSet;
	vector<Edge> addSet;
	int totalV = 0, totalE = 0;
	int failAdd=0;

	int maxV=0;
	string line;
	for(int i = 0; i < nPart; i++){
		while(getline(*fin[i], line)){
			// *fout[i] << line << endl;
			totalV++;
			//cout << line << endl;

			size_t pos = line.find("\t");
			int u = stoi(line.substr(0, pos));
			line = line.substr(pos + 1);

			unordered_set<int> hs;
			hs.insert(u);
			int addCnt=0;
			while((pos = line.find_first_of(" ")) != line.npos){
				totalE++;
				string link = line.substr(0, pos);
				int cut = link.find_first_of(",");
				Edge e;
				e.u = u;
				e.v = stoi(link.substr(0, cut));
				e.w = link.substr(cut + 1);

				line = line.substr(pos + 1);

				maxV = max(maxV, e.v);
				hs.insert(e.v);

				// whether to modify (remove, increase, decrease)
				double r=rnd_prob(gen);
				if(r < modifyProb){
					mSet.push_back(e);
				}else if(r < addProbThreshold){ // whether to add
					++addCnt;
				}
			}
			//cout << hs.size() << endl;
			// add
			while(addCnt--){
				int rpt=0;
				int newV;
				do{
					newV=ud(gen) % maxV;
				}while(hs.find(newV)!=hs.end() && rpt++<10);
				if(rpt<10){
					Edge e{u, newV, to_string(rnd_weight(gen))};
					addSet.push_back(move(e));
				}else{
					++failAdd;
				}
			}
		} // line
	} // file

	int addCnt = 0, rmvCnt = 0, incCnt = 0, decCnt = 0;
	double thRmv=rmvRate, thInc=rmvRate+incRate, thDes=rmvRate+incRate+desRate;
	for(Edge e : mSet){
		double r=rnd_prob(gen);
		if(r < thRmv){
			*fout[e.u % nPart] << "R " << e.u << "," << e.v << "," << e.w << "\n";
			//*fout[e.v % nPart] << "R " << e.v << "," << e.u << "," << e.w << "\n";
			rmvCnt++;
		}else if(r < thInc){
			float ww = stof(e.w) * (1 + rnd_prob(gen));
			*fout[e.u % nPart] << "I " << e.u << "," << e.v << "," << ww << "\n";
			//*fout[e.v % nPart] << "I " << e.v << "," << e.u << "," << ww << "\n";
			incCnt++;
		}else if(r < thDes){
			float ww = stof(e.w) * rnd_prob(gen);
			*fout[e.u % nPart] << "D " << e.u << "," << e.v << "," << ww << "\n";
			//*fout[e.v % nPart] << "D " << e.v << "," << e.u << "," << ww << "\n";
			decCnt++;
		}
	}

	for(Edge e : addSet){
		*fout[e.u % nPart] << "A " << e.u << "," << e.v << "," << e.w << "\n";
		//*fout[e.v % nPart] << "A " << e.v << "," << e.u << "," << e.w << "\n";
		addCnt++;
	}

	for(size_t i = 0; i < fout.size(); i++){
		delete fout[i];
		delete fin[i];
	}

	//delete inc;
	cout << "total vertex/edge: " << totalV << "/" << totalE << endl;
	cout << "Add edge: " << addCnt << " failed: " << failAdd << endl;
	cout << "Remove edge: " << rmvCnt << endl;
	cout << "Increase edge weight: " << incCnt << endl;
	cout << "Decrease edge weight: " << decCnt << endl;
	return nPart;
}

// ------ main ------

struct Option{
	int nPart, nNode;
	string deltaPrefix;
	string dist;
	double alpha; // for power-law distribution
	string weight;
	double wmin, wmax;
	string outDir;
	int prop;
	unsigned long seed;
	double rate;	// rate of changed edges
	double addRate, rmvRate, incRate, decRate;

	void parse(int argc, char* argv[]);
private:
	bool setDist(string& method);
	bool setWeight(string& method);
	bool normalizeRates();
};

void Option::parse(int argc, char* argv[]){
	outDir = argv[1];
	nPart = stoi(string(argv[2]));
//	nNode=stoi(string(argv[2]));
//	string distMethod="pl:2.3";
//	string weightMethod="no";
	deltaPrefix = argv[3];
	rate = stof(string(argv[4]));
	addRate = stof(string(argv[5]));
	rmvRate = stof(string(argv[6]));
	incRate = stof(string(argv[7]));
	decRate = stof(string(argv[8]));
	seed = 1535345;
	if(argc > 9)
		seed = stoul(string(argv[9]));
//	if(argc>=5)
//		weightMethod=argv[4];
	/*if(argc>=7)
	 prop=stoi(string(argv[6]));
	 // check distribution
	 if(!setDist(distMethod))
	 throw invalid_argument("unsupported degree distribution: "+distMethod);
	 if(!setWeight(weightMethod))
	 throw invalid_argument("unsupported weight distribution: "+weightMethod);
	 */
	if(!normalizeRates())
		throw invalid_argument("Given rates do not make sense.");
}
bool Option::setDist(string& method){
	if(method == "uni"){
		alpha = 2.0;
		dist = "uni";
	}else if(method.substr(0, 3) == "pl:"){
		alpha = stod(method.substr(3));
		dist = "pl";
	}else{
		return false;
	}
	return true;
}
bool Option::setWeight(string& method){
	if(method == "no"){
		weight = "no";
	}else if(method.substr(0, 7) == "weight:"){
		weight = "weight";
		size_t p = method.find(',', 7);
		wmin = stod(method.substr(7, p - 7));
		wmax = stod(method.substr(p + 1));
	}else{
		return false;
	}
	return true;
}
bool Option::normalizeRates(){
	bool flag= 0.0<rate && rate<=1.0
			&& 0.0<=addRate && 0.0<=rmvRate && 0.0<=incRate && 0.0<=decRate;
	if(!flag)
		return false;
	double total=addRate+rmvRate+incRate+decRate;
	if(total!=1.0){
		cout<<"normalizing modifying rates"<<endl;
		addRate/=total;
		rmvRate/=total;
		incRate/=total;
		decRate/=total;
	}
	return true;
}

int main(int argc, char* argv[]){
	if(argc < 3 || argc > 10){
		cerr<< "Wrong usage.\n"
				"Usage: \"deltaGen <dir> <#parts> <delta-prefix> <deltaRate> <addRate> <rmvRate> <incRate> <desRate> [random-seed]\""
				<< endl;
		cerr << "  <dir>: the folder of graphs.\n"
				"  <#parts>: number of parts the graphs are separated (the number of files to operate).\n"
				"  <delta-prefix>: the path and name prefix of generated delta graphs, naming format: \"<delta-prefix>-<part>\".\n"
				"  <deltaRate>: the rate of changed edges.\n"
				"  <addRate>, <rmvRate>, <incRate>, <desRate>: "
				"among the changed edges the rate for edge-addition, edge-removal, weight-increase and weight-decrease. "
				"They are automatically normalized.\n"
				"  [random-seed]: (=1535345) seed for random numbers\n"
				"i.e.: ./deltaGen.exe graphDir 2 delta-rd 0.05 0 0.3 0 0.7 123456\n"
				"i.e.: ./deltaGen.exe input 2 ../delta/d2 0.01 0.2 0.2 0.3 0.3\n"
				<< endl;
		return 1;
	}
	Option opt;
	try{
		opt.parse(argc, argv);
	} catch(exception& e){
		cerr << e.what() << endl;
		return 2;
	}
	ios_base::sync_with_stdio(false);
	cout << "Loading " << opt.nPart << " parts, from folder: " << opt.outDir << endl;

	int n = loadGraph(opt.outDir, opt.deltaPrefix, opt.nPart, opt.seed, opt.rate,
			opt.addRate, opt.rmvRate, opt.incRate);

	cout << "success " << n << " files.\n fail " << opt.nPart - n << " files." << endl;
	return 0;
}

