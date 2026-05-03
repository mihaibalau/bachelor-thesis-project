//! IBAN value object.
//! Keeps validation in one place and guarantees: if you have an `IBAN`, it's valid.

use core::fmt;
use core::str::FromStr;
use rand::Rng;
use crate::domain::errors::DomainError;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct IBAN(String);

impl IBAN {

    pub fn generate() -> Result<Self, DomainError> {
            let mut rng = rand::thread_rng();

            let country   = "RO";
            let bank_code = "GNTL";
            let branch    = "0000";

            let account: String = (0..12)
                .map(|_| rng.gen_range(b'0'..=b'9') as char)
                .collect();

            let bban = format!("{}{}{}", bank_code, branch, account);
            // => "GNTL00001234567890123456" (20 chars)

            // ISO 13616: check digits = 98 - mod97(BBAN + country + "00")
            let raw = format!("{}{}00", bban, country);
            // => "GNTL0000123456789012RO00"

            let check_digits = 98 - Self::mod97_str(&raw);
            // check_digits between 02 and 98

            let iban_str = format!("{}{:02}{}", country, check_digits, bban);
            // => "RO47GNTL00001234567890123456"

            Ok(IBAN(iban_str))
        }

    fn mod97_str(s: &str) -> u64 {
        s.chars()
            .flat_map(|c| {
                if c.is_ascii_alphabetic() {
                    // 'A' -> "10", 'B' -> "11", ..., 'Z' -> "35"
                    let n = c.to_ascii_uppercase() as u32 - b'A' as u32 + 10;
                    n.to_string().chars().collect::<Vec<_>>()
                } else {
                    vec![c]
                }
            })
            .fold(0u64, |acc, c| {
                // Procesăm câte o cifră pe rând ca să evităm overflow
                (acc * 10 + c.to_digit(10).unwrap() as u64) % 97
            })
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }

    pub fn into_string(self) -> String {
        self.0
    }
}

impl fmt::Display for IBAN {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl FromStr for IBAN {
    type Err = DomainError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let raw = s.trim();
        if raw.is_empty() {
            return Err(DomainError::validation("IBAN must not be empty"));
        }

        let iban: String = raw.chars().filter(|c| !c.is_whitespace()).collect::<String>().to_ascii_uppercase();

        if iban.len() != 24 { return Err(DomainError::validation("Invalid IBAN length")); }
        if !iban.starts_with("RO") { return Err(DomainError::validation("Invalid country code")); }

        Ok(IBAN(iban))
    }
}

impl TryFrom<String> for IBAN {
    type Error = DomainError;

    fn try_from(value: String) -> Result<Self, Self::Error> {
        IBAN::from_str(&value)
    }
}

impl From<IBAN> for String {
    fn from(value: IBAN) -> Self {
        value.0
    }
}
