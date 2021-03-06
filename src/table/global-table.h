#ifndef GLOBAL_TABLE_H_
#define GLOBAL_TABLE_H_

#include "local-table.h"
#include "table.h"
#include "util/timer.h"
#include <mutex>
#include <unordered_map>

//#define GLOBAL_TABLE_USE_SCOPEDLOCK

namespace dsm {

class Worker;
class Master;

// Encodes table entries using the passed in TableData protocol buffer.
struct ProtoTableCoder: public TableCoder{
	ProtoTableCoder(const TableData* in);
	virtual void WriteEntryToFile(StringPiece k, StringPiece v1, StringPiece v2, StringPiece v3);
	virtual bool ReadEntryFromFile(string* k, string *v1, string *v2, string *v3);

	int read_pos_;
	TableData *t_;
};

// Encodes table entries using the passed in TableData protocol buffer.
struct ProtoKVPairCoder: public KVPairCoder{
	ProtoKVPairCoder(const KVPairData* in);
	virtual void WriteEntryToNet(StringPiece k, StringPiece v1);
	virtual bool ReadEntryFromNet(string* k, string *v1);

	int read_pos_;
	KVPairData *t_;
};

struct PartitionInfo{
	PartitionInfo() :
			dirty(false), tainted(false){
	}
	bool dirty;
	bool tainted;
	ShardInfo sinfo;
};

class GlobalTableBase: virtual public Table{
public:
	virtual void UpdatePartitions(const ShardInfo& sinfo) = 0;
	virtual TableIterator* get_iterator(int shard, bool bfilter,
			unsigned int fetch_num = FETCH_NUM) = 0;

	virtual bool is_local_shard(int shard) = 0;
	virtual bool is_local_key(const StringPiece &k) = 0;

	virtual PartitionInfo* get_partition_info(int shard) = 0;
	virtual LocalTable* get_partition(int shard) = 0;

	virtual bool tainted(int shard) = 0;
	virtual int owner(int shard) = 0;

protected:
	friend class Worker;
	friend class Master;

	virtual int64_t shard_size(int shard) = 0;
};

class MutableGlobalTableBase: virtual public GlobalTableBase{
public:
	// Handle updates from the master or other workers.
	virtual void SendUpdates() = 0;
	virtual void MergeUpdates(const KVPairData& req) = 0;
	virtual void ProcessUpdates() = 0;
	virtual void TermCheck() = 0;

	virtual int pending_write_bytes() = 0;

	virtual void resize(int64_t new_size) = 0;

	virtual void clear() = 0;
	// Exchange the content of this table with that of table 'b'.
	virtual void swap(GlobalTableBase *b) = 0;
	virtual void local_swap(GlobalTableBase *b) = 0;
};

class GlobalTable: virtual public GlobalTableBase{
public:
	virtual ~GlobalTable();

	void Init(const TableDescriptor *tinfo);

	void UpdatePartitions(const ShardInfo& sinfo);

	virtual TableIterator* get_iterator(int shard, bool bfilter,
			unsigned int fetch_num = FETCH_NUM) = 0;

	virtual bool is_local_shard(int shard);
	virtual bool is_local_key(const StringPiece &k);

	int64_t shard_size(int shard);

	PartitionInfo* get_partition_info(int shard){
		return &partinfo_[shard];
	}
	LocalTable* get_partition(int shard){
		return partitions_[shard];
	}

	bool tainted(int shard){
		return get_partition_info(shard)->tainted;
	}
	int owner(int shard){
		return get_partition_info(shard)->sinfo.owner();
	}
protected:
	virtual int shard_for_key_str(const StringPiece& k) = 0;

	int worker_id_;

	std::vector<LocalTable*> partitions_;
	std::vector<LocalTable*> cache_;

	std::recursive_mutex m_;
	std::mutex m_trig_;
	std::recursive_mutex& mutex(){
		return m_;
	}
	std::mutex& trigger_mutex(){
		return m_trig_;
	}

	std::vector<PartitionInfo> partinfo_;

	struct CacheEntry{
		double last_read_time;
		string value;
	};

	std::unordered_map<StringPiece, CacheEntry> remote_cache_;
};

class MutableGlobalTable:
		virtual public GlobalTable,
		virtual public MutableGlobalTableBase,
		virtual public Checkpointable{
public:
	MutableGlobalTable(){
		pending_writes_ = 0;
		snapshot_index = 0;
//		sent_bytes_ = 0;
	}

	void BufSend();
	void SendUpdates();
	virtual void MergeUpdates(const KVPairData& req) = 0;
	virtual void ProcessUpdates() = 0;
	void TermCheck();

	int pending_write_bytes();

	void clear();
	void resize(int64_t new_size);

	//override from Checkpointable
	void start_checkpoint(const string& f);
	void write_message(const KVPairData& d);
	void finish_checkpoint();
	void restore(const string& f);

	void swap(GlobalTableBase *b);
	void local_swap(GlobalTableBase *b);

//	int64_t sent_bytes_;
	Timer timer;
	int timerindex;

protected:
	int64_t pending_writes_;
	int snapshot_index;
	void termcheck();

	//double send_overhead;
	//double objectcreate_overhead;
	//int sendtime;
};

}

#endif /* GLOBAL_TABLE_H_ */
