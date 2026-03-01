mod db;
mod domain;

use db::*;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {

    let database_url = std::env::var("DATABASE_URL")
        .unwrap_or_else(|_| "postgres://mihai:admingentlix01@localhost:5432/gentlix_bank".to_string());

    let db = Db::new(&database_url).await?;

    db.ping().await?;
    println!("DB ping OK");



    Ok(())
}
