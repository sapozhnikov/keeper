#pragma once
class TnxGuard
{
public:
	TnxGuard(DbEnv&);
	~TnxGuard();
	DbTxn* Get();
	void Abort();
private:
	DbEnv& env_;
	DbTxn* txn_ = nullptr;
	bool isAborted_ = false;
};
