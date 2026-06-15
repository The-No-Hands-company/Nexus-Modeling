//! Secondary Indexes — B-Tree indexes on non-primary columns.
//!
//! Each index maps column values → primary key values for O(log n) lookups.
//! Automatically maintained on INSERT/UPDATE/DELETE.
//! Query planner uses indexes when WHERE clause matches an indexed column.
//!
//! Index types:
//!   - BTREE (default): O(log n) equality + range queries
//!   - HASH: O(1) equality only (future)
//!   - GIN: full-text search (future)

use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::RwLock;

use crate::btree::BTree;
use crate::buffer::BufferPool;
use crate::catalog::{ColumnType, TableMeta};
use crate::page::PAGE_SIZE;
use crate::row::Row;
use crate::{EngineError, Result};

// ── Index Metadata ─────────────────────────────────────────────

#[derive(Debug, Clone)]
pub struct IndexMeta {
    pub name: String,
    pub table: String,
    pub columns: Vec<String>,
    pub unique: bool,
    pub index_type: IndexType,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IndexType {
    BTree,
}

impl IndexType {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_uppercase().as_str() {
            "BTREE" | "B-TREE" | "" => Some(Self::BTree),
            _ => None,
        }
    }
}

// ── Index Manager ──────────────────────────────────────────────

/// Manages all secondary indexes across all tables.
pub struct IndexManager {
    /// index_name → IndexMeta
    indexes: RwLock<HashMap<String, IndexMeta>>,
    /// table_name → list of index names
    table_indexes: RwLock<HashMap<String, Vec<String>>>,
    /// index_name → B-Tree (maps index key → primary key)
    trees: RwLock<HashMap<String, Arc<BTree>>>,
    pool: Arc<BufferPool>,
}

impl IndexManager {
    pub fn new(pool: Arc<BufferPool>) -> Self {
        Self {
            indexes: RwLock::new(HashMap::new()),
            table_indexes: RwLock::new(HashMap::new()),
            trees: RwLock::new(HashMap::new()),
            pool,
        }
    }

    /// Create a new index on a table.
    pub fn create_index(
        &self,
        name: &str,
        table: &str,
        columns: Vec<String>,
        unique: bool,
        index_type: IndexType,
        _table_meta: &TableMeta,
    ) -> Result<()> {
        let mut indexes = self.indexes.write();
        if indexes.contains_key(name) {
            return Err(EngineError::KeyNotFound(format!("Index {} already exists", name)));
        }

        let meta = IndexMeta {
            name: name.to_string(),
            table: table.to_string(),
            columns: columns.clone(),
            unique,
            index_type,
        };

        indexes.insert(name.to_string(), meta);
        self.table_indexes.write()
            .entry(table.to_string())
            .or_default()
            .push(name.to_string());

        // Create backing B-Tree
        let tree = Arc::new(BTree::new(self.pool.clone()));
        self.trees.write().insert(name.to_string(), tree);

        Ok(())
    }

    /// Drop an index.
    pub fn drop_index(&self, name: &str) -> Result<()> {
        let meta = self.indexes.write().remove(name)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Index {} not found", name)))?;

        if let Some(list) = self.table_indexes.write().get_mut(&meta.table) {
            list.retain(|n| n != name);
        }
        self.trees.write().remove(name);
        Ok(())
    }

    /// Insert entries into all indexes for a table when a row is inserted.
    pub fn index_row(&self, table: &str, pk: &[u8], row: &Row, meta: &TableMeta) -> Result<()> {
        let indexes = self.table_indexes.read();
        if let Some(idx_names) = indexes.get(table) {
            for idx_name in idx_names {
                let idx_meta = self.indexes.read();
                if let Some(im) = idx_meta.get(idx_name) {
                    let key = self.build_index_key(&im.columns, row, meta);
                    if let Some(ref key) = key {
                        if let Some(tree) = self.trees.read().get(idx_name) {
                            tree.insert(key, pk)?;
                        }
                    }
                }
            }
        }
        Ok(())
    }

    /// Delete entries from all indexes for a table when a row is deleted.
    pub fn deindex_row(&self, table: &str, row: &Row, table_meta: &TableMeta) -> Result<()> {
        let indexes = self.table_indexes.read();
        if let Some(idx_names) = indexes.get(table) {
            for idx_name in idx_names {
                let idx_meta = self.indexes.read();
                if let Some(im) = idx_meta.get(idx_name) {
                    let key = self.build_index_key(&im.columns, row, table_meta);
                    if let Some(ref key) = key {
                        if let Some(tree) = self.trees.read().get(idx_name) {
                            let _ = tree.delete(key);
                        }
                    }
                }
            }
        }
        Ok(())
    }

    /// Look up primary keys via an index. Returns matching PKs.
    pub fn lookup(&self, index_name: &str, value: &[u8]) -> Result<Vec<Vec<u8>>> {
        let trees = self.trees.read();
        let tree = trees.get(index_name)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Index {} not found", index_name)))?;

        // For equality lookup, search the B-Tree
        if let Some(pk) = tree.search(value)? {
            Ok(vec![pk])
        } else {
            Ok(vec![])
        }
    }

