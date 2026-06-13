use crate::config::DatabaseEngine;
use crate::error::{DatabaseError, DatabaseResult};
use crate::models::{DatabaseConfig, QueryResponse};
use sqlx::{Column, Row, ValueRef};
use std::time::Instant;

/// A connection pool for a managed database — abstracts over backend engines.
#[derive(Clone, Debug)]
pub enum DatabasePool {
    Postgres(sqlx::PgPool),
    Sqlite(sqlx::SqlitePool),
}

impl DatabasePool {
    /// Create a new connection pool from a database config.
    /// Returns an error if the connection string is invalid or the database is unreachable.
    pub async fn connect(config: &DatabaseConfig) -> DatabaseResult<Self> {
        let engine = DatabaseEngine::from_scheme(&config.engine)?;
        match engine {
            DatabaseEngine::Postgres => {
                let pool = sqlx::PgPool::connect(&config.connection_string)
                    .await
                    .map_err(|e| DatabaseError::ConnectionPool(e.to_string()))?;
                Ok(Self::Postgres(pool))
            }
            DatabaseEngine::Sqlite => {
                let pool = sqlx::SqlitePool::connect(&config.connection_string)
                    .await
                    .map_err(|e| DatabaseError::ConnectionPool(e.to_string()))?;
                Ok(Self::Sqlite(pool))
            }
        }
    }

    /// Close the pool gracefully.
    pub async fn close(self) {
        match self {
            Self::Postgres(pool) => pool.close().await,
            Self::Sqlite(pool) => pool.close().await,
        }
    }
    /// Execute a read query and return structured results.
    pub async fn execute_query(&self, sql: &str) -> DatabaseResult<QueryResponse> {
        let start = Instant::now();
        match self {
            Self::Postgres(pool) => execute_read_pg(pool, sql, start).await,
            Self::Sqlite(pool) => execute_read_sqlite(pool, sql, start).await,
        }
    }

    /// Execute a write statement (INSERT / UPDATE / DELETE) and return rows affected.
    pub async fn execute_write(&self, sql: &str) -> DatabaseResult<u64> {
        match self {
            Self::Postgres(pool) => {
                sqlx::query(sql)
                    .execute(pool)
                    .await
                    .map(|r| r.rows_affected())
                    .map_err(|e| DatabaseError::Query(e.to_string()))
            }
            Self::Sqlite(pool) => {
                sqlx::query(sql)
                    .execute(pool)
                    .await
                    .map(|r| r.rows_affected())
                    .map_err(|e| DatabaseError::Query(e.to_string()))
            }
        }
    }

    /// Check whether the pool is healthy with a simple ping.
    pub async fn health_check(&self) -> bool {
        match self {
            Self::Postgres(pool) => sqlx::query("SELECT 1").execute(pool).await.is_ok(),
            Self::Sqlite(pool) => sqlx::query("SELECT 1").execute(pool).await.is_ok(),
        }
    }
}

// ── Postgres helpers ──────────────────────────────────────────────

async fn execute_read_pg(
    pool: &sqlx::PgPool,
    sql: &str,
    start: Instant,
) -> DatabaseResult<QueryResponse> {
    let rows = sqlx::query(sql)
        .fetch_all(pool)
        .await
        .map_err(|e| DatabaseError::Query(e.to_string()))?;

    let columns: Vec<String> = if rows.is_empty() {
        vec![]
    } else {
        rows[0].columns().iter().map(|c| c.name().to_string()).collect()
    };

    let mut result_rows = Vec::with_capacity(rows.len());
    for row in &rows {
        let mut values = Vec::with_capacity(columns.len());
        for i in 0..columns.len() {
            values.push(pg_cell_to_json(row, i));
        }
        result_rows.push(values);
    }

    Ok(QueryResponse {
        columns,
        rows: result_rows,
        row_count: rows.len(),
        elapsed_ms: start.elapsed().as_secs_f64() * 1000.0,
    })
}

fn pg_cell_to_json(row: &sqlx::postgres::PgRow, idx: usize) -> serde_json::Value {
    // Check null first
    if let Ok(raw) = row.try_get_raw(idx) {
        if raw.is_null() {
            return serde_json::Value::Null;
        }
    }
    // Cascade: try each type until one succeeds
    if let Ok(n) = row.try_get::<i64, _>(idx) {
        return serde_json::Value::Number(n.into());
    }
    if let Ok(n) = row.try_get::<f64, _>(idx) {
        if let Some(num) = serde_json::Number::from_f64(n) {
            return serde_json::Value::Number(num);
        }
    }
    if let Ok(b) = row.try_get::<bool, _>(idx) {
        return serde_json::Value::Bool(b);
    }
    if let Ok(s) = row.try_get::<String, _>(idx) {
        return serde_json::Value::String(s);
    }
    serde_json::Value::Null
}

// ── SQLite helpers ────────────────────────────────────────────────

