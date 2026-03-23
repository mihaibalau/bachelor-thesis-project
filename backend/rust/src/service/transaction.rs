use std::collections::{BTreeMap, HashMap};
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicI64, Ordering};
use std::thread;
use async_trait::async_trait;
use chrono::{DateTime, NaiveDate, Utc};
use tokio::task;
use crate::db::errors::RepoError;
use crate::db::transaction_repo::TransactionRepo;
use crate::domain::ids::{AccountId, TransactionId, UserId};
use crate::domain::transaction::Transaction;
use crate::domain::value::transaction_type::TransactionType;
use crate::service::account::AccountRepository;
use crate::service::errors::{ServiceError, ServiceResult};

/// Abstraction over transaction persistence
///
/// Defining this as a trait lets you unit-test `TransactionService` with
/// in-memory fakes instead of hitting Postgres
#[async_trait]
pub trait TransactionRepository: Send + Sync {
    async fn get_by_id(&self, transaction_id: TransactionId) -> Result<Transaction, RepoError>;
    async fn insert(&self, tx: &Transaction) -> Result<TransactionId, RepoError>;
    async fn list_for_account(&self, account_id: AccountId, limit: i64, offset: i64)
            -> Result<Vec<Transaction>, RepoError>;
}

/// Thin adapter: your concrete SQLx repo implements the trait
///
/// This mirrors the "port/adapter" pattern and keeps the service layer
/// decoupled from SQLx's exact API
#[async_trait]
impl TransactionRepository for TransactionRepo {
    async fn get_by_id(&self, transaction_id: TransactionId) -> Result<Transaction, RepoError> {
        self.get_by_id(transaction_id).await
    }

    async fn insert(&self, tx: &Transaction) -> Result<TransactionId, RepoError> {
        self.insert(tx).await
    }

    async fn list_for_account(&self, account_id: AccountId, limit: i64, offset: i64) -> Result<Vec<Transaction>, RepoError> {
        self.list_for_account(account_id, limit, offset).await
    }
}

/// Command object for recording a new transaction
///
/// Domain invariants are checked when we
/// build the `Transaction` entity
#[derive(Debug, Clone)]
pub struct RecordTransactionCommand {
    pub from_account_id: AccountId,
    pub to_account_id: AccountId,
    pub transaction_type: TransactionType,
    pub value_cents: i64,
    pub description: String
}

/// Input for requesting an account statement
#[derive(Debug, Clone)]
pub struct AccountStatementQuery {
    pub account_id: AccountId,
    pub from: Option<DateTime<Utc>>,
    pub to: Option<DateTime<Utc>>,
    pub limit: i64,
    pub offset: i64,
}

/// Output row in an account statement
///
/// This can be sent directly to the frontend and plotted as a graph
#[derive(Debug, Clone)]
pub struct AccountStatementEntry {
    pub transaction_id: TransactionId,
    pub recorded_on: DateTime<Utc>,
    pub description: String,
    pub transaction_type: TransactionType,
    pub value_cents: i64,
    /// Balance *after* applying this transaction.
    pub balance_after_cents: i64,
}

/// High-level analytics across *all* accounts belonging to a user
///
/// Designed to show off multi-threaded aggregation using `Arc`,
/// `Mutex` and atomics
#[derive(Debug, Clone)]
pub struct UserTransactionStatistics {
    pub total_incoming_cents: i64,
    pub total_outgoing_cents: i64,
    pub total_volume_cents: i64,
    pub per_type_totals: HashMap<TransactionType, i64>,
    pub per_day_totals: BTreeMap<NaiveDate, i64>,
}

/// Main application service for transaction-related use-cases
#[derive(Clone)]
pub struct TransactionService<T, A>
where
    T: TransactionRepository,
    A: AccountRepository,
{
    tx_repo: Arc<T>,
    account_repo: Arc<A>
}

