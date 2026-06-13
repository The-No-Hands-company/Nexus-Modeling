//! Nexus-Cloud Systems API registration.
//!
//! When `NEXUS_CLOUD_URL` is set, the server registers itself with the
//! Nexus-Cloud control plane on startup, maintains a heartbeat, and
//! deregisters on graceful shutdown. This enables the cloud orchestrator
//! to discover and route to this database service instance.
//!
//! ## Systems API contract
//!
//! ### POST /systems/register
//! Registers a new system service. Returns a system ID.
//!
//! ### PUT /systems/:id/heartbeat
//! Periodic health pulse. The cloud marks the system as unhealthy if
//! heartbeats stop arriving.
//!
//! ### DELETE /systems/:id
//! Deregisters the system on graceful shutdown.

use reqwest::Client;
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use std::time::Duration;
use tracing::{info, warn};

// ── Config ─────────────────────────────────────────────────────────────

/// Configuration for Nexus-Cloud integration, read from environment.
#[derive(Clone)]
pub struct CloudConfig {
    pub base_url: String,
    pub token: Option<String>,
    pub heartbeat_interval: Duration,
}

impl CloudConfig {
    /// Read from `NEXUS_CLOUD_URL` and `NEXUS_CLOUD_TOKEN`.
    /// Returns `None` when `NEXUS_CLOUD_URL` is not set (cloud disabled).
    pub fn from_env() -> Option<Self> {
        let base_url = std::env::var("NEXUS_CLOUD_URL").ok()?;
        let token = std::env::var("NEXUS_CLOUD_TOKEN").ok();
        Some(Self {
            base_url: base_url.trim_end_matches('/').to_string(),
            token,
            heartbeat_interval: Duration::from_secs(30),
        })
    }
}

// ── API types ──────────────────────────────────────────────────────────

/// Payload sent to register with Nexus-Cloud.
#[derive(Debug, Serialize)]
struct RegisterRequest {
    service_type: String,
    name: String,
    version: String,
    endpoint: String,
    capabilities: Vec<String>,
}

/// Response from a successful registration.
#[derive(Debug, Deserialize)]
struct RegisterResponse {
    id: String,
    #[allow(dead_code)]
    #[serde(default)]
    heartbeat_interval_secs: Option<u64>,
}

/// Payload sent on each heartbeat pulse.
#[derive(Debug, Serialize)]
struct HeartbeatRequest {
    status: String,
    managed_databases: usize,
    uptime_seconds: f64,
}

// ── Client ─────────────────────────────────────────────────────────────

/// Handle to the Nexus-Cloud control plane.
///
/// Spawns a background heartbeat task. Call `shutdown()` before process exit
/// to deregister gracefully.
#[derive(Clone)]
pub struct CloudClient {
    inner: Option<Arc<CloudInner>>,
}

struct CloudInner {
    client: Client,
    config: CloudConfig,
    system_id: String,
    shutdown_tx: tokio::sync::watch::Sender<bool>,
}

impl CloudClient {
    /// Attempt to register with Nexus-Cloud.
    /// Returns `None` when cloud is not configured or registration fails
    /// (failure is non-fatal — the server continues standalone).
    pub async fn register(
        config: Option<CloudConfig>,
        listen_addr: String,
    ) -> Option<Self> {
        let config = config?;

        let client = Client::builder()
            .timeout(Duration::from_secs(10))
            .build()
            .ok()?;

        let endpoint = format!("http://{}", listen_addr);

        let body = RegisterRequest {
            service_type: "database".into(),
            name: "nexus-database".into(),
            version: env!("CARGO_PKG_VERSION").into(),
            endpoint,
            capabilities: vec![
                "provision".into(),
                "query".into(),
                "health".into(),
            ],
        };

        let url = format!("{}/systems/register", config.base_url);
        let mut req = client.post(&url).json(&body);
        if let Some(ref token) = config.token {
            req = req.header("Authorization", format!("Bearer {}", token));
        }

        match req.send().await {
            Ok(resp) if resp.status().is_success() => {
                match resp.json::<RegisterResponse>().await {
                    Ok(reg) => {
                        info!(
                            system_id = %reg.id,
                            "Registered with Nexus-Cloud"
                        );
                        let (shutdown_tx, _) = tokio::sync::watch::channel(false);
                        let inner = Arc::new(CloudInner {
                            client,
                            config,
                            system_id: reg.id,
                            shutdown_tx,
                        });
                        Some(Self { inner: Some(inner) })
                    }
                    Err(e) => {
                        warn!("Nexus-Cloud returned unexpected register response: {e}");
                        None
                    }
                }
            }
            Ok(resp) => {
                let status = resp.status();
                let body = resp.text().await.unwrap_or_default();
                warn!("Nexus-Cloud registration rejected ({status}): {body}");
                None
            }
            Err(e) => {
                warn!("Cannot reach Nexus-Cloud at {}: {e}", config.base_url);
                None
            }
        }
    }

