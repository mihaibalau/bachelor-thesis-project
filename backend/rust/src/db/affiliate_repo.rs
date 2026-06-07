use crate::db::Db;
use crate::db::errors::RepoError;
use crate::domain::affiliate::Affiliate;
use crate::domain::errors::DomainError;
use crate::domain::ids::{AccountId, UserId};

#[derive(Debug)]
struct AffiliateRow {
    owner_user_id: i64,
    recipient_sub_account_id: i64,
    nickname: String,
}

pub struct AffiliateRepo {
    db: Db,
}

impl AffiliateRepo {
    pub fn new(db: Db) -> Self {
        Self { db }
    }

    pub async fn get(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, ) -> Result<Affiliate, RepoError> {
        let row = sqlx::query_as!(
            AffiliateRow,
            r#"
            SELECT owner_user_id, recipient_sub_account_id, nickname
            FROM affiliates
            WHERE owner_user_id = $1 AND recipient_sub_account_id = $2
            "#,
            owner_user_id.0,
            recipient_sub_account_id.0,
        )
            .fetch_optional(self.db.pool())
            .await?;

        let row = row.ok_or_else(|| RepoError::not_found("affiliate"))?;
        Ok(Affiliate::try_from(row)?)
    }

    pub async fn list_for_owner(&self, owner_user_id: UserId) -> Result<Vec<Affiliate>, RepoError> {
        let rows = sqlx::query_as!(
            AffiliateRow,
            r#"
            SELECT owner_user_id, recipient_sub_account_id, nickname
            FROM affiliates
            WHERE owner_user_id = $1
            ORDER BY recipient_sub_account_id
            "#,
            owner_user_id.0
        )
            .fetch_all(self.db.pool())
            .await?;

        let mut out = Vec::with_capacity(rows.len());
        for row in rows {
            out.push(Affiliate::try_from(row)?);
        }
        Ok(out)
    }

    pub async fn insert(&self, affiliate: &Affiliate) -> Result<(), RepoError> {
        sqlx::query!(
            r#"
            INSERT INTO affiliates (owner_user_id, recipient_sub_account_id, nickname)
            VALUES ($1, $2, $3)
            "#,
            affiliate.owner_user_id().0,
            affiliate.recipient_sub_account_id().0,
            affiliate.nickname(),
        )
            .execute(self.db.pool())
            .await?;

        Ok(())
    }

    pub async fn update_nickname(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, nickname: &str) -> Result<(), RepoError> {
        // 1. Reuse domain validation for the new nickname
        let _ = Affiliate::new(owner_user_id, recipient_sub_account_id, nickname.to_string())?;

        // 2. Apply update; treat zero rows as NotFound
        let result = sqlx::query!(
            r#"
            UPDATE affiliates
            SET nickname = $1
            WHERE owner_user_id = $2 AND recipient_sub_account_id = $3
            "#,
            nickname,
            owner_user_id.0,
            recipient_sub_account_id.0
        )
            .execute(self.db.pool())
            .await?;

        if result.rows_affected() == 0 {
            return Err(RepoError::not_found("affiliate"));
        }

        Ok(())
    }

    pub async fn delete(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<(), RepoError> {
        // 1. Delete composite key; treat zero rows as NotFound
        let result = sqlx::query!(
            r#"
            DELETE FROM affiliates
            WHERE owner_user_id = $1 AND recipient_sub_account_id = $2
            "#,
            owner_user_id.0,
            recipient_sub_account_id.0
        )
            .execute(self.db.pool())
            .await?;

        if result.rows_affected() == 0 {
            return Err(RepoError::not_found("affiliate"));
        }

        Ok(())
    }

    pub async fn exists(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, ) -> Result<bool, RepoError> {
        // Presence check via LIMIT 1 — no full row fetch needed
        let rec = sqlx::query!(
            r#"
            SELECT 1 as "exists!"
            FROM affiliates
            WHERE owner_user_id = $1 AND recipient_sub_account_id = $2
            LIMIT 1
            "#,
            owner_user_id.0,
            recipient_sub_account_id.0
        )
            .fetch_optional(self.db.pool())
            .await?;

        Ok(rec.is_some())
    }
}

impl TryFrom<AffiliateRow> for Affiliate {
    type Error = DomainError;

    fn try_from(row: AffiliateRow) -> Result<Self, Self::Error> {
        // Re-run domain validation when loading from DB
        Affiliate::new(
            UserId(row.owner_user_id),
            AccountId(row.recipient_sub_account_id),
            row.nickname,
        )
    }
}
