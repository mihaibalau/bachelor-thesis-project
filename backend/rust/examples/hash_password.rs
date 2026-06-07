//! One-off helper: `cargo run --example hash_password -- admin`
use argon2::{Argon2, PasswordHasher};
use argon2::password_hash::{SaltString, rand_core::OsRng};

fn main() {
    let plain = std::env::args().nth(1).unwrap_or_else(|| "admin".to_string());
    let salt = SaltString::generate(&mut OsRng);
    let hash = Argon2::default()
        .hash_password(plain.as_bytes(), &salt)
        .expect("hash failed");
    println!("{hash}");
}
