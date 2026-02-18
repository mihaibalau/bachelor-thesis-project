use sqlx::{PgPool, postgres::PgPoolOptions};
use std::time::Duration;

pub struct Db {
    pool: PgPool,
}

impl Db {
    pub async fn new(database_url: &str) -> Result<Self, sqlx::Error> {
        let pool = PgPoolOptions::new()
            .max_connections(5)
            .acquire_timeout(Duration::from_secs(5))
            .connect(database_url)
            .await?;

        Ok(Self { pool })
    }

    pub fn pool(&self) -> &PgPool {
        &self.pool
    }

    // =========================================
    // TEST QUERY 1: ping database
    // =========================================
    pub async fn ping(&self) -> Result<(), sqlx::Error> {
        sqlx::query("SELECT 1")
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    // =========================================
    // TEST QUERY 2: insert user
    // =========================================
    pub async fn insert_user(
        &self,
        tag: &str,
        email: &str,
        first_name: &str,
        last_name: &str,
        password_hash: &str,
    ) -> Result<i64, sqlx::Error> {

        let rec = sqlx::query!(
            r#"
            INSERT INTO users (tag, email, first_name, last_name, password_hash)
            VALUES ($1, $2, $3, $4, $5)
            RETURNING id
            "#,
            tag,
            email,
            first_name,
            last_name,
            password_hash
        )
            .fetch_one(&self.pool)
            .await?;

        Ok(rec.id)
    }

    // =========================================
    // TEST QUERY 3: get accounts for user
    // =========================================
    pub async fn get_accounts_for_user(
        &self,
        user_id: i64,
    ) -> Result<Vec<AccountRow>, sqlx::Error> {

        let rows = sqlx::query_as!(
            AccountRow,
            r#"
            SELECT id, user_id, account_type, currency, balance_cents, iban
            FROM accounts
            WHERE user_id = $1
            "#,
            user_id
        )
            .fetch_all(&self.pool)
            .await?;

        Ok(rows)
    }
}


#[derive(Debug)]
pub struct AccountRow {
    pub id: i64,
    pub user_id: i64,
    pub account_type: String,
    pub currency: String,
    pub balance_cents: i64,
    pub iban: String,
}
