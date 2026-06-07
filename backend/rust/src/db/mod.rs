pub mod user_repo;
pub mod account_repo;
pub mod transaction_repo;
pub mod affiliate_repo;
pub mod errors;

use sqlx::{PgPool, postgres::PgPoolOptions};


#[derive(Clone)]
pub struct Db {
    pool: PgPool,
}

impl Db {
    pub async fn new(database_url: &str) -> Result<Self, sqlx::Error> {
        // 1. Open a bounded connection pool to Postgres
        let pool = PgPoolOptions::new()
            .max_connections(5)
            .connect(database_url)
            .await?;

        Ok(Self { pool })
    }

    pub fn pool(&self) -> &PgPool {
        &self.pool
    }

    pub async fn ping(&self) -> Result<(), sqlx::Error> {
        sqlx::query("SELECT 1")
            .execute(&self.pool)
            .await?;
        Ok( ())
    }
}