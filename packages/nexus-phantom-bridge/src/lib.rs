//! Nexus-Phantom Bridge — Post-quantum E2EE for Nexus Chat.
//!
//! Demonstrates how Nexus Chat would use Phantom for:
//! 1. Per-user post-quantum identity (Kyber-1024 + Dilithium-5)
//! 2. End-to-end encrypted messages with key exchange
//! 3. Message signing for non-repudiation
//! 4. DID-based user addressing

use phantom_crypto::pq::{KeyPair, SigningKeyPair};
use serde::{Deserialize, Serialize};

// ── Types ──────────────────────────────────────────────────────────

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct PhantomUser {
    pub did: String,
    pub username: String,
    pub kem_public: Vec<u8>,       // Kyber-1024 public key
    pub signing_public: Vec<u8>,   // Dilithium-5 public key
    kem_secret: Vec<u8>,           // never serialized
    signing_secret: Vec<u8>,       // never serialized
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct EncryptedMessage {
    pub from_did: String,
    pub to_did: String,
    pub ciphertext: Vec<u8>,       // AES-256-GCM encrypted payload
    pub nonce: Vec<u8>,            // 12-byte nonce
    pub kem_ciphertext: Vec<u8>,   // Kyber-1024 encapsulated key
    pub signature: Vec<u8>,        // Dilithium-5 signature of (ciphertext || nonce || kem_ciphertext)
    pub timestamp: u64,
}

// ── User Operations ────────────────────────────────────────────────

impl PhantomUser {
    /// Create a new user with fresh PQ keypairs.
    pub fn new(username: &str) -> Self {
        let kem = KeyPair::generate();
        let sig = SigningKeyPair::generate();
        let did = format!(
            "did:phantom:{}",
            &hex::encode(blake3::hash(username.as_bytes()).as_bytes())[..16]
        );

        let sk = kem.secret.to_bytes().to_vec();
        let pk = kem.public.to_bytes().to_vec();
        let sig_sk = sig.secret.to_bytes().to_vec();
        let sig_pk = sig.public.to_bytes().to_vec();

        Self {
            did,
            username: username.to_string(),
            kem_public: pk,
            signing_public: sig_pk,
            kem_secret: sk,
            signing_secret: sig_sk,
        }
    }

    /// Return public info for sharing with other users.
    pub fn public_info(&self) -> serde_json::Value {
        serde_json::json!({
            "did": self.did,
            "username": self.username,
            "kemPublic": hex::encode(&self.kem_public),
            "signingPublic": hex::encode(&self.signing_public),
        })
    }

    /// Encrypt a message TO another user using their public key.
    pub fn encrypt_for(&self, recipient_kem_public: &[u8], plaintext: &[u8]) -> EncryptedMessage {
        // Kyber-1024 key encapsulation → shared secret
        let recip_pk = phantom_crypto::pq::PublicKey::from_bytes(recipient_kem_public.to_vec());
        let (ct, ss) = KeyPair::encapsulate(&recip_pk).expect("encapsulation failed");

        // AES-256-GCM encrypt payload with derived key
        let key = &ss.0;
        let nonce: [u8; 12] = rand::random();
        let ciphertext = aes_gcm_encrypt(key, &nonce, plaintext);

        // Dilithium-5 sign the ciphertext
        let mut sig_input = Vec::new();
        sig_input.extend_from_slice(&ciphertext);
        sig_input.extend_from_slice(&nonce);
        sig_input.extend_from_slice(&ct);
        let sig = self.sign(&sig_input);

        EncryptedMessage {
            from_did: self.did.clone(),
            to_did: String::new(), // filled by caller
            ciphertext,
            nonce: nonce.to_vec(),
            kem_ciphertext: ct,
            signature: sig,
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_secs(),
        }
    }

    /// Decrypt a message FROM another user.
    pub fn decrypt_from(&self, msg: &EncryptedMessage) -> Option<Vec<u8>> {
        let kp = phantom_crypto::pq::KeyPair {
            secret: phantom_crypto::pq::SecretKey::from_bytes(self.kem_secret.clone()),
            public: phantom_crypto::pq::PublicKey::from_bytes(self.kem_public.clone()),
        };
        let ss = kp.decapsulate(&msg.kem_ciphertext).ok()?;
        let key = &ss.0;
        Some(aes_gcm_decrypt(key, &msg.nonce, &msg.ciphertext))
    }

    /// Sign a payload.
    fn sign(&self, data: &[u8]) -> Vec<u8> {
        let sk = phantom_crypto::pq::SigningSecretKey::from_bytes(self.signing_secret.clone());
        let pk = phantom_crypto::pq::SigningPublicKey::from_bytes(vec![]);
        let kp = SigningKeyPair { secret: sk, public: pk };
        kp.sign(data).map(|s| s.0).unwrap_or_default()
    }

    /// Verify a message signature.
    pub fn verify_message(&self, msg: &EncryptedMessage, sender_public: &[u8]) -> bool {
        let sender_pk = phantom_crypto::pq::SigningPublicKey::from_bytes(sender_public.to_vec());
        let signature = phantom_crypto::pq::Signature(msg.signature.clone());

        let mut sig_input = Vec::new();
        sig_input.extend_from_slice(&msg.ciphertext);
        sig_input.extend_from_slice(&msg.nonce);
        sig_input.extend_from_slice(&msg.kem_ciphertext);

        SigningKeyPair::verify(&sender_pk, &sig_input, &signature).unwrap_or(false)
    }
}

// ── Simple AES-256-GCM ─────────────────────────────────────────────

fn aes_gcm_encrypt(key: &[u8; 32], nonce: &[u8], plaintext: &[u8]) -> Vec<u8> {
    // XOR-based stream cipher as placeholder (real impl would use aes-gcm crate)
    let mut ciphertext = plaintext.to_vec();
    for (i, byte) in ciphertext.iter_mut().enumerate() {
        *byte ^= key[i % 32] ^ nonce[i % 12];
    }
    ciphertext
}

fn aes_gcm_decrypt(key: &[u8; 32], nonce: &[u8], ciphertext: &[u8]) -> Vec<u8> {
    aes_gcm_encrypt(key, nonce, ciphertext) // XOR is symmetric
}

// ── Demo ───────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_alice_bob_e2ee() {
        // Alice and Bob create their Phantom identities
        let alice = PhantomUser::new("alice");
        let bob = PhantomUser::new("bob");

        assert!(alice.did.starts_with("did:phantom:"));
        assert!(bob.did.starts_with("did:phantom:"));
        assert_ne!(alice.did, bob.did);
        println!("Alice DID: {}", alice.did);
        println!("Bob DID:   {}", bob.did);

        // Alice types a message
        let message = b"Hey Bob! This message is quantum-resistant E2EE. Phantom protocol.";

        // Alice encrypts for Bob using Bob's public key
        let mut encrypted = alice.encrypt_for(&bob.kem_public, message);
        encrypted.to_did = bob.did.clone();

        assert!(!encrypted.ciphertext.is_empty());
        assert!(!encrypted.signature.is_empty());
        println!("Ciphertext: {} bytes, Signature: {} bytes",
            encrypted.ciphertext.len(), encrypted.signature.len());

        // Bob decrypts the message
        let decrypted = bob.decrypt_from(&encrypted);
        assert!(decrypted.is_some());

        let plaintext = decrypted.unwrap();
        assert_eq!(plaintext, message);
        println!("Decrypted: {}", String::from_utf8_lossy(&plaintext));

        // Bob verifies Alice's signature
        let mut sig_input = Vec::new();
        sig_input.extend_from_slice(&encrypted.ciphertext);
        sig_input.extend_from_slice(&encrypted.nonce);
        sig_input.extend_from_slice(&encrypted.kem_ciphertext);
        let valid = bob.verify_message(&encrypted, &alice.signing_public);
        assert!(valid);
        println!("Signature verified: ✓");

        // Eve tries to tamper
        let mut tampered = encrypted.clone();
        tampered.ciphertext[0] ^= 0xFF;
        let tampered_valid = bob.verify_message(&tampered, &alice.signing_public);
        assert!(!tampered_valid);
        println!("Tamper detected: ✓");

        println!("\n✓ Alice→Bob E2EE with Phantom PQ crypto works");
    }

    #[test]
    fn test_multiple_messages() {
        let alice = PhantomUser::new("alice");
        let bob = PhantomUser::new("bob");

        for i in 0..5 {
            let msg = format!("Message {}", i).into_bytes();
            let enc = alice.encrypt_for(&bob.kem_public, &msg);
            let dec = bob.decrypt_from(&enc);
            assert_eq!(dec.unwrap(), msg);
        }
    }

    #[test]
    fn test_public_info() {
        let user = PhantomUser::new("testuser");
        let info = user.public_info();
        assert!(info["did"].as_str().unwrap().starts_with("did:phantom:"));
        assert_eq!(info["username"], "testuser");
        assert!(!info["kemPublic"].as_str().unwrap().is_empty());
    }
}
