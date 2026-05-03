use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Claims {
    pub sub: i64,
    pub tag: String,
    pub exp: usize,     // expiry Unix timestamp
}

/// Input DTO
#[derive(Debug)]
pub struct LoginUserCommand {
    pub email: String,
    pub password: String,
}

#[derive(Debug)]
pub struct LoginResult {
    pub token: String,
    pub user_id: i64,
}