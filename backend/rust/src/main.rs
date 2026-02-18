mod db;

use db::test::Db;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {

    let database_url = std::env::var("DATABASE_URL")
        .unwrap_or_else(|_| "postgres://mihai:admingentlix01@localhost:5432/gentlix_bank".to_string());

    let db = Db::new(&database_url).await?;

    db.ping().await?;
    println!("DB ping OK");

    let user_id = db.insert_user(
        "mihiblu",
        "mihiblu00@gmail.com",
        "Mihai",
        "Balau",
        "fake_hash",
    ).await?;

    println!("Inserted user_id = {user_id}");

    let accounts = db.get_accounts_for_user(user_id).await?;
    println!("Accounts for user: {accounts:#?}");

    Ok(())
}