    /// Check if a WHERE clause column has an index (for query planning).
    pub fn find_best_index(&self, table: &str, column: &str) -> Option<IndexMeta> {
        let table_idx = self.table_indexes.read();
        let idx_list = table_idx.get(table)?;
        let indexes = self.indexes.read();

        // Prefer single-column indexes over multi-column
        idx_list.iter()
            .filter_map(|name| indexes.get(name))
            .filter(|m| m.columns.len() == 1 && m.columns[0] == column)
            .next()
            .cloned()
            .or_else(|| {
                // Fallback: multi-column index starting with this column
                idx_list.iter()
                    .filter_map(|name| indexes.get(name))
                    .filter(|m| m.columns.first().map(|c| c == column).unwrap_or(false))
                    .next()
                    .cloned()
            })
    }

    /// List all indexes for a table.
    pub fn list_indexes(&self, table: &str) -> Vec<IndexMeta> {
        let table_idx = self.table_indexes.read();
        let indexes = self.indexes.read();
        table_idx.get(table)
            .map(|list| list.iter().filter_map(|n| indexes.get(n).cloned()).collect())
            .unwrap_or_default()
    }

    /// List all indexes across all tables.
    pub fn list_all(&self) -> Vec<IndexMeta> {
        self.indexes.read().values().cloned().collect()
    }

    /// Build the index key from a row's column values.
    fn build_index_key(&self, columns: &[String], row: &Row, meta: &TableMeta) -> Option<Vec<u8>> {
        let mut key = Vec::new();
        for col_name in columns {
            let idx = meta.column_names.iter().position(|c| c == col_name)?;
            let val = row.columns.get(idx)?.as_ref()?;
            key.extend_from_slice(val);
            key.push(0); // separator
        }
        if key.is_empty() { None } else { Some(key) }
    }
}

// ── Index-aware query planning ─────────────────────────────────

/// Extends the SQL executor with index-aware query execution.
/// When a SELECT has WHERE col = val AND col has an index, use the index.
pub fn index_lookup(
    index_mgr: &IndexManager,
    table: &str,
    column: &str,
    value: &[u8],
) -> Result<Option<Vec<Vec<u8>>>> {
    if let Some(idx) = index_mgr.find_best_index(table, column) {
        let pks = index_mgr.lookup(&idx.name, value)?;
        if !pks.is_empty() {
            return Ok(Some(pks));
        }
    }
    // No index — fall back to full table scan
    Ok(None)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::catalog::TableCatalog;
    use crate::catalog::StorageEngine;

    #[test]
    fn test_create_index() {
        let pool = Arc::new(BufferPool::new(100, PAGE_SIZE));
        let mgr = IndexManager::new(pool.clone());

        let catalog = TableCatalog::new();
        catalog.create_table("users", StorageEngine::BTree, vec![
            ("id".into(), ColumnType::Integer),
            ("name".into(), ColumnType::Text),
        ]).unwrap();
        let meta = catalog.get_table("users").unwrap();

        mgr.create_index("idx_users_name", "users", vec!["name".into()], false, IndexType::BTree, &meta).unwrap();
        let list = mgr.list_indexes("users");
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].name, "idx_users_name");
    }

    #[test]
    fn test_index_lookup() {
        let pool = Arc::new(BufferPool::new(200, PAGE_SIZE));
        let mgr = IndexManager::new(pool.clone());

        let catalog = TableCatalog::new();
        catalog.create_table("users", StorageEngine::BTree, vec![
            ("id".into(), ColumnType::Integer),
            ("name".into(), ColumnType::Text),
        ]).unwrap();
        let meta = catalog.get_table("users").unwrap();

        mgr.create_index("idx_name", "users", vec!["name".into()], false, IndexType::BTree, &meta).unwrap();

        // Insert a row into the index
        let row = Row::new(vec![Some(b"1".to_vec()), Some(b"alice".to_vec())]);
        mgr.index_row("users", b"pk1", &row, &meta).unwrap();

        // Look up by name
        let pks = mgr.lookup("idx_name", b"alice").unwrap();
        assert!(!pks.is_empty());
    }

    #[test]
    fn test_query_planner_finds_index() {
        let pool = Arc::new(BufferPool::new(100, PAGE_SIZE));
        let mgr = IndexManager::new(pool);
        let catalog = TableCatalog::new();
        catalog.create_table("users", StorageEngine::BTree, vec![
            ("name".into(), ColumnType::Text),
        ]).unwrap();
        let meta = catalog.get_table("users").unwrap();
        mgr.create_index("idx_n", "users", vec!["name".into()], false, IndexType::BTree, &meta).unwrap();

        let found = mgr.find_best_index("users", "name");
        assert!(found.is_some());
        assert_eq!(found.unwrap().name, "idx_n");

        let not_found = mgr.find_best_index("users", "email");
        assert!(not_found.is_none());
    }

    #[test]
    fn test_drop_index() {
        let pool = Arc::new(BufferPool::new(100, PAGE_SIZE));
        let mgr = IndexManager::new(pool);
        let catalog = TableCatalog::new();
        catalog.create_table("t", StorageEngine::BTree, vec![("x".into(), ColumnType::Integer)]).unwrap();
        let meta = catalog.get_table("t").unwrap();
        mgr.create_index("idx_x", "t", vec!["x".into()], false, IndexType::BTree, &meta).unwrap();
        assert_eq!(mgr.list_indexes("t").len(), 1);

        mgr.drop_index("idx_x").unwrap();
        assert_eq!(mgr.list_indexes("t").len(), 0);
    }
}
