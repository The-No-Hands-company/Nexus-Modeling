use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};

/// Summary of a managed database returned via the API.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabaseInfo {
    pub id: uuid::Uuid,
    pub name: String,
    pub engine: String,
    pub status: DatabaseStatus,
    pub pool_size: u32,
    pub created_at: DateTime<Utc>,
    pub tags: std::collections::HashMap<String, String>,
}

/// Runtime status of a managed database.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum DatabaseStatus {
    Healthy,
    Degraded,
    Unavailable,
    Provisioning,
}

/// Configuration for a managed database — the canonical registry record.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabaseConfig {
    pub id: uuid::Uuid,
    pub name: String,
    pub engine: String, // "postgres" | "sqlite"
    pub connection_string: String,
    #[serde(default = "default_pool_size")]
    pub max_connections: u32,
    #[serde(default)]
    pub auto_migrate: bool,
    #[serde(default)]
    pub tags: std::collections::HashMap<String, String>,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

fn default_pool_size() -> u32 {
    10
}

impl DatabaseConfig {
    /// Derive the engine enum from the engine string.
    pub fn engine_enum(&self) -> Option<super::config::DatabaseEngine> {
        match self.engine.as_str() {
            "postgres" => Some(super::config::DatabaseEngine::Postgres),
            "sqlite" => Some(super::config::DatabaseEngine::Sqlite),
            _ => None,
        }
    }
}

/// Row stored in the control-plane `managed_databases` table.
/// Mirrors `DatabaseConfig` but uses sqlx-compatible types for direct queries.
#[derive(Debug, Clone, sqlx::FromRow)]
pub struct ManagedDatabaseRow {
    pub id: uuid::Uuid,
    pub name: String,
    pub engine: String,
    pub connection_string: String,
    pub max_connections: i32,
    pub auto_migrate: bool,
    pub tags: serde_json::Value,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

impl From<DatabaseConfig> for ManagedDatabaseRow {
    fn from(c: DatabaseConfig) -> Self {
        Self {
            id: c.id,
            name: c.name,
            engine: c.engine,
            connection_string: c.connection_string,
            max_connections: c.max_connections as i32,
            auto_migrate: c.auto_migrate,
            tags: serde_json::to_value(&c.tags).unwrap_or_default(),
            created_at: c.created_at,
            updated_at: c.updated_at,
        }
    }
}

impl From<ManagedDatabaseRow> for DatabaseConfig {
    fn from(r: ManagedDatabaseRow) -> Self {
        Self {
            id: r.id,
            name: r.name,
            engine: r.engine,
            connection_string: r.connection_string,
            max_connections: r.max_connections as u32,
            auto_migrate: r.auto_migrate,
            tags: serde_json::from_value(r.tags).unwrap_or_default(),
            created_at: r.created_at,
            updated_at: r.updated_at,
        }
    }
}

/// Request to provision a new database.
#[derive(Debug, Clone, Deserialize)]
pub struct ProvisionDatabaseRequest {
    pub name: String,
    pub engine: String,
    #[serde(default)]
    pub max_connections: Option<u32>,
    /// Optional override for the connection string.
    /// If absent, the server chooses a sensible default (e.g. `sqlite::memory:` for SQLite).
    #[serde(default)]
    pub connection_string: Option<String>,
    #[serde(default)]
    pub tags: std::collections::HashMap<String, String>,
}

/// Request to execute a SQL query against a managed database.
#[derive(Debug, Clone, Deserialize)]
pub struct QueryRequest {
    pub sql: String,
    #[serde(default)]
    pub readonly: bool,
}

/// Response from a query execution.
#[derive(Debug, Clone, Serialize)]
pub struct QueryResponse {
    pub columns: Vec<String>,
    pub rows: Vec<Vec<serde_json::Value>>,
    pub row_count: usize,
    pub elapsed_ms: f64,
}

/// Health check response.
#[derive(Debug, Clone, Serialize)]
pub struct HealthResponse {
    pub status: &'static str,
    pub version: &'static str,
    pub managed_databases: usize,
    pub uptime_seconds: f64,
    pub persistence: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cloud_system_id: Option<String>,
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── DatabaseConfig ↔ ManagedDatabaseRow ────────────────────

    #[test]
    fn config_to_row_roundtrip() {
        let cfg = DatabaseConfig {
            id: uuid::Uuid::new_v4(),
            name: "test-db".into(),
            engine: "postgres".into(),
            connection_string: "postgres://localhost/test".into(),
            max_connections: 20,
            auto_migrate: true,
            tags: {
                let mut m = std::collections::HashMap::new();
                m.insert("env".into(), "staging".into());
                m
            },
            created_at: chrono::Utc::now(),
            updated_at: chrono::Utc::now(),
        };

        let row = ManagedDatabaseRow::from(cfg.clone());
        let back = DatabaseConfig::from(row);

        assert_eq!(back.id, cfg.id);
        assert_eq!(back.name, cfg.name);
        assert_eq!(back.engine, cfg.engine);
        assert_eq!(back.connection_string, cfg.connection_string);
        assert_eq!(back.max_connections, cfg.max_connections);
        assert_eq!(back.auto_migrate, cfg.auto_migrate);
        assert_eq!(back.tags.get("env").unwrap(), "staging");
    }

