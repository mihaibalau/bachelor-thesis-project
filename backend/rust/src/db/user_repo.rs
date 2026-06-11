use tracing::debug;
use crate::db::Db;
use crate::db::errors::RepoError;
use crate::domain::account::Account;
use crate::domain::errors::DomainError;
use crate::domain::ids::UserId;
use crate::domain::user::User;
use crate::domain::value::email::Email;

#[derive(Debug)]
struct UserRow {
    id: i64,
    tag: String,
    email: String,
    first_name: String,
    last_name: String,
    phone: Option<String>,
    birth_date: Option<chrono::NaiveDate>,
    password_hash: String,
}

pub struct UserRepo {
    db: Db,
}

impl UserRepo {
    pub fn new(db: Db) -> Self {
        Self { db }
    }

    pub async fn get_by_id(&self, user_id: i64) -> Result<User, RepoError> {
        let row = sqlx::query_as!(
            UserRow,
            r#"
            SELECT id, tag, email, first_name, last_name, phone, birth_date, password_hash
            FROM users
            WHERE id = $1
            "#,
            user_id,
        )
            .fetch_optional(self.db.pool())
            .await?;

        // Map missing row to NotFound, then rehydrate domain entity
        let row = row.ok_or_else(|| RepoError::not_found("user"))?;
        Ok(User::try_from(row)?)
    }

    pub async fn get_by_email(&self, email: &str) -> Result<User, RepoError> {
        let row = sqlx::query_as!(
            UserRow,
            r#"
            SELECT id, tag, email, first_name, last_name, phone, birth_date, password_hash
            FROM users
            WHERE email = $1
            "#,
            email,
        )
            .fetch_optional(self.db.pool())
            .await?;

        let row = row.ok_or_else(|| RepoError::not_found("user"))?;
        Ok(User::try_from(row)?)
    }

    pub async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError> {
        let row = sqlx::query_as!(
            UserRow,
            r#"
            SELECT id, tag, email, first_name, last_name, phone, birth_date, password_hash
            FROM users
            WHERE tag = $1
            "#,
            tag,
        )
            .fetch_optional(self.db.pool())
            .await?;

        let row = row.ok_or_else(|| RepoError::not_found("user"))?;
        Ok(User::try_from(row)?)
    }

    pub async fn insert(&self, user: &User) -> Result<UserId, RepoError> {
        debug!("inserting user into DB");

        // 1. Only persist users without an assigned id
        if user.id().is_some() {
            return Err(RepoError::from(DomainError::validation(
                "Cannot insert a User that already has an id",
            )));
        }

        // 2. Insert and return generated id
        let rec = sqlx::query!(
            r#"
            INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
            VALUES ($1, $2, $3, $4, $5, $6, $7)
            RETURNING id
            "#,
            user.tag(),
            user.email().as_str(),
            user.first_name(),
            user.last_name(),
            user.phone(),
            user.birth_date(),
            user.password_hash(),
        )
            .fetch_one(self.db.pool())
            .await?;

        Ok(UserId(rec.id))
    }

    pub async fn insert_with_default_account(
        &self,
        user: &User,
        make_account: &(dyn Fn(UserId) -> Result<Account, DomainError> + Send + Sync),
    ) -> Result<UserId, RepoError> {
        // 1. Only persist users without an assigned id
        if user.id().is_some() {
            return Err(RepoError::from(DomainError::validation(
                "Cannot insert a User that already has an id",
            )));
        }

        // 2. Single DB transaction: insert the user and its default account
        //    together (rolls back automatically on any early return).
        let mut tx = self.db.pool().begin().await?;

        let rec = sqlx::query!(
            r#"
            INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
            VALUES ($1, $2, $3, $4, $5, $6, $7)
            RETURNING id
            "#,
            user.tag(),
            user.email().as_str(),
            user.first_name(),
            user.last_name(),
            user.phone(),
            user.birth_date(),
            user.password_hash(),
        )
            .fetch_one(&mut *tx)
            .await?;

        let user_id = UserId(rec.id);

        // 3. Build the default account for the new user id and insert it
        let account = make_account(user_id)?;
        let account_type = account.account_type();
        let currency = account.currency();
        sqlx::query!(
            r#"
            INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
            VALUES ($1, $2, $3, $4, $5)
            "#,
            account.user_id().0,
            account_type.as_str(),
            currency.as_str(),
            account.balance_cents(),
            account.iban().as_str(),
        )
            .execute(&mut *tx)
            .await?;

        tx.commit().await?;
        Ok(user_id)
    }

    pub async fn update(&self, user: &User) -> Result<(), RepoError> {
        // 1. Require a persisted id before updating
        let id = user.id().ok_or_else(|| {
            RepoError::from(DomainError::validation(
                "Cannot update a User without an id",
            ))
        })?;

        // 2. Apply changes; treat zero rows as NotFound
        let result = sqlx::query!(
            r#"
            UPDATE users
            SET tag = $1,
                email = $2,
                first_name = $3,
                last_name = $4,
                phone = $5,
                birth_date = $6,
                password_hash = $7
            WHERE id = $8
            "#,
            user.tag(),
            user.email().as_str(),
            user.first_name(),
            user.last_name(),
            user.phone(),
            user.birth_date(),
            user.password_hash(),
            id.0,
        )
            .execute(self.db.pool())
            .await?;

        if result.rows_affected() == 0 {
            return Err(RepoError::not_found("user"));
        }

        Ok(())
    }

    pub async fn delete(&self, user_id: UserId) -> Result<(), RepoError> {
        // Delete by id; treat zero rows as NotFound
        let result = sqlx::query!(
            r#"
            DELETE FROM users
            WHERE id = $1
            "#,
            user_id.0
        )
            .execute(self.db.pool())
            .await?;

        if result.rows_affected() == 0 {
            return Err(RepoError::not_found("user"));
        }

        Ok(())
    }
}

impl TryFrom<UserRow> for User {
    type Error = DomainError;

    fn try_from(row: UserRow) -> Result<Self, Self::Error> {
        // Parse email value object, then rehydrate with DB id
        let id = UserId(row.id);
        let email: Email = row.email.parse()?;

        User::rehydrate(
            id,
            row.tag,
            email,
            row.first_name,
            row.last_name,
            row.phone,
            row.birth_date,
            row.password_hash,
        )
    }
}