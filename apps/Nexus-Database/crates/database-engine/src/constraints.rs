//! Column Constraints — NOT NULL, DEFAULT, UNIQUE validation.
//!
//! Enforces data integrity rules on INSERT and UPDATE.
//! Stored separately from the table catalog for modularity.
//!
//! SQL:
//!   CREATE TABLE users (id INTEGER NOT NULL, name TEXT DEFAULT 'anon');
//!   INSERT INTO users (id) VALUES (1);  -- name gets DEFAULT 'anon'
//!   INSERT INTO users (name) VALUES ('alice');  -- ERROR: id is NOT NULL

use std::collections::HashMap;
use parking_lot::RwLock;

#[derive(Debug)]
pub struct ConstraintStore {
    /// table_name → column_name → list of constraints
    constraints: RwLock<HashMap<String, HashMap<String, Vec<ColumnConstraint>>>>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ColumnConstraint {
    NotNull,
    DefaultValue(String),
    Unique,
    Check(String),
}

impl ConstraintStore {
    pub fn new() -> Self {
        Self { constraints: RwLock::new(HashMap::new()) }
    }

    /// Register constraints for a table's columns.
    pub fn register(&self, table: &str, columns: &[(String, Vec<ColumnConstraint>)]) {
        let mut map = HashMap::new();
        for (col_name, constraints) in columns {
            if !constraints.is_empty() {
                map.insert(col_name.clone(), constraints.clone());
            }
        }
        if !map.is_empty() {
            self.constraints.write().insert(table.to_string(), map);
        }
    }

    /// Validate a row before INSERT. Returns error message if constraint violated.
    pub fn validate_insert(&self, table: &str, column_names: &[String], values: &[Option<Vec<u8>>]) -> Option<String> {
        let constraints = self.constraints.read();
        let table_constraints = constraints.get(table)?;

        for (i, col_name) in column_names.iter().enumerate() {
            if let Some(col_constraints) = table_constraints.get(col_name) {
                for c in col_constraints {
                    match c {
                        ColumnConstraint::NotNull => {
                            if i >= values.len() || values[i].is_none() {
                                return Some(format!("null value in column \"{}\" violates not-null constraint", col_name));
                            }
                        }
                        ColumnConstraint::DefaultValue(_default) => {
                            // Default is applied, not validated
                        }
                        ColumnConstraint::Unique => {
                            // Validated by auto-created unique index during CREATE TABLE
                        }
                        ColumnConstraint::Check(cond) => {
                            let val_str = values.get(i).and_then(|v| v.as_ref())
                                .map(|b| String::from_utf8_lossy(b).to_string())
                                .unwrap_or_else(|| "NULL".to_string());
                            if !eval_check(cond, &val_str) {
                                return Some(format!("new row violates check constraint \"{}\"", cond));
                            }
                        }
                    }
                }
            }
        }
        None
    }

    /// Apply DEFAULT values to a row before INSERT.
    /// Returns modified values with defaults filled in.
    pub fn apply_defaults(&self, table: &str, column_names: &[String], values: Vec<Option<Vec<u8>>>) -> Vec<Option<Vec<u8>>> {
        let constraints = self.constraints.read();
        let table_constraints = match constraints.get(table) {
            Some(c) => c,
            None => return values,
        };

        let mut result = values;
        // Pad with Nones to match column count
        while result.len() < column_names.len() {
            result.push(None);
        }

        for (i, col_name) in column_names.iter().enumerate() {
            if let Some(col_constraints) = table_constraints.get(col_name) {
                for c in col_constraints {
                    if let ColumnConstraint::DefaultValue(default) = c {
                        if i < result.len() && result[i].is_none() {
                            result[i] = Some(default.as_bytes().to_vec());
                        }
                    }
                }
            }
        }
        result
    }

