mod user_repo;
mod account_repo;
mod transaction_repo;
mod affiliate_repo;
mod errors;

use sqlx::{PgPool, postgres::PgPoolOptions};
use std::time::Duration;


#[derive(Clone)]
pub struct Db {
    pool: PgPool,
}

impl Db {
    pub async fn new(database_url: &str) -> Result<Self, sqlx::Error> {
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