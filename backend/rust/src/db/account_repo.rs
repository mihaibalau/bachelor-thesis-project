use crate::db::Db;
use crate::db::errors::RepoError;
use crate::domain::account::Account;
use crate::domain::errors::DomainError;
use crate::domain::ids::{AccountId, UserId};
use crate::domain::value::account_type::AccountType;
use crate::domain::value::currency::Currency;
use crate::domain::value::iban::IBAN;

#[derive(Debug)]
pub struct AccountRow {
    id: i64,
    user_id: i64,
    account_type: String,
    currency: String,
    balance_cents: i64,
    iban: String,
}

pub struct AccountRepo {
    db: Db
}

impl AccountRepo {
    pub fn new(db: Db) -> Self {
        Self { db }
    }

    pub async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError> {
        let row = sqlx::query_as!(
            AccountRow,
            r#"
                SELECT id, user_id, account_type, currency, balance_cents, iban
                FROM accounts
                WHERE id = $1
            "#,
            account_id.0
        )
            .fetch_optional(self.db.pool())
            .await?;

        let row = row.ok_or_else(|| RepoError::not_found("account"))?;
        Ok(Account::try_from(row)?)
    }

    pub async fn get_by_iban(&self, iban: &str) -> Result<Account, RepoError> {
        let row = sqlx::query_as!(
            AccountRow,
            r#"
                SELECT id, user_id, account_type, currency, balance_cents, iban
                FROM accounts
                WHERE iban = $1
            "#,
            iban
        )
            .fetch_optional(self.db.pool())
            .await?;

        let row = row.ok_or_else(|| RepoError::not_found("account"))?;
        Ok(Account::try_from(row)?)
    }

    pub async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError>{
        let rows = sqlx::query_as!(
            AccountRow,
            r#"
                SELECT id, user_id, account_type, currency, balance_cents, iban
                FROM accounts
                WHERE user_id = $1
                ORDER BY id
            "#,
            user_id.0
        )
            .fetch_all(self.db.pool())
            .await?;

        let mut out = Vec::with_capacity(rows.len());
        for row in rows {
            out.push(Account::try_from(row)?);
        }
        Ok(out)
    }

    pub async fn insert(&self, account: &Account) -> Result<AccountId, RepoError> {
        if account.id().is_some() {
            return Err(RepoError::from(DomainError::validation(
                "Cannot insert an Account that already has an id",
            )));
        }

        let account_type = account.account_type();
        let currency = account.currency();

        let rec = sqlx::query!(
            r#"
                INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
                VALUES ($1, $2, $3, $4, $5)
                RETURNING id
            "#,
            account.user_id().0,
            account_type.as_str(),
            currency.as_str(),
            account.balance_cents(),
            account.iban().as_str(),
        )
            .fetch_one(self.db.pool())
            .await?;

        Ok(AccountId(rec.id))
    }

    pub async fn update(&self, account: &Account) -> Result<(), RepoError> {
        let id = account.id().ok_or_else(|| {
            RepoError::from(DomainError::validation(
                "Cannot update an Account without an id",
            ))
        })?;

        let account_type = account.account_type();
        let currency = account.currency();

        let result = sqlx::query!(
            r#"
                UPDATE accounts
                SET user_id = $1,
                    account_type = $2,
                    currency = $3,
                    balance_cents = $4,
                    iban = $5
                WHERE id = $6
            "#,
            account.user_id().0,
            account_type.as_str(),
            currency.as_str(),
            account.balance_cents(),
            account.iban().as_str(),
            id.0,
        )
            .execute(self.db.pool())
            .await?;

        if result.rows_affected() == 0 {
            return Err(RepoError::not_found("account"));
        }

        Ok(())
    }

    pub async fn delete(&self, account_id: AccountId) -> Result<(), RepoError> {
        let result = sqlx::query!(
            r#"
                DELETE FROM accounts
                WHERE id = $1
            "#,
            account_id.0
        )
            .execute(self.db.pool())
            .await?;

        if result.rows_affected() == 0 {
            return Err(RepoError::not_found("account"));
        }

        Ok(())
    }

    pub async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError> {
        let rec = sqlx::query!(
            r#"
                SELECT EXISTS (
                    SELECT 1
                    FROM accounts
                    WHERE iban = $1
                )
            "#,
            iban
        )
            .fetch_optional(self.db.pool())
            .await?;

        Ok(rec.is_some())
    }

    pub async fn exists_by_account_type(&self, user_id: UserId, account_type: AccountType) -> Result<bool, RepoError> {
        let rec = sqlx::query!(
            r#"
                SELECT EXISTS (
                    SELECT 1
                    FROM accounts
                    WHERE account_type = $1 AND user_id = $2
                )
            "#,
            account_type.as_str(),
            user_id.0
        )
            .fetch_optional(self.db.pool())
            .await?;

        Ok(rec.is_some())
    }
}

impl TryFrom<AccountRow> for Account {
    type Error = DomainError;

    fn try_from(row: AccountRow) -> Result<Self, Self::Error> {
        let id = AccountId(row.id);
        let user_id = UserId(row.user_id);

        let account_type: AccountType = row.account_type.parse()?;
        let currency: Currency = row.currency.parse()?;
        let iban: IBAN = row.iban.parse()?;

        Account::rehydrate(id, user_id, account_type, currency, row.balance_cents, iban)
    }
}