async fn execute_read_sqlite(
    pool: &sqlx::SqlitePool,
    sql: &str,
    start: Instant,
) -> DatabaseResult<QueryResponse> {
    let rows = sqlx::query(sql)
        .fetch_all(pool)
        .await
        .map_err(|e| DatabaseError::Query(e.to_string()))?;

    let columns: Vec<String> = if rows.is_empty() {
        vec![]
    } else {
        rows[0].columns().iter().map(|c| c.name().to_string()).collect()
    };

    let mut result_rows = Vec::with_capacity(rows.len());
    for row in &rows {
        let mut values = Vec::with_capacity(columns.len());
        for i in 0..columns.len() {
            values.push(sqlite_cell_to_json(row, i));
        }
        result_rows.push(values);
    }

    Ok(QueryResponse {
        columns,
        rows: result_rows,
        row_count: rows.len(),
        elapsed_ms: start.elapsed().as_secs_f64() * 1000.0,
    })
}

fn sqlite_cell_to_json(row: &sqlx::sqlite::SqliteRow, idx: usize) -> serde_json::Value {
    // Check null first
    if let Ok(raw) = row.try_get_raw(idx) {
        if raw.is_null() {
            return serde_json::Value::Null;
        }
    }
    // Cascade: try each type until one succeeds
    if let Ok(n) = row.try_get::<i64, _>(idx) {
        return serde_json::Value::Number(n.into());
    }
    if let Ok(n) = row.try_get::<f64, _>(idx) {
        if let Some(num) = serde_json::Number::from_f64(n) {
            return serde_json::Value::Number(num);
        }
    }
    if let Ok(b) = row.try_get::<bool, _>(idx) {
        return serde_json::Value::Bool(b);
    }
    if let Ok(s) = row.try_get::<String, _>(idx) {
        return serde_json::Value::String(s);
    }
    serde_json::Value::Null
}

// ── Tests ──────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::DatabaseEngine;

    /// Build an SQLite config pointing at an in-memory database.
    fn sqlite_config() -> DatabaseConfig {
        DatabaseConfig {
            id: uuid::Uuid::new_v4(),
            name: "test".into(),
            engine: "sqlite".into(),
            connection_string: "sqlite::memory:".into(),
            max_connections: 1,
            auto_migrate: false,
            tags: Default::default(),
            created_at: chrono::Utc::now(),
            updated_at: chrono::Utc::now(),
        }
    }

    #[tokio::test]
    async fn connect_sqlite_memory() {
        let pool = DatabasePool::connect(&sqlite_config()).await;
        assert!(pool.is_ok(), "should connect to :memory:");
    }

    #[tokio::test]
    async fn health_check_healthy() {
        let pool = DatabasePool::connect(&sqlite_config()).await.unwrap();
        assert!(pool.health_check().await);
    }

    #[tokio::test]
    async fn execute_read_smoke() {
        let pool = DatabasePool::connect(&sqlite_config()).await.unwrap();
        let result = pool.execute_query("SELECT 1 AS num").await.unwrap();
        assert_eq!(result.columns, vec!["num"]);
        assert_eq!(result.row_count, 1);
        assert!(result.elapsed_ms >= 0.0);
    }

    #[tokio::test]
    async fn execute_write_smoke() {
        let pool = DatabasePool::connect(&sqlite_config()).await.unwrap();
        // Create a table and insert
        pool.execute_write("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)")
            .await
            .unwrap();
        let affected = pool
            .execute_write("INSERT INTO t (id, name) VALUES (1, 'hello')")
            .await
            .unwrap();
        assert_eq!(affected, 1);

        // Verify with a read
        let result = pool.execute_query("SELECT * FROM t").await.unwrap();
        assert_eq!(result.row_count, 1);
    }

    #[tokio::test]
    async fn execute_query_empty_result() {
        let pool = DatabasePool::connect(&sqlite_config()).await.unwrap();
        pool.execute_write("CREATE TABLE x (a INTEGER)")
            .await
            .unwrap();
        let result = pool.execute_query("SELECT * FROM x").await.unwrap();
        assert_eq!(result.row_count, 0);
        assert!(result.columns.is_empty());
        assert!(result.rows.is_empty());
    }

    #[tokio::test]
    async fn execute_query_bad_sql() {
        let pool = DatabasePool::connect(&sqlite_config()).await.unwrap();
        let err = pool.execute_query("GARBAGE SQL").await.unwrap_err();
        match err {
            DatabaseError::Query(msg) => assert!(!msg.is_empty()),
            other => panic!("expected Query error, got {:?}", other),
        }
    }

    #[tokio::test]
    async fn close_does_not_panic() {
        let pool = DatabasePool::connect(&sqlite_config()).await.unwrap();
        pool.close().await; // should not panic
    }

    #[tokio::test]
    async fn connect_bad_connection_string() {
        let mut cfg = sqlite_config();
        cfg.connection_string = "postgres://nonexistent-host:5432/nope".into();
        let err = DatabasePool::connect(&cfg).await.unwrap_err();
        assert!(matches!(err, DatabaseError::ConnectionPool(_)));
    }

    // ── Engine::from_scheme integration with pool ──────────────────

    #[test]
    fn default_port_postgres() {
        assert_eq!(DatabaseEngine::Postgres.default_port(), 5432);
    }

    #[test]
    fn default_port_sqlite() {
        assert_eq!(DatabaseEngine::Sqlite.default_port(), 0);
    }
}