impl <T, A> TransactionService<T, A>
where
    T: TransactionRepository,
    A: AccountRepository,
{
    pub fn new(tx_repo: Arc<T>, account_repo: Arc<A>) -> Self {
        Self { tx_repo, account_repo }
    }

    /// Record a new transaction
    ///
    /// This method:
    /// - validates using the domain `Transaction` constructor,
    /// - stores the transaction
    pub async fn record_transaction(&self, cmd: RecordTransactionCommand)
        -> ServiceResult<TransactionId> {

        if cmd.value_cents <= 0 {
            return Err(ServiceError::Validation("transaction value must be strictly positive".into()));
        }

        // Construct domain entity – `Transaction::create`
        // should enforce domain invariants
        let tx = Transaction::create(
            cmd.from_account_id,
            cmd.to_account_id,
            cmd.transaction_type,
            cmd.value_cents,
            cmd.description,
        )?;

        let id = self.tx_repo.insert(&tx).await?;
        Ok(id)
    }

    /// Raw listing for an account
    pub async fn list_for_account(&self, account_id: AccountId, limit: i64, offset: i64)
        -> ServiceResult<Vec<Transaction>> {

        let txs = self.tx_repo.list_for_account(account_id, limit, offset).await?;
        Ok(txs)
    }

    /// Compute a statement for one account with running balances and
    /// optional time range filter
    ///
    /// This is intentionally done *in memory* on the service side,
    /// to show `spawn_blocking` for CPU-heavy processing as recommended
    /// in Zero To Production, §10.2.4.
    pub async fn compute_account_statement(&self, query: AccountStatementQuery)
        -> ServiceResult<Vec<AccountStatementEntry>> {

        let mut txs = self.tx_repo.list_for_account(query.account_id, query.limit, query.offset).await?;

        // Filter by date range
        if let Some(from) = query.from {
            txs.retain(|t| t.recorded_on() >= from);
        }
        if let Some(to) = query.to {
            txs.retain(|t| t.recorded_on() <= to);
        }

        // Sort oldest -> newest
        txs.sort_by_key(|t| t.recorded_on().clone());

        // Offload running-balance computation to a blocking thread.
        let handle = task::spawn_blocking(move || {
            let mut balance: i64 = 0;
            let mut entries = Vec::with_capacity(txs.len());

            for tx in txs {
                // Simple model: outgoing reduces balance, incoming increases
                let delta = match tx.transaction_type() {
                    TransactionType::Transfer | TransactionType::Withdrawal | TransactionType::Send => -tx.value_cents(),
                    TransactionType::Deposit => tx.value_cents(),
                };

                balance += delta;

                entries.push(AccountStatementEntry {
                    transaction_id: tx.id().expect("rehydrated transaction must have id"),
                    recorded_on: tx.recorded_on().clone(),
                    description: tx.description().to_string(),
                    transaction_type: tx.transaction_type().clone(),
                    value_cents: tx.value_cents(),
                    balance_after_cents: balance,
                });
            }

            entries
        });

        let entries = handle.await.map_err(|e| ServiceError::Concurrency(format!("join error: {e}")))?;
        Ok(entries)
    }

    pub async fn compute_user_statistics(&self, user_id: UserId, per_account_limit: i64, )
        -> ServiceResult<UserTransactionStatistics> {
        let accounts = self.account_repo.list_for_user(user_id).await?;

        // Load transactions for each account
        let mut all_transactions = Vec::new();
        for account in &accounts {
            let txs = self
                .tx_repo
                .list_for_account(account.id().unwrap(), per_account_limit, 0)
                .await?;
            all_transactions.extend(txs);
        }

        let stats_handle = task::spawn_blocking(move || {
            let total_incoming = AtomicI64::new(0);
            let total_outgoing = AtomicI64::new(0);
            let total_volume = AtomicI64::new(0);

            let per_type = Arc::new(Mutex::new(HashMap::<TransactionType, i64>::new()));
            let per_day = Arc::new(Mutex::new(BTreeMap::<NaiveDate, i64>::new()));

            let num_threads = thread::available_parallelism()
                .map(|n| n.get())
                .unwrap_or(1);
            let chunk_size = (all_transactions.len() / num_threads).max(1);

            thread::scope(|scope| {
                for chunk in all_transactions.chunks(chunk_size) {
                    let per_type = Arc::clone(&per_type);
                    let per_day = Arc::clone(&per_day);

                    // Share the same atomics with all workers by reference
                    let total_incoming = &total_incoming;
                    let total_outgoing = &total_outgoing;
                    let total_volume = &total_volume;

                    scope.spawn(move || {
                        for tx in chunk {
                            let value = tx.value_cents();

                            match tx.transaction_type() {
                                TransactionType::Transfer
                                | TransactionType::Send
                                | TransactionType::Withdrawal => {
                                    total_outgoing.fetch_add(value, Ordering::Relaxed);
                                }
                                TransactionType::Deposit => {
                                    total_incoming.fetch_add(value, Ordering::Relaxed);
                                }
                            }

                            total_volume.fetch_add(value.abs(), Ordering::Relaxed);

                            // Per-type totals under a mutex
                            {
                                let mut map = per_type.lock().expect("poisoned mutex");
                                *map.entry(*tx.transaction_type()).or_insert(0) += value;
                            }

                            // Per-day totals under a mutex
                            {
                                let date = tx.recorded_on().date_naive();
                                let mut map = per_day.lock().expect("poisoned mutex");
                                *map.entry(date).or_insert(0) += value;
                            }
                        }
                    });
                }
            });

            let total_incoming_cents = total_incoming.load(Ordering::Relaxed);
            let total_outgoing_cents = total_outgoing.load(Ordering::Relaxed);
            let total_volume_cents = total_volume.load(Ordering::Relaxed);

            let per_type_totals = Arc::try_unwrap(per_type)
                .map(|mutex| mutex.into_inner().unwrap())
                .unwrap_or_else(|arc| arc.lock().unwrap().clone());
            let per_day_totals = Arc::try_unwrap(per_day)
                .map(|mutex| mutex.into_inner().unwrap())
                .unwrap_or_else(|arc| arc.lock().unwrap().clone());

            UserTransactionStatistics {
                total_incoming_cents,
                total_outgoing_cents,
                total_volume_cents,
                per_type_totals,
                per_day_totals,
            }
        });

        let stats = stats_handle
            .await
            .map_err(|e| ServiceError::Concurrency(format!("join error: {e}")))?;

        Ok(stats)
    }


}