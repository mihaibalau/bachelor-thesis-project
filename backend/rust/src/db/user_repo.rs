
use crate::db::Db;
use crate::db::errors::RepoError;
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

        let row = row.ok_or_else(|| RepoError::not_found("user"))?;

        let user = User::try_from(row)?;
        Ok(user)
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

        let user = User::try_from(row)?;
        Ok(user)
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

        let user = User::try_from(row)?;
        Ok(user)
    }

    pub async fn insert(&self, user: &User) -> Result<UserId, RepoError> {

        if user.id().is_some() {
            return Err(RepoError::from(DomainError::validation(
                "Cannot insert a User that already has an id",
            )));
        }

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

}

impl TryFrom<UserRow> for User {
    type Error = DomainError;

    fn try_from(row: UserRow) -> Result<Self, Self::Error> {

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