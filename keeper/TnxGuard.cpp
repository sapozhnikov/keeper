#include "stdafx.h"
#include "TnxGuard.h"

TnxGuard::TnxGuard(DbEnv& env) :
	env_(env)
{
	env_.txn_begin(NULL, &txn_, 0);
}

TnxGuard::~TnxGuard()
{
	if (!isAborted_ && (txn_ != nullptr))
		txn_->commit(0);
}

DbTxn* TnxGuard::Get()
{
	return txn_;
}

void TnxGuard::Abort()
{
	if (txn_ != nullptr)
		txn_->abort();
	isAborted_ = true;
}
