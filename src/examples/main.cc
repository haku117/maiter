#include "client/client.h"
//TODO: change back after message-driven is finished
#include "net/NetworkThread.h"
//#include <mpi.h>
#include <iostream>

using namespace dsm;
using namespace std;

DEFINE_string(runner, "", "");

DEFINE_int32(shards, 10, "");
DEFINE_int32(iterations, 10, "");
DEFINE_int32(block_size, 10, "");
DEFINE_int32(edge_size, 1000, "");
DEFINE_bool(build_graph, false, "");
DEFINE_bool(dump_results, false, "");

DEFINE_int32(bufmsg, 10000, "expected minimum number of message per sending");
DEFINE_double(buftime, 3, "maximum time interval between 2 sendings");

DEFINE_string(graph_dir, "subgraphs", "");
DEFINE_string(result_dir, "result", "");
DEFINE_int32(max_iterations, 100, "");
DEFINE_int64(num_nodes, 100, "");
DEFINE_double(portion, 1, "");
DEFINE_double(termcheck_threshold, 1000000000, "");
DEFINE_double(sleep_time, 0.001, "");

DEFINE_string(checkpoint_write_dir, "/tmp/maiter/checkpoints", "");
DEFINE_string(checkpoint_read_dir, "/tmp/maiter/checkpoints", "");
DEFINE_double(flush_time,0.2,"waiting time for flushing out all network message");

DEFINE_int32(adsorption_starts, 100, "");
DEFINE_double(adsorption_damping, 0.1, "");
DEFINE_int64(shortestpath_source, 0, "");
DEFINE_int64(katz_source, 0, "");
DEFINE_double(katz_beta, 0.1, "");


int main(int argc, char** argv){
	FLAGS_log_prefix = false;
//  cout<<getcallstack()<<endl;

	Init(argc, argv);

	ConfigData conf;
	//TODO: change back after message-driven is finished
//  conf.set_num_workers(MPI::COMM_WORLD.Get_size() - 1);
//  conf.set_worker_id(MPI::COMM_WORLD.Get_rank() - 1);
	conf.set_num_workers(NetworkThread::Get()->size() - 1);
	conf.set_worker_id(NetworkThread::Get()->id() - 1);

//  cout<<NetworkThread::Get()->id()<<":"<<getcallstack()<<endl;
// return 0;
//  LOG(INFO) << "Running: " << FLAGS_runner;
	CHECK_NE(FLAGS_runner, "");
	RunnerRegistry::KernelRunner k = RunnerRegistry::Get()->runner(FLAGS_runner);
	LOG(INFO)<< "kernel runner is " << FLAGS_runner;
	CHECK(k != NULL) << "Could not find kernel runner " << FLAGS_runner;
	k(conf);
	LOG(INFO)<< "Exiting.";
}
