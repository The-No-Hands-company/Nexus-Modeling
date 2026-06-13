use axum::http::StatusCode;
use axum::response::{IntoResponse, Response};

/// Primary error type for Nexus Database operations.
#[derive(Debug, thiserror::Error)]
pub enum DatabaseError {
    #[error("database not found: {0}")]
    DatabaseNotFound(String),

    #[error("table not found: {0}")]
    TableNotFound(String),

    #[error("connection pool error: {0}")]
    ConnectionPool(String),

    #[error("migration error: {0}")]
    Migration(String),

    #[error("query error: {0}")]
    Query(String),

    #[error("configuration error: {0}")]
    Config(String),

    #[error("authentication error: {0}")]
    Auth(String),

    #[error("internal error: {0}")]
    Internal(String),
}

impl IntoResponse for DatabaseError {
    fn into_response(self) -> Response {
        let (status, msg) = match &self {
            DatabaseError::DatabaseNotFound(m) => (StatusCode::NOT_FOUND, m),
            DatabaseError::TableNotFound(m) => (StatusCode::NOT_FOUND, m),
            DatabaseError::ConnectionPool(m) => (StatusCode::SERVICE_UNAVAILABLE, m),
            DatabaseError::Migration(m) => (StatusCode::INTERNAL_SERVER_ERROR, m),
            DatabaseError::Query(m) => (StatusCode::BAD_REQUEST, m),
            DatabaseError::Config(m) => (StatusCode::INTERNAL_SERVER_ERROR, m),
            DatabaseError::Auth(m) => (StatusCode::UNAUTHORIZED, m),
            DatabaseError::Internal(m) => (StatusCode::INTERNAL_SERVER_ERROR, m),
        };

        let body = serde_json::json!({
            "error": msg,
            "code": status.as_u16()
        });

        (status, axum::Json(body)).into_response()
    }
}

pub type DatabaseResult<T> = Result<T, DatabaseError>;

#[cfg(test)]
mod tests {
    use super::*;
    use axum::response::IntoResponse;

    // Helper: extract status code and body from an IntoResponse
    fn response_parts(err: DatabaseError) -> (u16, serde_json::Value) {
        let resp = err.into_response();
        let status = resp.status().as_u16();
        let rt = tokio::runtime::Runtime::new().unwrap();
        let body: serde_json::Value = rt.block_on(async {
            let bytes = axum::body::to_bytes(resp.into_body(), usize::MAX)
                .await
                .unwrap_or_default();
            serde_json::from_slice(&bytes).unwrap_or_default()
        });
        (status, body)
    }

    #[test]
    fn database_not_found_returns_404() {
        let (code, body) = response_parts(DatabaseError::DatabaseNotFound("test-db".into()));
        assert_eq!(code, 404);
        assert_eq!(body["code"], 404);
        assert!(body["error"].as_str().unwrap().contains("test-db"));
    }

    #[test]
    fn table_not_found_returns_404() {
        let (code, _body) = response_parts(DatabaseError::TableNotFound("users".into()));
        assert_eq!(code, 404);
    }

    #[test]
    fn connection_pool_returns_503() {
        let (code, _body) =
            response_parts(DatabaseError::ConnectionPool("timeout".into()));
        assert_eq!(code, 503);
    }

    #[test]
    fn migration_returns_500() {
        let (code, _body) =
            response_parts(DatabaseError::Migration("schema mismatch".into()));
        assert_eq!(code, 500);
    }

    #[test]
    fn query_returns_400() {
        let (code, body) =
            response_parts(DatabaseError::Query("syntax error near X".into()));
        assert_eq!(code, 400);
        assert_eq!(body["code"], 400);
    }

    #[test]
    fn config_returns_500() {
        let (code, _body) =
            response_parts(DatabaseError::Config("bad port".into()));
        assert_eq!(code, 500);
    }

    #[test]
    fn auth_returns_401() {
        let (code, body) =
            response_parts(DatabaseError::Auth("invalid token".into()));
        assert_eq!(code, 401);
        assert_eq!(body["code"], 401);
    }

    #[test]
    fn internal_returns_500() {
        let (code, _body) =
            response_parts(DatabaseError::Internal("panic".into()));
        assert_eq!(code, 500);
    }

    #[test]
    fn error_display_contains_message() {
        let err = DatabaseError::DatabaseNotFound("xyz".into());
        assert!(err.to_string().contains("xyz"));
    }

    // ── DatabaseResult smoke ──────────────────────────────────────

    #[test]
    fn ok_result_typecheck() {
        let r: DatabaseResult<i32> = Ok(42);
        assert_eq!(r.unwrap(), 42);
    }

    #[test]
    fn err_result_typecheck() {
        let r: DatabaseResult<i32> =
            Err(DatabaseError::Internal("boom".into()));
        assert!(r.is_err());
    }
}
