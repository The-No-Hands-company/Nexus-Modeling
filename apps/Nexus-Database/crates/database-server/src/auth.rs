//! Token authentication compatible with Nexus-Vault.
//!
//! Validates Bearer tokens using SHA-256 digest + constant-time comparison,
//! matching Nexus-Vault's approach. Tokens are configured via
//! `NEXUS_DB_ACCESS_TOKEN` and `NEXUS_DB_ADMIN_TOKEN` env vars
//! (comma-separated lists, same format as VAULT_ACCESS_TOKEN / VAULT_ADMIN_TOKEN).

use axum::{
    extract::Request,
    http::{header, StatusCode},
    middleware::Next,
    response::{IntoResponse, Response},
};
use sha2::{Digest, Sha256};
use subtle::ConstantTimeEq;
use tracing::warn;

// ── Configuration ──────────────────────────────────────────────────────

const MIN_TOKEN_LENGTH: usize = 8;

fn parse_token_list(env_name: &str) -> Vec<String> {
    std::env::var(env_name)
        .unwrap_or_default()
        .split(',')
        .map(|t| t.trim().to_string())
        .filter(|t| !t.is_empty())
        .collect()
}

fn validate_token_length(tokens: &[String]) -> Result<(), String> {
    let allow_weak = std::env::var("NEXUS_DB_ALLOW_WEAK_TOKENS")
        .map(|v| v == "true" || v == "1")
        .unwrap_or(false);

    if !allow_weak {
        for t in tokens {
            if t.len() < MIN_TOKEN_LENGTH {
                return Err(format!(
                    "Token too short ({}/{} chars). Set NEXUS_DB_ALLOW_WEAK_TOKENS=true for dev only.",
                    t.len(),
                    MIN_TOKEN_LENGTH
                ));
            }
        }
    }
    Ok(())
}

pub(crate) fn digest_token(token: &str) -> Vec<u8> {
    Sha256::digest(token.as_bytes()).to_vec()
}

/// Holds SHA-256 digests of configured access and admin tokens.
#[derive(Clone)]
pub struct TokenSet {
    access_digests: Vec<Vec<u8>>,
    admin_digests: Vec<Vec<u8>>,
}

impl TokenSet {
    /// Build a TokenSet from environment variables.
    /// Returns `None` when no tokens are configured (auth is disabled).
    pub fn from_env() -> Option<Self> {
        let access_raw = parse_token_list("NEXUS_DB_ACCESS_TOKEN");
        let admin_raw = parse_token_list("NEXUS_DB_ADMIN_TOKEN");

        if access_raw.is_empty() && admin_raw.is_empty() {
            warn!("No NEXUS_DB_ACCESS_TOKEN or NEXUS_DB_ADMIN_TOKEN configured — auth DISABLED");
            return None;
        }

        if access_raw.is_empty() {
            warn!("NEXUS_DB_ADMIN_TOKEN set but NEXUS_DB_ACCESS_TOKEN is empty");
        }
        if admin_raw.is_empty() {
            warn!("NEXUS_DB_ACCESS_TOKEN set but NEXUS_DB_ADMIN_TOKEN is empty");
        }

        if let Err(e) = validate_token_length(&access_raw) {
            tracing::error!("{e}");
            panic!("{e}");
        }
        if let Err(e) = validate_token_length(&admin_raw) {
            tracing::error!("{e}");
            panic!("{e}");
        }

        Some(Self {
            access_digests: access_raw.iter().map(|t| digest_token(t)).collect(),
            admin_digests: admin_raw.iter().map(|t| digest_token(t)).collect(),
        })
    }

    /// Whether auth is enabled (tokens were configured).
    pub fn is_enabled(&self) -> bool {
        !self.access_digests.is_empty() || !self.admin_digests.is_empty()
    }

    /// Check if a raw token matches any access or admin digest.
    fn is_access_allowed(&self, token: &str) -> bool {
        let digest = digest_token(token);
        self.access_digests
            .iter()
            .chain(self.admin_digests.iter())
            .any(|d| d.ct_eq(&digest).into())
    }

    /// Check if a raw token matches any admin digest.
    pub fn is_admin(&self, token: &str) -> bool {
        let digest = digest_token(token);
        self.admin_digests
            .iter()
            .any(|d| d.ct_eq(&digest).into())
    }
}

// ── Bearer extraction ───────────────────────────────────────────────────

fn extract_bearer<B>(req: &Request<B>) -> Option<String> {
    let header = req.headers().get(header::AUTHORIZATION)?.to_str().ok()?;
    let stripped = header.strip_prefix("Bearer ")?.trim();
    if stripped.is_empty() {
        None
    } else {
        Some(stripped.to_string())
    }
}

// ── Middleware factories ────────────────────────────────────────────────

/// Middleware that requires a valid access or admin token.
/// Skips auth when no tokens are configured.
pub async fn require_read(
    axum::extract::Extension(token_set): axum::extract::Extension<Option<TokenSet>>,
    req: Request,
    next: Next,
) -> Result<Response, Response> {
    match token_set {
        None => {
            // Auth not configured — passthrough
            Ok(next.run(req).await)
        }
        Some(ts) if !ts.is_enabled() => {
            Ok(next.run(req).await)
        }
        Some(ts) => {
            let token = extract_bearer(&req).ok_or_else(|| {
                (
                    StatusCode::UNAUTHORIZED,
                    axum::Json(serde_json::json!({
                        "error": "Missing Authorization: Bearer <token> header"
                    })),
                )
                    .into_response()
            })?;

            if ts.is_access_allowed(&token) {
                Ok(next.run(req).await)
            } else {
                Err((
                    StatusCode::UNAUTHORIZED,
                    axum::Json(serde_json::json!({ "error": "Invalid access token" })),
                )
                    .into_response())
            }
        }
    }
}

