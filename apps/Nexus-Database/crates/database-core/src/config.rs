use serde::{Deserialize, Serialize};

/// Supported database backends.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum DatabaseEngine {
    Postgres,
    Sqlite,
}

impl DatabaseEngine {
    /// Derive a default port for the engine.
    pub fn default_port(&self) -> u16 {
        match self {
            DatabaseEngine::Postgres => 5432,
            DatabaseEngine::Sqlite => 0,
        }
    }

    /// Parse from a connection-string scheme.
    pub fn from_scheme(s: &str) -> Result<Self, crate::error::DatabaseError> {
        match s {
            "postgres" | "postgresql" => Ok(Self::Postgres),
            "sqlite" => Ok(Self::Sqlite),
            other => Err(crate::error::DatabaseError::Config(format!(
                "unsupported engine: {}",
                other
            ))),
        }
    }

    /// Scheme used in connection strings.
    pub fn scheme(&self) -> &'static str {
        match self {
            DatabaseEngine::Postgres => "postgres",
            DatabaseEngine::Sqlite => "sqlite",
        }
    }
}

/// Server-level configuration loaded from environment or file.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServerConfig {
    /// Host to bind the HTTP server on.
    #[serde(default = "default_host")]
    pub host: String,

    /// Port to bind the HTTP server on.
    #[serde(default = "default_port")]
    pub port: u16,

    /// Postgres connection URL for the control-plane (registry persistence).
    /// When absent the server runs with in-memory state only.
    pub control_plane_url: Option<String>,

    /// Path to a configuration file for managed databases.
    pub databases_config: Option<String>,

    /// Log level.
    #[serde(default = "default_log_level")]
    pub log_level: String,
}

fn default_host() -> String {
    "0.0.0.0".into()
}

fn default_port() -> u16 {
    3000
}

fn default_log_level() -> String {
    "info".into()
}

impl ServerConfig {
    /// Load from environment variables prefixed with `NEXUS_DB_`.
    pub fn from_env() -> Result<Self, crate::error::DatabaseError> {
        envy::prefixed("NEXUS_DB_")
            .from_env()
            .map_err(|e| crate::error::DatabaseError::Config(e.to_string()))
    }

    pub fn listen_addr(&self) -> String {
        format!("{}:{}", self.host, self.port)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── DatabaseEngine ─────────────────────────────────────────

    #[test]
    fn engine_from_scheme_postgres() {
        assert_eq!(
            DatabaseEngine::from_scheme("postgres").unwrap(),
            DatabaseEngine::Postgres
        );
        assert_eq!(
            DatabaseEngine::from_scheme("postgresql").unwrap(),
            DatabaseEngine::Postgres
        );
    }

    #[test]
    fn engine_from_scheme_sqlite() {
        assert_eq!(
            DatabaseEngine::from_scheme("sqlite").unwrap(),
            DatabaseEngine::Sqlite
        );
    }

    #[test]
    fn engine_from_scheme_unsupported() {
        assert!(DatabaseEngine::from_scheme("mysql").is_err());
        assert!(DatabaseEngine::from_scheme("").is_err());
    }

    #[test]
    fn engine_scheme_roundtrip() {
        for engine in &[DatabaseEngine::Postgres, DatabaseEngine::Sqlite] {
            let scheme = engine.scheme();
            let parsed = DatabaseEngine::from_scheme(scheme).unwrap();
            assert_eq!(&parsed, engine);
        }
    }

    #[test]
    fn engine_default_port() {
        assert_eq!(DatabaseEngine::Postgres.default_port(), 5432);
        assert_eq!(DatabaseEngine::Sqlite.default_port(), 0);
    }

    // ── ServerConfig ──────────────────────────────────────────

    #[test]
    fn listen_addr_defaults() {
        let cfg = ServerConfig {
            host: "127.0.0.1".into(),
            port: 8080,
            control_plane_url: None,
            databases_config: None,
            log_level: "debug".into(),
        };
        assert_eq!(cfg.listen_addr(), "127.0.0.1:8080");
    }

    #[test]
    fn listen_addr_ipv6() {
        let cfg = ServerConfig {
            host: "::1".into(),
            port: 3000,
            control_plane_url: None,
            databases_config: None,
            log_level: "info".into(),
        };
        // The listen_addr method does a simple format!("{}:{}", host, port);
        // for bracketed IPv6 use `[::1]:3000` as the host value directly.
        assert_eq!(cfg.listen_addr(), "::1:3000");
    }

    #[test]
    fn server_config_json_roundtrip() {
        let json = serde_json::json!({
            "host": "0.0.0.0",
            "port": 5000,
            "control_plane_url": null,
            "databases_config": null,
            "log_level": "trace"
        });
        let cfg: ServerConfig = serde_json::from_value(json.clone()).unwrap();
        assert_eq!(cfg.host, "0.0.0.0");
        assert_eq!(cfg.port, 5000);
        assert_eq!(cfg.log_level, "trace");

        let back = serde_json::to_value(&cfg).unwrap();
        assert_eq!(back["host"], json["host"]);
        assert_eq!(back["port"], json["port"]);
    }
}
