#ifndef WORKER_H_
#define WORKER_H_

#include "util/common.h"
#include "kernel/kernel.h"
#include "table/TableHelper.h"
#include "table/table.h"
#include "table/local-table.h"
#include "table/global-table.h"
#include "msg/message.pb.h"
#include "net/RPCInfo.h"
#include "driver/MsgDriver.h"

#include <mutex>

namespace dsm {

class NetworkThread;

// If this node is the master, return false immediately.  Otherwise
// start a worker and exit when the computation is finished.
bool StartWorker(const ConfigData& conf);

class Worker: public TableHelper, private noncopyable{
	struct Stub;
public:
	Worker(const ConfigData &c);
	~Worker();

	void Run();

	void KernelLoop();
	Stats get_stats(){
		return stats_;
	}

	void CheckForMasterUpdates();
	void CheckNetwork();

	void HandleSwapRequest(const SwapTable& req, EmptyMessage *resp, const RPCInfo& rpc);
	void HandleClearRequest(const ClearTable& req, EmptyMessage *resp, const RPCInfo& rpc);
	void HandleIteratorRequest(const IteratorRequest& iterator_req, IteratorResponse *iterator_resp,
			const RPCInfo& rpc);
	void HandleShardAssignment(const ShardAssignmentRequest& req, EmptyMessage *resp,
			const RPCInfo& rpc);

	void SendPutRequest(int dstWorkerID, const KVPairData& msg);
	void HandlePutRequest();

	void SyncSwapRequest(const SwapTable& req){}
	void SyncClearRequest(const ClearTable& req){}


	// Barrier: wait until all table data is transmitted.
	void HandleFlush(const EmptyMessage& req, EmptyMessage *resp, const RPCInfo& rpc);
	void HandleApply(const EmptyMessage& req, EmptyMessage *resp, const RPCInfo& rpc);

	void FlushUpdates();

	// Enable or disable triggers
	void HandleEnableTrigger(const EnableTrigger& req, EmptyMessage* resp, const RPCInfo& rpc);

	// terminate iteration
	void HandleTermNotification(const TerminationNotification& req, EmptyMessage* resp,
			const RPCInfo& rpc);

	int peer_for_shard(int table_id, int shard) const;
	int id() const{
		return config_.worker_id();
	}
	int epoch() const{
		return epoch_;
	}

	int64_t pending_kernel_bytes() const;
	bool network_idle() const;

	bool has_incoming_data() const;

private:
	void registerHandlers();

	void StartCheckpoint(int epoch, CheckpointType type);
	void FinishCheckpoint();
	void SendTermcheck(int index, long updates, double current);
	void Restore(int epoch);
	void UpdateEpoch(int peer, int peer_epoch);

	mutable std::recursive_mutex state_lock_;

	// The current epoch this worker is running within.
	int epoch_;

	int num_peers_;
	bool running_;
	CheckpointType active_checkpoint_;

	typedef unordered_map<int, bool> CheckpointMap;
	CheckpointMap checkpoint_tables_;

	ConfigData config_;

	// The status of other workers.
	vector<Stub*> peers_;

	NetworkThread *network_;
	unordered_set<GlobalTableBase*> dirty_tables_;

	uint32_t iterator_id_;
	unordered_map<uint32_t, TableIterator*> iterators_;

	struct KernelId{
		std::string kname_;
		int table_;
		int shard_;

		KernelId(std::string kname, int table, int shard) :
				kname_(kname), table_(table), shard_(shard){
		}

#define CMP_LESS(a, b, member)\
  if ((a).member < (b).member) { return true; }\
  if ((b).member < (a).member) { return false; }

		bool operator<(const KernelId& o) const{
			CMP_LESS(*this, o, kname_);
			CMP_LESS(*this, o, table_);
			CMP_LESS(*this, o, shard_);
			return false;
		}
	};

	std::map<KernelId, DSMKernel*> kernels_;

	Stats stats_;

	MsgDriver driver;
};

}

#endif /* WORKER_H_ */
