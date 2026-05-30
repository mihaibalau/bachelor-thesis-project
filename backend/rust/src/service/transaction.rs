use std::collections::{BTreeMap, HashMap, HashSet};
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

const BANK_ACCOUNT_ID: AccountId = AccountId(1);

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

    async fn ensure_account_owned_by(&self, user_id: UserId, account_id: AccountId) -> ServiceResult<()> {
        let account = self.account_repo.get_by_id(account_id).await?;
        if account.user_id() != user_id { return Err(ServiceError::not_found("account")); }
        Ok(())
    }

    /// Thin wrappers to keep routes slim -------------------------------------------------
    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, account_id = %account_id.0, amount_units))]
    pub async fn record_deposit_for_user(&self, user_id: UserId, account_id: AccountId, amount_units: i64) -> ServiceResult<TransactionId> {
        self.ensure_account_owned_by(user_id, account_id).await?;
        self.record_deposit(account_id, amount_units).await
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, account_id = %account_id.0, amount_units))]
    pub async fn record_withdrawal_for_user(&self, user_id: UserId, account_id: AccountId, amount_units: i64) -> ServiceResult<TransactionId> {
        self.ensure_account_owned_by(user_id, account_id).await?;
        self.record_withdrawal(account_id, amount_units).await
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, from = %from_id.0, to = %recipient_account_id.0, value_cents))]
    pub async fn record_send_for_user(&self, user_id: UserId, from_id: AccountId, recipient_account_id: AccountId, value_cents: i64, message: String) -> ServiceResult<TransactionId> {
        self.ensure_account_owned_by(user_id, from_id).await?;
        self.record_send(from_id, recipient_account_id, value_cents, message).await
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, from = %from_id.0, to = %to_id.0, value_cents))]
    pub async fn record_transfer_for_user(&self, user_id: UserId, from_id: AccountId, to_id: AccountId, value_cents: i64) -> ServiceResult<TransactionId> {
        self.ensure_account_owned_by(user_id, from_id).await?;
        self.ensure_account_owned_by(user_id, to_id).await?;
        self.record_transfer(from_id, to_id, value_cents).await
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, from = %from_id.0, amount_units))]
    pub async fn record_payment_for_user(&self, user_id: UserId, from_id: AccountId, amount_units: i64, category: String, merchant_name: String, note: Option<String>) -> ServiceResult<TransactionId> {
        self.ensure_account_owned_by(user_id, from_id).await?;
        self.record_payment(from_id, amount_units, category, merchant_name, note).await
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, account_id = %account_id.0, limit))]
    pub async fn list_recent_for_user(&self, user_id: UserId, account_id: AccountId, limit: Option<i64>) -> ServiceResult<Vec<Transaction>> {
        self.ensure_account_owned_by(user_id, account_id).await?;
        let limit = limit.unwrap_or(10).max(1);
        self.list_for_account(account_id, limit, 0).await
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, account_id = %account_id.0))]
    pub async fn compute_account_statement_for_user_from_strings(
        &self,
        user_id: UserId,
        account_id: AccountId,
        from: Option<String>,
        to: Option<String>,
        limit: Option<i64>,
        offset: Option<i64>,
    ) -> ServiceResult<Vec<AccountStatementEntry>> {
        use chrono::{NaiveDate, TimeZone};
        use chrono::Utc;
        self.ensure_account_owned_by(user_id, account_id).await?;
        let from_dt = match from.as_ref() {
            Some(s) => Some(NaiveDate::parse_from_str(s, "%Y-%m-%d")
                .map_err(|e| ServiceError::Validation(e.to_string()))?
                .and_hms_opt(0,0,0).unwrap()
                .and_local_timezone(Utc).unwrap()),
            None => None,
        };
        let to_dt = match to.as_ref() {
            Some(s) => Some(NaiveDate::parse_from_str(s, "%Y-%m-%d")
                .map_err(|e| ServiceError::Validation(e.to_string()))?
                .and_hms_opt(23,59,59).unwrap()
                .and_local_timezone(Utc).unwrap()),
            None => None,
        };
        let query = AccountStatementQuery { account_id, from: from_dt, to: to_dt, limit: limit.unwrap_or(100), offset: offset.unwrap_or(0) };
        self.compute_account_statement(query).await
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, per_account_limit))]
    pub async fn compute_user_monthly_summary_for_user(&self, user_id: UserId, per_account_limit: Option<i64>) -> ServiceResult<UserTransactionStatistics> {
        let limit = per_account_limit.unwrap_or(500).clamp(1, 1000);
        // The current-month window is decided by the route handler (the agreed
        // "shaping" exception); when called directly this computes all-time stats.
        self.compute_user_statistics(user_id, limit, None, None).await
    }

    /// Record a new transaction
    ///
    /// This method:
    /// - validates using the domain `Transaction` constructor,
    /// - applies the monetary effect to the affected account balances
    ///   (debit source / credit destination) using the domain helpers,
    /// - persists the updated balances,
    /// - stores the transaction.
    ///
    /// The balance effect depends on the transaction type:
    /// - `Deposit`            : credit the destination account.
    /// - `Withdrawal`/`Payment`: debit the source account.
    /// - `Send`/`Transfer`    : debit the source AND credit the destination.
    ///
    /// Any operation that would overdraw the source account is rejected with a
    /// validation error ("insufficient funds") before the transaction row is
    /// written, so the ledger never records money that was not actually moved.
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

        // Apply the monetary effect to the affected accounts and persist it.
        match cmd.transaction_type {
            TransactionType::Deposit => {
                let mut to_acc = self.account_repo.get_by_id(cmd.to_account_id).await?;
                to_acc.credit(cmd.value_cents)?;
                self.account_repo.update(&to_acc).await?;
            }
            TransactionType::Withdrawal | TransactionType::Payment => {
                let mut from_acc = self.account_repo.get_by_id(cmd.from_account_id).await?;
                if from_acc.balance_cents() < cmd.value_cents {
                    return Err(ServiceError::Validation("insufficient funds".into()));
                }
                from_acc.debit(cmd.value_cents)?;
                self.account_repo.update(&from_acc).await?;
            }
            TransactionType::Send | TransactionType::Transfer => {
                let mut from_acc = self.account_repo.get_by_id(cmd.from_account_id).await?;
                let mut to_acc = self.account_repo.get_by_id(cmd.to_account_id).await?;
                if from_acc.balance_cents() < cmd.value_cents {
                    return Err(ServiceError::Validation("insufficient funds".into()));
                }
                from_acc.debit(cmd.value_cents)?;
                to_acc.credit(cmd.value_cents)?;
                self.account_repo.update(&from_acc).await?;
                self.account_repo.update(&to_acc).await?;
            }
        }

        let id = self.tx_repo.insert(&tx).await?;
        Ok(id)
    }

    /// Raw listing for an account
    pub async fn list_for_account(&self, account_id: AccountId, limit: i64, offset: i64)
        -> ServiceResult<Vec<Transaction>> {

        let limit = limit.clamp(1, 100);
        let offset = offset.max(0);

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

        let account_id = query.account_id;

        // The account's *current* balance reflects every transaction ever applied
        // to it. We use it to anchor the running balance: the opening balance is
        // reconstructed as `current - sum(all signed deltas)`, after which we can
        // replay forward to obtain a correct `balance_after` for each transaction.
        let account = self.account_repo.get_by_id(account_id).await?;
        let current_balance = account.balance_cents();

        // Load the FULL history for the account (date filtering + pagination are
        // applied *after* the running-balance computation, so a date-range query
        // is never truncated by an early LIMIT and pagination slices the correct,
        // already-filtered set).
        let all = self.tx_repo.list_for_account(account_id, i64::MAX, 0).await?;

        let from = query.from;
        let to = query.to;
        let offset = query.offset.max(0);
        let limit = query.limit;

        // Offload the CPU-heavy sort + accumulation to a blocking thread.
        let handle = task::spawn_blocking(move || {
            let mut all = all;

            // Signed delta relative to *this* account: a transaction credits the
            // account when it is the destination (`to`) and debits it when it is
            // the source (`from`). `list_for_account` only returns transactions
            // touching the account, so exactly one side matches.
            let delta = |tx: &Transaction| -> i64 {
                if tx.to_account_id() == account_id {
                    tx.value_cents()
                } else {
                    -tx.value_cents()
                }
            };

            // Chronological order (oldest -> newest), ties broken by id.
            all.sort_by(|a, b| {
                a.recorded_on()
                    .cmp(&b.recorded_on())
                    .then_with(|| a.id().map(|i| i.0).cmp(&b.id().map(|i| i.0)))
            });

            let total: i64 = all.iter().map(|t| delta(t)).sum();
            let mut running = current_balance - total;

            // Forward pass: tag every transaction with its balance-after.
            let mut rows: Vec<AccountStatementEntry> = Vec::with_capacity(all.len());
            for tx in &all {
                running += delta(tx);
                rows.push(AccountStatementEntry {
                    transaction_id: tx.id().expect("rehydrated transaction must have id"),
                    recorded_on: tx.recorded_on(),
                    description: tx.description().to_string(),
                    transaction_type: *tx.transaction_type(),
                    value_cents: tx.value_cents(),
                    balance_after_cents: running,
                });
            }

            // Date-range filter (applied before pagination).
            let mut filtered: Vec<AccountStatementEntry> = rows
                .into_iter()
                .filter(|e| {
                    from.map_or(true, |f| e.recorded_on >= f)
                        && to.map_or(true, |t| e.recorded_on <= t)
                })
                .collect();

            // Pagination mirrors the original "most recent first" semantics:
            // order newest -> oldest, take the requested slice, then present the
            // slice oldest -> newest (each entry keeps its absolute balance-after).
            filtered.sort_by(|a, b| {
                b.recorded_on
                    .cmp(&a.recorded_on)
                    .then_with(|| b.transaction_id.0.cmp(&a.transaction_id.0))
            });

            let off = offset as usize;
            let lim = if limit < 0 { 0usize } else { limit as usize };

            let mut page: Vec<AccountStatementEntry> =
                filtered.into_iter().skip(off).take(lim).collect();

            page.sort_by(|a, b| {
                a.recorded_on
                    .cmp(&b.recorded_on)
                    .then_with(|| a.transaction_id.0.cmp(&b.transaction_id.0))
            });

            page
        });

        let entries = handle.await.map_err(|e| ServiceError::Concurrency(format!("join error: {e}")))?;
        Ok(entries)
    }

    pub async fn compute_user_statistics(
        &self,
        user_id: UserId,
        per_account_limit: i64,
        from: Option<DateTime<Utc>>,
        to: Option<DateTime<Utc>>,
    ) -> ServiceResult<UserTransactionStatistics> {
        let accounts = self.account_repo.list_for_user(user_id).await?;

        // The set of accounts owned by this user. Direction (incoming vs
        // outgoing) is decided relative to this set, not by transaction type.
        let user_account_ids: HashSet<AccountId> =
            accounts.iter().filter_map(|a| a.id()).collect();

        // Load transactions for each account and DEDUPLICATE by id. An internal
        // Transfer/Send between two of the user's own accounts is returned once
        // per side; keeping it once prevents double counting.
        let mut unique: HashMap<TransactionId, Transaction> = HashMap::new();
        for account in &accounts {
            let txs = self
                .tx_repo
                .list_for_account(account.id().unwrap(), per_account_limit, 0)
                .await?;
            for tx in txs {
                if let Some(id) = tx.id() {
                    unique.entry(id).or_insert(tx);
                }
            }
        }
        let all_transactions: Vec<Transaction> = unique.into_values().collect();

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
                    let user_account_ids = &user_account_ids;

                    scope.spawn(move || {
                        for tx in chunk {
                            // Restrict to the requested [from, to] window.
                            let recorded = tx.recorded_on();
                            if let Some(f) = from { if recorded < f { continue; } }
                            if let Some(t) = to { if recorded > t { continue; } }

                            let value = tx.value_cents();

                            // Signed effect on the user: credited when one of the
                            // user's accounts is the destination, debited when one
                            // is the source. An internal transfer between two owned
                            // accounts nets to zero (so it is neither income nor
                            // spending), but still appears in per-type totals.
                            let to_owned = user_account_ids.contains(&tx.to_account_id());
                            let from_owned = user_account_ids.contains(&tx.from_account_id());
                            let net = (if to_owned { value } else { 0 })
                                - (if from_owned { value } else { 0 });

                            if net > 0 {
                                total_incoming.fetch_add(net, Ordering::Relaxed);
                            } else if net < 0 {
                                total_outgoing.fetch_add(-net, Ordering::Relaxed);
                            }

                            total_volume.fetch_add(net.abs(), Ordering::Relaxed);

                            // Per-type totals under a mutex (gross value per type).
                            {
                                let mut map = per_type.lock().expect("poisoned mutex");
                                *map.entry(*tx.transaction_type()).or_insert(0) += value;
                            }

                            // Per-day totals under a mutex: SIGNED net so that
                            // outgoing reduces the day's total (the route derives
                            // daily spending from the negative part).
                            {
                                let date = recorded.date_naive();
                                let mut map = per_day.lock().expect("poisoned mutex");
                                *map.entry(date).or_insert(0) += net;
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

    pub async fn record_deposit(
        &self,
        user_account_id: AccountId,
        amount_units: i64,
    ) -> ServiceResult<TransactionId> {
        if amount_units <= 0 {
            return Err(ServiceError::Validation(
                "deposit amount must be positive".into(),
            ));
        }

        let value_cents = amount_units
            .checked_mul(100)
            .ok_or_else(|| ServiceError::Validation("amount too large".into()))?;

        let cmd = RecordTransactionCommand {
            from_account_id: BANK_ACCOUNT_ID,
            to_account_id: user_account_id,
            transaction_type: TransactionType::Deposit,
            value_cents,
            description: "ATM Deposit".to_string(),
        };

        self.record_transaction(cmd).await
    }

    pub async fn record_withdrawal(
        &self,
        user_account_id: AccountId,
        amount_units: i64,
    ) -> ServiceResult<TransactionId> {
        if amount_units <= 0 {
            return Err(ServiceError::Validation(
                "withdrawal amount must be positive".into(),
            ));
        }

        let value_cents = amount_units
            .checked_mul(100)
            .ok_or_else(|| ServiceError::Validation("amount too large".into()))?;

        let cmd = RecordTransactionCommand {
            from_account_id: user_account_id,
            to_account_id: BANK_ACCOUNT_ID,
            transaction_type: TransactionType::Withdrawal,
            value_cents,
            description: "ATM Withdrawal".to_string(),
        };

        self.record_transaction(cmd).await
    }

    pub async fn record_send(
        &self,
        from_account_id: AccountId,
        recipient_account_id: AccountId,
        value_cents: i64,
        user_message: String,
    ) -> ServiceResult<TransactionId> {
        if value_cents <= 0 {
            return Err(ServiceError::Validation(
                "send amount must be positive".into(),
            ));
        }

        let from_acc = self.account_repo.get_by_id(from_account_id).await?;
        let to_acc = self.account_repo.get_by_id(recipient_account_id).await?;

        if from_acc.currency() != to_acc.currency() {
            return Err(ServiceError::Validation(
                "cannot send between accounts with different currencies (use exchange)".into(),
            ));
        }

        let description = format!("Send: {}", user_message.trim());

        let cmd = RecordTransactionCommand {
            from_account_id,
            to_account_id: recipient_account_id,
            transaction_type: TransactionType::Send,
            value_cents,
            description,
        };

        self.record_transaction(cmd).await
    }

    pub async fn record_transfer(
        &self,
        from_account_id: AccountId,
        to_account_id: AccountId,
        value_cents: i64,
    ) -> ServiceResult<TransactionId> {
        if value_cents <= 0 {
            return Err(ServiceError::Validation(
                "transfer amount must be positive".into(),
            ));
        }

        let from_acc = self.account_repo.get_by_id(from_account_id).await?;
        let to_acc = self.account_repo.get_by_id(to_account_id).await?;

        if from_acc.user_id() != to_acc.user_id() {
            return Err(ServiceError::Forbidden);
        }

        if from_acc.currency() != to_acc.currency() {
            return Err(ServiceError::Validation(
                "cross-currency transfer not supported yet".into(),
            ));
        }

        let description = format!(
            "Transfer {} {} -> {} {}",
            from_acc.account_type().as_str(),
            from_acc.currency().as_str(),
            to_acc.account_type().as_str(),
            to_acc.currency().as_str()
        );

        let cmd = RecordTransactionCommand {
            from_account_id,
            to_account_id,
            transaction_type: TransactionType::Transfer,
            value_cents,
            description,
        };

        self.record_transaction(cmd).await
    }

    pub async fn record_payment(
        &self,
        from_account_id: AccountId,
        amount_units: i64,
        category: String,
        merchant_name: String,
        note: Option<String>,
    ) -> ServiceResult<TransactionId> {
        if amount_units <= 0 {
            return Err(ServiceError::Validation(
                "payment amount must be positive".into(),
            ));
        }

        let value_cents = amount_units
            .checked_mul(100)
            .ok_or_else(|| ServiceError::Validation("amount too large".into()))?;

        let description = format!(
            "Payment | category: {} | merchant: {}{}",
            category.trim(),
            merchant_name.trim(),
            note
                .as_ref()
                .map(|n| format!(" | note: {}", n.trim()))
                .unwrap_or_default()
        );

        let cmd = RecordTransactionCommand {
            from_account_id,
            to_account_id: BANK_ACCOUNT_ID,
            transaction_type: TransactionType::Payment,
            value_cents,
            description,
        };

        self.record_transaction(cmd).await
    }
}