    /// Parse constraint from SQL column definition.
    /// e.g., "NOT NULL" → NotNull, "DEFAULT 'hello'" → DefaultValue("hello")
    pub fn parse_constraints(def: &str) -> Vec<ColumnConstraint> {
        let mut constraints = Vec::new();
        let upper = def.to_uppercase();

        if upper.contains("UNIQUE") {
            constraints.push(ColumnConstraint::Unique);
        }
        if upper.contains("NOT NULL") {
            constraints.push(ColumnConstraint::NotNull);
        }

        if let Some(pos) = upper.find("DEFAULT") {
            let rest = &def[pos + 7..].trim();
            let val = rest.trim_matches('\'').trim_matches('"').to_string();
            constraints.push(ColumnConstraint::DefaultValue(val));
        }

        if let Some(pos) = upper.find("CHECK") {
            if let Some(paren_open) = def[pos..].find('(') {
                let paren_pos = pos + paren_open;
                if let Some(paren_close) = def[paren_pos..].find(')') {
                    let cond = def[paren_pos + 1..paren_pos + paren_close].trim().to_string();
                    constraints.push(ColumnConstraint::Check(cond));
                }
            }
        }

        constraints
    }

    /// Get constraints for a table column.
    pub fn get(&self, table: &str, column: &str) -> Vec<ColumnConstraint> {
        self.constraints.read()
            .get(table)
            .and_then(|cols| cols.get(column))
            .cloned()
            .unwrap_or_default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_not_null_violation() {
        let store = ConstraintStore::new();
        store.register("users", &[("id".into(), vec![ColumnConstraint::NotNull])]);

        let err = store.validate_insert("users", &["id".into()], &[None]);
        assert!(err.is_some());
        assert!(err.unwrap().contains("not-null"));
    }

    #[test]
    fn test_not_null_passes() {
        let store = ConstraintStore::new();
        store.register("users", &[("id".into(), vec![ColumnConstraint::NotNull])]);

        let err = store.validate_insert("users", &["id".into()], &[Some(b"1".to_vec())]);
        assert!(err.is_none());
    }

    #[test]
    fn test_default_value() {
        let store = ConstraintStore::new();
        store.register("users", &[("name".into(), vec![ColumnConstraint::DefaultValue("anon".into())])]);

        let values = store.apply_defaults("users", &["name".into()], vec![None]);
        assert_eq!(values[0], Some(b"anon".to_vec()));
    }

    #[test]
    fn test_default_does_not_override() {
        let store = ConstraintStore::new();
        store.register("users", &[("name".into(), vec![ColumnConstraint::DefaultValue("anon".into())])]);

        let values = store.apply_defaults("users", &["name".into()], vec![Some(b"alice".to_vec())]);
        assert_eq!(values[0], Some(b"alice".to_vec())); // not overridden
    }
}

/// Evaluate a simple CHECK condition against a string value.
/// Supports: "val > 0", "val != ''", "val IS NOT NULL", "val IN ('a','b')"
fn eval_check(cond: &str, actual: &str) -> bool {
    let parts: Vec<_> = cond.split_whitespace().collect();
    if parts.len() < 2 { return true; }

    let col_ref = parts[0];
    let op = parts[1];

    // IS NULL / IS NOT NULL
    if parts.len() >= 4 && parts[1] == "IS" && parts[2] == "NOT" && parts[3] == "NULL" {
        return actual != "NULL" && !actual.is_empty();
    }
    if parts.len() >= 3 && parts[1] == "IS" && parts[2] == "NULL" {
        return actual == "NULL" || actual.is_empty();
    }

    let right = parts.get(2).map(|s| s.trim_matches('\'').trim_matches('"').to_string()).unwrap_or_default();

    // Numeric comparison
    if let (Ok(av), Ok(bv)) = (actual.parse::<f64>(), right.parse::<f64>()) {
        return match op {
            "=" => av == bv, "!=" | "<>" => av != bv,
            ">" => av > bv, "<" => av < bv,
            ">=" => av >= bv, "<=" => av <= bv,
            _ => true,
        };
    }

    // String comparison
    match op {
        "=" => actual == right,
        "!=" | "<>" => actual != right,
        _ => {
            let av_cmp = actual.to_string();
            let bv_cmp = right.to_string();
            match op {
                ">" => av_cmp > bv_cmp,
                "<" => av_cmp < bv_cmp,
                _ => true,
            }
        },
        "IN" => {
            let in_vals: Vec<_> = right.split(',').map(|s| s.trim().trim_matches('\'').trim_matches('"').to_string()).collect();
            in_vals.iter().any(|v| v == actual)
        }
        _ => true,
    }
}
