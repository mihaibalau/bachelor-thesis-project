use crate::db::Db;
use crate::db::errors::RepoError;
use crate::domain::errors::DomainError;
use crate::domain::ids::{AccountId, TransactionId};
use crate::domain::transaction::Transaction;
use crate::domain::value::transaction_type::TransactionType;

use chrono::{DateTime, Utc};

#[derive(Debug)]
struct TransactionRow {
    id: i64,
    from_account_id: i64,
    to_account_id: i64,
    transaction_type: String,
    value_cents: i64,
    recorded_on: DateTime<Utc>,
    description: String,
}

pub struct TransactionRepo {
    db: Db,
}

impl TransactionRepo {
    pub fn new(db: Db) -> Self {
        Self { db }
    }

    pub async fn get_by_id(&self, transaction_id: TransactionId) -> Result<Transaction, RepoError> {
        let row = sqlx::query_as!(
            TransactionRow,
            r#"
            SELECT id,
                   from_account_id,
                   to_account_id,
                   transaction_type,
                   value_cents,
                   recorded_on,
                   description
            FROM transactions
            WHERE id = $1
            "#,
            transaction_id.0
        )
            .fetch_optional(self.db.pool())
            .await?;

        let row = row.ok_or_else(|| RepoError::not_found("transaction"))?;
        Ok(Transaction::try_from(row)?)
    }

    pub async fn insert(&self, tx: &Transaction) -> Result<TransactionId, RepoError> {
        if tx.id().is_some() {
            return Err(RepoError::from(DomainError::validation(
                "Cannot insert a Transaction that already has an id",
            )));
        }

        let tx_type = tx.transaction_type_str();
        let recorded_on = tx.recorded_on();

        let rec = sqlx::query!(
            r#"
            INSERT INTO transactions (
                from_account_id,
                to_account_id,
                transaction_type,
                value_cents,
                recorded_on,
                description
            )
            VALUES ($1, $2, $3, $4, $5, $6)
            RETURNING id
            "#,
            tx.from_account_id().0,
            tx.to_account_id().0,
            tx_type,
            tx.value_cents(),
            recorded_on,
            tx.description(),
        )
            .fetch_one(self.db.pool())
            .await?;

        Ok(TransactionId(rec.id))
    }

    pub async fn list_for_account(&self, account_id: AccountId, limit: i64, offset: i64, ) -> Result<Vec<Transaction>, RepoError> {
        let rows = sqlx::query_as!(
            TransactionRow,
            r#"
            SELECT id,
                   from_account_id,
                   to_account_id,
                   transaction_type,
                   value_cents,
                   recorded_on,
                   description
            FROM transactions
            WHERE from_account_id = $1 OR to_account_id = $1
            ORDER BY recorded_on DESC, id DESC
            LIMIT $2 OFFSET $3
            "#,
            account_id.0,
            limit,
            offset
        )
            .fetch_all(self.db.pool())
            .await?;

        let mut out = Vec::with_capacity(rows.len());
        for row in rows {
            out.push(Transaction::try_from(row)?);
        }
        Ok(out)
    }
}

impl TryFrom<TransactionRow> for Transaction {
    type Error = DomainError;

    fn try_from(row: TransactionRow) -> Result<Self, Self::Error> {
        let id = TransactionId(row.id);

        let from_account_id = AccountId(row.from_account_id);
        let to_account_id = AccountId(row.to_account_id);

        let tx_type: TransactionType = row.transaction_type.parse()?;

        Transaction::rehydrate(
            id,
            from_account_id,
            to_account_id,
            tx_type,
            row.value_cents,
            row.recorded_on,
            row.description,
        )
    }
}
