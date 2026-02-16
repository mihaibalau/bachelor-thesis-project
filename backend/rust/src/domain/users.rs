
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
    birth_date: Option<String>,

    password_hash: String,
}

impl User {
    /// Create a new user (not yet persisted in DB, so no id yet).
    pub fn create(
        tag: impl Into<String>,
        email: Email,
        first_name: impl Into<String>,
        last_name: impl Into<String>,
        phone: Option<String>,
        birth_date: Option<String>,
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

    /// Rehydrate an existing user loaded from DB (id is known).
    pub fn rehydrate(
        id: UserId,
        tag: impl Into<String>,
        email: Email,
        first_name: impl Into<String>,
        last_name: impl Into<String>,
        phone: Option<String>,
        birth_date: Option<String>,
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
        birth_date: Option<String>,
        password_hash: impl Into<String>,
    ) -> Result<Self, DomainError> {
        let tag = normalize_required(tag.into(), "Tag")?;
        let first_name = normalize_required(first_name.into(), "First name")?;
        let last_name = normalize_required(last_name.into(), "Last name")?;
        let password_hash = normalize_required(password_hash.into(), "Password hash")?;

        let phone = normalize_optional(phone);
        let birth_date = normalize_optional(birth_date);

        if let Some(ref d) = birth_date {
            if d.len() != 10 {
                return Err(DomainError::validation(
                    "Birth date must look like YYYY-MM-DD",
                ));
            }
        }

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

    // Getters
    pub fn id(&self) -> Option<UserId> { self.id }
    pub fn tag(&self) -> &str { &self.tag }
    pub fn email(&self) -> &Email { &self.email }
    pub fn first_name(&self) -> &str { &self.first_name }
    pub fn last_name(&self) -> &str { &self.last_name }
    pub fn phone(&self) -> Option<&str> { self.phone.as_deref() }
    pub fn birth_date(&self) -> Option<&str> { self.birth_date.as_deref() }
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

    pub fn set_birth_date(&mut self, birth_date: Option<String>) -> Result<(), DomainError> {
        let birth_date = normalize_optional(birth_date);
        if let Some(ref d) = birth_date {
            if d.len() != 10 {
                return Err(DomainError::validation(
                    "Birth date must look like YYYY-MM-DD",
                ));
            }
        }
        self.birth_date = birth_date;
        Ok(())
    }

    pub fn set_password_hash(&mut self, password_hash: impl Into<String>) -> Result<(), DomainError> {
        self.password_hash = normalize_required(password_hash.into(), "Password hash")?;
        Ok(())
    }

    /// Set id only after DB insert
    pub(crate) fn set_id_after_insert(&mut self, id: UserId) {
        self.id = Some(id);
    }
}

fn normalize_required(s: String, field: &str) -> Result<String, DomainError> {
    let v = s.trim().to_string();
    if v.is_empty() {
        return Err(DomainError::validation(format!("{field} must not be empty")));
    }
    Ok(v)
}

fn normalize_optional(v: Option<String>) -> Option<String> {
    v.map(|x| x.trim().to_string()).filter(|x| !x.is_empty())
}
