use chrono::NaiveDate;
use crate::domain::errors::DomainError;
use crate::domain::ids::UserId;
use crate::domain::value::email::Email;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct User {
    id: Option<UserId>,
    tag: String,
    email: Email,

    first_name: String,
    last_name: String,

    phone: Option<String>,
    birth_date: Option<chrono::NaiveDate>,

    password_hash: String,
}

impl User {
    pub fn create(
        tag: impl Into<String>,
        email: Email,
        first_name: impl Into<String>,
        last_name: impl Into<String>,
        phone: Option<String>,
        birth_date: Option<chrono::NaiveDate>,
        password_hash: impl Into<String>,
    ) -> Result<Self, DomainError> {
        Self::build(
            None,
            tag,
            email,
            first_name,
            last_name,
            phone,
            birth_date,
            password_hash,
        )
    }

    pub fn rehydrate(
        id: UserId,
        tag: impl Into<String>,
        email: Email,
        first_name: impl Into<String>,
        last_name: impl Into<String>,
        phone: Option<String>,
        birth_date: Option<chrono::NaiveDate>,
        password_hash: impl Into<String>,
    ) -> Result<Self, DomainError> {
        Self::build(
            Some(id),
            tag,
            email,
            first_name,
            last_name,
            phone,
            birth_date,
            password_hash,
        )
    }

    fn build(
        id: Option<UserId>,
        tag: impl Into<String>,
        email: Email,
        first_name: impl Into<String>,
        last_name: impl Into<String>,
        phone: Option<String>,
        birth_date: Option<chrono::NaiveDate>,
        password_hash: impl Into<String>,
    ) -> Result<Self, DomainError> {
        // 1. Trim and validate required string fields
        let tag = normalize_required(tag.into(), "Tag")?;
        let first_name = normalize_required(first_name.into(), "First name")?;
        let last_name = normalize_required(last_name.into(), "Last name")?;
        let password_hash = normalize_required(password_hash.into(), "Password hash")?;

        // 2. Normalize optional phone (empty string becomes None)
        let phone = normalize_optional(phone);

        Ok(Self {
            id,
            tag,
            email,
            first_name,
            last_name,
            phone,
            birth_date,
            password_hash,
        })
    }

    pub fn id(&self) -> Option<UserId> { self.id }
    pub fn tag(&self) -> &str { &self.tag }
    pub fn email(&self) -> &Email { &self.email }
    pub fn first_name(&self) -> &str { &self.first_name }
    pub fn last_name(&self) -> &str { &self.last_name }
    pub fn phone(&self) -> Option<&str> { self.phone.as_deref() }
    pub fn birth_date(&self) -> Option<NaiveDate> { self.birth_date}
    pub fn password_hash(&self) -> &str { &self.password_hash }

    pub fn set_tag(&mut self, tag: impl Into<String>) -> Result<(), DomainError> {
        self.tag = normalize_required(tag.into(), "Tag")?;
        Ok(())
    }

    pub fn set_email(&mut self, email: Email) {
        self.email = email;
    }

    pub fn set_first_name(&mut self, first_name: impl Into<String>) -> Result<(), DomainError> {
        self.first_name = normalize_required(first_name.into(), "First name")?;
        Ok(())
    }

    pub fn set_last_name(&mut self, last_name: impl Into<String>) -> Result<(), DomainError> {
        self.last_name = normalize_required(last_name.into(), "Last name")?;
        Ok(())
    }

    pub fn set_phone(&mut self, phone: Option<String>) {
        self.phone = normalize_optional(phone);
    }

    pub fn set_birth_date(&mut self, birth_date: Option<chrono::NaiveDate>) -> Result<(), DomainError> {
        self.birth_date = birth_date;
        Ok(())
    }

    pub fn set_password_hash(&mut self, password_hash: impl Into<String>) -> Result<(), DomainError> {
        self.password_hash = normalize_required(password_hash.into(), "Password hash")?;
        Ok(())
    }

    pub(crate) fn set_id_after_insert(&mut self, id: UserId) {
        self.id = Some(id);
    }
}

fn normalize_required(s: String, field: &str) -> Result<String, DomainError> {
    // Trim whitespace; reject if nothing remains
    let v = s.trim().to_string();
    if v.is_empty() {
        return Err(DomainError::validation(format!("{field} must not be empty")));
    }
    Ok(v)
}

fn normalize_optional(v: Option<String>) -> Option<String> {
    // Treat blank/whitespace-only strings as absent
    v.map(|x| x.trim().to_string()).filter(|x| !x.is_empty())
}