    #[test]
    fn config_engine_enum_postgres() {
        let cfg = DatabaseConfig {
            id: uuid::Uuid::new_v4(),
            name: "pg".into(),
            engine: "postgres".into(),
            connection_string: "postgres:///".into(),
            max_connections: 10,
            auto_migrate: false,
            tags: Default::default(),
            created_at: chrono::Utc::now(),
            updated_at: chrono::Utc::now(),
        };
        assert_eq!(
            cfg.engine_enum(),
            Some(crate::config::DatabaseEngine::Postgres)
        );
    }

    #[test]
    fn config_engine_enum_sqlite() {
        let cfg = DatabaseConfig {
            id: uuid::Uuid::new_v4(),
            name: "sql".into(),
            engine: "sqlite".into(),
            connection_string: "sqlite://data.db".into(),
            max_connections: 10,
            auto_migrate: false,
            tags: Default::default(),
            created_at: chrono::Utc::now(),
            updated_at: chrono::Utc::now(),
        };
        assert_eq!(
            cfg.engine_enum(),
            Some(crate::config::DatabaseEngine::Sqlite)
        );
    }

    #[test]
    fn config_engine_enum_unknown() {
        let cfg = DatabaseConfig {
            id: uuid::Uuid::new_v4(),
            name: "x".into(),
            engine: "cassandra".into(),
            connection_string: "cassandra:///".into(),
            max_connections: 10,
            auto_migrate: false,
            tags: Default::default(),
            created_at: chrono::Utc::now(),
            updated_at: chrono::Utc::now(),
        };
        assert_eq!(cfg.engine_enum(), None);
    }

    // ── ProvisionDatabaseRequest ───────────────────────────────

    #[test]
    fn provision_request_minimal() {
        let json = serde_json::json!({"name": "mydb", "engine": "postgres"});
        let req: ProvisionDatabaseRequest = serde_json::from_value(json).unwrap();
        assert_eq!(req.name, "mydb");
        assert_eq!(req.engine, "postgres");
        assert_eq!(req.max_connections, None);
        assert!(req.tags.is_empty());
    }

    #[test]
    fn provision_request_full() {
        let json = serde_json::json!({
            "name": "mydb",
            "engine": "sqlite",
            "max_connections": 5,
            "connection_string": "sqlite::memory:",
            "tags": {"team": "platform"}
        });
        let req: ProvisionDatabaseRequest = serde_json::from_value(json).unwrap();
        assert_eq!(req.name, "mydb");
        assert_eq!(req.engine, "sqlite");
        assert_eq!(req.max_connections, Some(5));
        assert_eq!(req.connection_string.as_deref(), Some("sqlite::memory:"));
        assert_eq!(req.tags.get("team").unwrap(), "platform");
    }

    // ── HealthResponse ────────────────────────────────────────

    #[test]
    fn health_response_with_cloud() {
        let resp = HealthResponse {
            status: "ok",
            version: "0.1.0",
            managed_databases: 3,
            uptime_seconds: 123.4,
            persistence: true,
            cloud_system_id: Some("sys-abc".into()),
        };
        let json = serde_json::to_value(&resp).unwrap();
        assert_eq!(json["status"], "ok");
        assert_eq!(json["cloud_system_id"], "sys-abc");
        assert_eq!(json["persistence"], true);
    }

    #[test]
    fn health_response_without_cloud() {
        let resp = HealthResponse {
            status: "ok",
            version: "0.1.0",
            managed_databases: 0,
            uptime_seconds: 1.0,
            persistence: false,
            cloud_system_id: None,
        };
        let json = serde_json::to_value(&resp).unwrap();
        assert!(json.get("cloud_system_id").is_none());
    }

    // ── DatabaseStatus ────────────────────────────────────────

    #[test]
    fn database_status_serialization() {
        let statuses = [
            (DatabaseStatus::Healthy, "healthy"),
            (DatabaseStatus::Degraded, "degraded"),
            (DatabaseStatus::Unavailable, "unavailable"),
            (DatabaseStatus::Provisioning, "provisioning"),
        ];
        for (variant, expected) in &statuses {
            let json = serde_json::to_value(variant).unwrap();
            assert_eq!(json, serde_json::Value::String(expected.to_string()));
        }
    }
}