    /// Whether cloud registration is active.
    #[allow(dead_code)]
    pub fn is_registered(&self) -> bool {
        self.inner.is_some()
    }

    /// Return the system ID assigned by the cloud (for logging / health).
    pub fn system_id(&self) -> Option<&str> {
        self.inner.as_ref().map(|i| i.system_id.as_str())
    }

    /// Start the heartbeat background task.
    /// The `get_stats` closure is called on each heartbeat to provide current metrics.
    pub fn start_heartbeat<F>(&self, get_stats: F)
    where
        F: Fn() -> (usize, f64) + Send + Sync + 'static,
    {
        let inner = match &self.inner {
            Some(i) => i.clone(),
            None => return,
        };

        tokio::spawn(async move {
            let mut interval = tokio::time::interval(inner.config.heartbeat_interval);
            let mut shutdown_rx = inner.shutdown_tx.subscribe();
            // Skip the immediate first tick — the interval fires on first poll
            interval.tick().await;

            loop {
                tokio::select! {
                    _ = interval.tick() => {
                        let (managed_db_count, uptime) = get_stats();
                        let body = HeartbeatRequest {
                            status: "healthy".into(),
                            managed_databases: managed_db_count,
                            uptime_seconds: uptime,
                        };
                        let url = format!(
                            "{}/systems/{}/heartbeat",
                            inner.config.base_url,
                            inner.system_id,
                        );
                        let mut req = inner.client.put(&url).json(&body);
                        if let Some(ref token) = inner.config.token {
                            req = req.header("Authorization", format!("Bearer {}", token));
                        }
                        match req.send().await {
                            Ok(resp) if resp.status().is_success() => {
                                // Heartbeat acknowledged — all good
                            }
                            Ok(resp) => {
                                warn!(
                                    status = %resp.status(),
                                    "Nexus-Cloud heartbeat rejected"
                                );
                            }
                            Err(e) => {
                                warn!("Nexus-Cloud heartbeat failed: {e}");
                            }
                        }
                    }
                    _ = shutdown_rx.changed() => {
                        info!("Heartbeat loop shutting down");
                        break;
                    }
                }
            }
        });
    }

    /// Gracefully deregister from Nexus-Cloud and stop the heartbeat.
    pub async fn shutdown(&self) {
        let inner = match &self.inner {
            Some(i) => i.clone(),
            None => return,
        };

        // Signal the heartbeat loop to stop
        let _ = inner.shutdown_tx.send(true);

        // Deregister
        let url = format!("{}/systems/{}", inner.config.base_url, inner.system_id);
        let mut req = inner.client.delete(&url);
        if let Some(ref token) = inner.config.token {
            req = req.header("Authorization", format!("Bearer {}", token));
        }

        match req.send().await {
            Ok(resp) if resp.status().is_success() => {
                info!("Deregistered from Nexus-Cloud");
            }
            Ok(resp) => {
                warn!(
                    status = %resp.status(),
                    "Nexus-Cloud deregistration returned non-success"
                );
            }
            Err(e) => {
                warn!("Failed to deregister from Nexus-Cloud: {e}");
            }
        }
    }
}