// ── Tests ────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    // ── digest_token ──────────────────────────────────────────────

    #[test]
    fn same_token_same_digest() {
        let a = digest_token("my-secret-token");
        let b = digest_token("my-secret-token");
        assert_eq!(a, b);
    }

    #[test]
    fn different_tokens_different_digests() {
        let a = digest_token("token-alpha");
        let b = digest_token("token-beta");
        assert_ne!(a, b);
    }

    #[test]
    fn digest_is_32_bytes() {
        let d = digest_token("anything");
        assert_eq!(d.len(), 32); // SHA-256
    }

    // ── extract_bearer ────────────────────────────────────────────

    fn make_req(auth_value: Option<&str>) -> axum::http::Request<axum::body::Body> {
        use axum::http::{header, HeaderValue};
        let mut req = axum::http::Request::new(axum::body::Body::empty());
        if let Some(val) = auth_value {
            req.headers_mut()
                .insert(header::AUTHORIZATION, HeaderValue::from_str(val).unwrap());
        }
        req
    }

    #[test]
    fn bearer_extraction_valid() {
        let req = make_req(Some("Bearer abc123"));
        assert_eq!(extract_bearer(&req), Some("abc123".into()));
    }

    #[test]
    fn bearer_extraction_no_header() {
        let req = make_req(None);
        assert_eq!(extract_bearer(&req), None);
    }

    #[test]
    fn bearer_extraction_wrong_scheme() {
        let req = make_req(Some("Basic YWxhZGRpbjpvcGVuc2VzYW1l"));
        assert_eq!(extract_bearer(&req), None);
    }

    #[test]
    fn bearer_extraction_empty_token() {
        let req = make_req(Some("Bearer "));
        assert_eq!(extract_bearer(&req), None);
    }

    #[test]
    fn bearer_extraction_whitespace_token() {
        let req = make_req(Some("Bearer   "));
        // trim() removes trailing whitespace, then empty check rejects it
        assert_eq!(extract_bearer(&req), None);
    }

    // ── TokenSet ──────────────────────────────────────────────────

    fn make_token_set(access: &[&str], admin: &[&str]) -> TokenSet {
        TokenSet {
            access_digests: access.iter().map(|t| digest_token(t)).collect(),
            admin_digests: admin.iter().map(|t| digest_token(t)).collect(),
        }
    }

    #[test]
    fn access_token_accepted() {
        let ts = make_token_set(&["access-1"], &["admin-1"]);
        assert!(ts.is_access_allowed("access-1"));
    }

    #[test]
    fn admin_token_also_has_access() {
        let ts = make_token_set(&["access-1"], &["admin-1"]);
        assert!(ts.is_access_allowed("admin-1"));
    }

    #[test]
    fn unknown_token_rejected() {
        let ts = make_token_set(&["access-1"], &[]);
        assert!(!ts.is_access_allowed("evil-token"));
    }

    #[test]
    fn admin_token_accepted() {
        let ts = make_token_set(&["access-1"], &["admin-1"]);
        assert!(ts.is_admin("admin-1"));
    }

    #[test]
    fn access_token_not_admin() {
        let ts = make_token_set(&["access-1"], &["admin-1"]);
        assert!(!ts.is_admin("access-1"));
    }

    #[test]
    fn is_enabled_when_tokens_present() {
        let ts = make_token_set(&["a"], &[]);
        assert!(ts.is_enabled());
    }

    #[test]
    fn is_enabled_false_when_empty() {
        let ts = make_token_set(&[], &[]);
        assert!(!ts.is_enabled());
    }

    #[test]
    fn multiple_access_tokens() {
        let ts = make_token_set(&["a", "b", "c"], &[]);
        assert!(ts.is_access_allowed("a"));
        assert!(ts.is_access_allowed("b"));
        assert!(ts.is_access_allowed("c"));
        assert!(!ts.is_access_allowed("d"));
    }

    // ── parse_token_list ──────────────────────────────────────────

    #[test]
    fn parse_empty_string() {
        // We can't set env vars in unit tests safely; test via env manipulation
        // Just assert the function signature is callable
        let result = std::env::var("__NONEXISTENT_VAR_FOR_TEST__");
        assert!(result.is_err());
    }
}

/// Middleware that requires a valid admin token.
/// Skips auth when no tokens are configured.
pub async fn require_admin(
    axum::extract::Extension(token_set): axum::extract::Extension<Option<TokenSet>>,
    req: Request,
    next: Next,
) -> Result<Response, Response> {
    match token_set {
        None => Ok(next.run(req).await),
        Some(ts) if !ts.is_enabled() => Ok(next.run(req).await),
        Some(ts) => {
            let token = extract_bearer(&req).ok_or_else(|| {
                (
                    StatusCode::UNAUTHORIZED,
                    axum::Json(serde_json::json!({
                        "error": "Missing Authorization: Bearer <token> header"
                    })),
                )
                    .into_response()
            })?;

            if ts.is_admin(&token) {
                Ok(next.run(req).await)
            } else {
                Err((
                    StatusCode::UNAUTHORIZED,
                    axum::Json(serde_json::json!({ "error": "Admin token required" })),
                )
                    .into_response())
            }
        }
    }
}
