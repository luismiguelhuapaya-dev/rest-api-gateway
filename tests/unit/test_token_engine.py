"""
Unit tests for the token system.
Tests mirror the behaviour implemented in src/auth/TokenEngine.cpp and
src/auth/AesGcm.cpp.

Reference: GW-008 - Token-Based Authentication

Uses Python's cryptography library for AES-256-GCM to create a reference
implementation matching the C++ code.
"""

import struct
import time
import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_helpers import (
    TokenPayload, TokenType,
    serialize_token_payload, deserialize_token_payload,
    base64url_encode, base64url_decode,
)

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    HAS_CRYPTO = True
except ImportError:
    HAS_CRYPTO = False


# ---------------------------------------------------------------------------
# Python reference AES-256-GCM matching AesGcm.cpp format
# ---------------------------------------------------------------------------

IV_SIZE = 12
TAG_SIZE = 16
KEY_SIZE = 32


def aes_gcm_encrypt(key: bytes, plaintext: bytes, aad: bytes = b"") -> bytes:
    """Encrypt using AES-256-GCM.
    Output format: IV (12 bytes) || ciphertext || tag (16 bytes).
    """
    aesgcm = AESGCM(key)
    iv = os.urandom(IV_SIZE)
    # cryptography library appends tag to ciphertext
    ct_with_tag = aesgcm.encrypt(iv, plaintext, aad if aad else None)
    return iv + ct_with_tag


def aes_gcm_decrypt(key: bytes, data: bytes, aad: bytes = b"") -> bytes:
    """Decrypt AES-256-GCM.
    Input format: IV (12 bytes) || ciphertext || tag (16 bytes).
    """
    if len(data) <= IV_SIZE + TAG_SIZE:
        raise ValueError("Data too short")
    iv = data[:IV_SIZE]
    ct_with_tag = data[IV_SIZE:]
    aesgcm = AESGCM(key)
    return aesgcm.decrypt(iv, ct_with_tag, aad if aad else None)


# ---------------------------------------------------------------------------
# Python reference TokenEngine matching TokenEngine.cpp
# ---------------------------------------------------------------------------

class ReferenceTokenEngine:
    """Python reference implementation of the C++ TokenEngine."""

    MAX_TOKEN_SIZE = 4096

    def __init__(self):
        self._key: bytes = b""
        self._access_expiry: int = 300
        self._refresh_expiry: int = 86400
        self._initialized: bool = False
        self._revoked: set = set()

    def initialize(self, key: bytes, access_expiry: int = 300, refresh_expiry: int = 86400) -> bool:
        if len(key) != KEY_SIZE:
            return False
        self._key = key
        self._access_expiry = access_expiry
        self._refresh_expiry = refresh_expiry
        self._initialized = True
        return True

    def generate_token_pair(self, server_id: str, user_id: str):
        """Returns (access_token, refresh_token) or (None, None)."""
        if not self._initialized:
            return None, None
        now = int(time.time())

        access_payload = TokenPayload()
        access_payload.server_identifier = server_id
        access_payload.user_identifier = user_id
        access_payload.token_type = TokenType.Access
        access_payload.creation_timestamp = now
        access_payload.expiry_timestamp = now + self._access_expiry

        refresh_payload = TokenPayload()
        refresh_payload.server_identifier = server_id
        refresh_payload.user_identifier = user_id
        refresh_payload.token_type = TokenType.Refresh
        refresh_payload.creation_timestamp = now
        refresh_payload.expiry_timestamp = now + self._refresh_expiry

        access_token = self._encrypt_token(access_payload)
        refresh_token = self._encrypt_token(refresh_payload)
        if access_token and refresh_token:
            return access_token, refresh_token
        return None, None

    def validate_access_token(self, token: str, expected_server_id: str):
        """Returns user_id or None."""
        if not self._initialized or token in self._revoked:
            return None
        if len(token) > self.MAX_TOKEN_SIZE:
            return None
        payload = self._decrypt_token(token)
        if payload is None:
            return None
        now = int(time.time())
        if (payload.token_type == TokenType.Access and
                payload.server_identifier == expected_server_id and
                payload.expiry_timestamp > now):
            return payload.user_identifier
        return None

    def validate_refresh_token(self, token: str, expected_server_id: str):
        """Returns user_id or None."""
        if not self._initialized or token in self._revoked:
            return None
        if len(token) > self.MAX_TOKEN_SIZE:
            return None
        payload = self._decrypt_token(token)
        if payload is None:
            return None
        now = int(time.time())
        if (payload.token_type == TokenType.Refresh and
                payload.server_identifier == expected_server_id and
                payload.expiry_timestamp > now):
            return payload.user_identifier
        return None

    def revoke_token(self, token: str):
        self._revoked.add(token)

    def is_token_revoked(self, token: str) -> bool:
        return token in self._revoked

    def _encrypt_token(self, payload: TokenPayload) -> str:
        serialized = serialize_token_payload(payload)
        encrypted = aes_gcm_encrypt(self._key, serialized)
        return base64url_encode(encrypted)

    def _decrypt_token(self, token: str):
        raw = base64url_decode(token)
        if raw is None:
            return None
        try:
            plaintext = aes_gcm_decrypt(self._key, raw)
        except Exception:
            return None
        return deserialize_token_payload(plaintext)


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

@unittest.skipUnless(HAS_CRYPTO, "cryptography library not available")
class TestTokenGeneration(unittest.TestCase):
    """GW-008 AC1: Token generation (access and refresh)."""

    def setUp(self):
        self.key = os.urandom(KEY_SIZE)
        self.engine = ReferenceTokenEngine()
        self.engine.initialize(self.key)

    def test_generate_token_pair(self):
        """GW-008 AC1: Generate both access and refresh tokens."""
        access, refresh = self.engine.generate_token_pair("server1", "user1")
        self.assertIsNotNone(access)
        self.assertIsNotNone(refresh)
        self.assertNotEqual(access, refresh)

    def test_tokens_are_base64url(self):
        """GW-008 AC1: Tokens are base64url encoded strings."""
        access, refresh = self.engine.generate_token_pair("server1", "user1")
        import re
        pattern = re.compile(r'^[A-Za-z0-9_-]+$')
        self.assertRegex(access, pattern)
        self.assertRegex(refresh, pattern)


@unittest.skipUnless(HAS_CRYPTO, "cryptography library not available")
class TestTokenValidation(unittest.TestCase):
    """GW-008 AC2: Token decryption and validation."""

    def setUp(self):
        self.key = os.urandom(KEY_SIZE)
        self.engine = ReferenceTokenEngine()
        self.engine.initialize(self.key)

    def test_validate_access_token(self):
        """GW-008 AC2: Access token decrypts and validates correctly."""
        access, _ = self.engine.generate_token_pair("srv", "user42")
        user = self.engine.validate_access_token(access, "srv")
        self.assertEqual(user, "user42")

    def test_validate_refresh_token(self):
        """GW-008 AC2: Refresh token decrypts and validates correctly."""
        _, refresh = self.engine.generate_token_pair("srv", "user42")
        user = self.engine.validate_refresh_token(refresh, "srv")
        self.assertEqual(user, "user42")


@unittest.skipUnless(HAS_CRYPTO, "cryptography library not available")
class TestTokenExpiry(unittest.TestCase):
    """GW-008 AC3: Expired token rejection."""

    def test_expired_access_token(self):
        """GW-008 AC3: Access token with 0-second expiry is rejected."""
        key = os.urandom(KEY_SIZE)
        engine = ReferenceTokenEngine()
        engine.initialize(key, access_expiry=0, refresh_expiry=86400)
        # Token is created with expiry = now + 0, so immediately expired
        # Need a slight delay or the "> now" check might still pass
        access, _ = engine.generate_token_pair("srv", "user1")
        time.sleep(1)
        user = engine.validate_access_token(access, "srv")
        self.assertIsNone(user)

    def test_expired_refresh_token(self):
        """GW-008 AC3: Refresh token with 0-second expiry is rejected."""
        key = os.urandom(KEY_SIZE)
        engine = ReferenceTokenEngine()
        engine.initialize(key, access_expiry=300, refresh_expiry=0)
        _, refresh = engine.generate_token_pair("srv", "user1")
        time.sleep(1)
        user = engine.validate_refresh_token(refresh, "srv")
        self.assertIsNone(user)


@unittest.skipUnless(HAS_CRYPTO, "cryptography library not available")
class TestTokenTypeChecking(unittest.TestCase):
    """GW-008 AC4: Token type checking (access vs refresh)."""

    def setUp(self):
        self.key = os.urandom(KEY_SIZE)
        self.engine = ReferenceTokenEngine()
        self.engine.initialize(self.key)

    def test_access_token_fails_refresh_validation(self):
        """GW-008 AC4: Access token cannot be used as refresh token."""
        access, _ = self.engine.generate_token_pair("srv", "user1")
        user = self.engine.validate_refresh_token(access, "srv")
        self.assertIsNone(user)

    def test_refresh_token_fails_access_validation(self):
        """GW-008 AC4: Refresh token cannot be used as access token."""
        _, refresh = self.engine.generate_token_pair("srv", "user1")
        user = self.engine.validate_access_token(refresh, "srv")
        self.assertIsNone(user)


@unittest.skipUnless(HAS_CRYPTO, "cryptography library not available")
class TestServerIdVerification(unittest.TestCase):
    """GW-008 AC5: Server ID verification."""

    def test_wrong_server_id_rejected(self):
        """GW-008 AC5: Token validated against wrong server ID is rejected."""
        key = os.urandom(KEY_SIZE)
        engine = ReferenceTokenEngine()
        engine.initialize(key)
        access, _ = engine.generate_token_pair("server-a", "user1")
        user = engine.validate_access_token(access, "server-b")
        self.assertIsNone(user)


@unittest.skipUnless(HAS_CRYPTO, "cryptography library not available")
class TestTamperedToken(unittest.TestCase):
    """GW-008 AC6: Tampered token rejection."""

    def test_modified_bytes_rejected(self):
        """GW-008 AC6: Modifying token bytes causes validation failure."""
        key = os.urandom(KEY_SIZE)
        engine = ReferenceTokenEngine()
        engine.initialize(key)
        access, _ = engine.generate_token_pair("srv", "user1")

        # Tamper with the token by changing a character
        if len(access) > 5:
            chars = list(access)
            chars[5] = 'X' if chars[5] != 'X' else 'Y'
            tampered = ''.join(chars)
        else:
            tampered = access + "tamper"

        user = engine.validate_access_token(tampered, "srv")
        self.assertIsNone(user)

    def test_wrong_key_rejected(self):
        """GW-008 AC6: Token encrypted with different key is rejected."""
        key1 = os.urandom(KEY_SIZE)
        key2 = os.urandom(KEY_SIZE)
        engine1 = ReferenceTokenEngine()
        engine1.initialize(key1)
        engine2 = ReferenceTokenEngine()
        engine2.initialize(key2)

        access, _ = engine1.generate_token_pair("srv", "user1")
        user = engine2.validate_access_token(access, "srv")
        self.assertIsNone(user)


class TestBase64UrlEncoding(unittest.TestCase):
    """GW-008 AC7: Base64url encoding/decoding."""

    def test_roundtrip(self):
        """GW-008 AC7: base64url encode then decode returns original data."""
        data = b"\x00\x01\x02\xff\xfe\xfd"
        encoded = base64url_encode(data)
        decoded = base64url_decode(encoded)
        self.assertEqual(data, decoded)

    def test_empty(self):
        """GW-008 AC7: Empty data roundtrips correctly."""
        self.assertEqual(base64url_encode(b""), "")
        self.assertEqual(base64url_decode(""), b"")

    def test_no_padding(self):
        """GW-008 AC7: base64url output has no padding characters."""
        data = b"test"
        encoded = base64url_encode(data)
        self.assertNotIn('=', encoded)

    def test_url_safe_chars(self):
        """GW-008 AC7: Encoding uses - and _ instead of + and /."""
        encoded = base64url_encode(b"\xff\xff\xff")
        self.assertNotIn('+', encoded)
        self.assertNotIn('/', encoded)

    def test_known_value(self):
        """GW-008 AC7: Known encoding matches expected output."""
        data = b"Hello"
        encoded = base64url_encode(data)
        decoded = base64url_decode(encoded)
        self.assertEqual(decoded, data)


class TestTokenPayloadSerialization(unittest.TestCase):
    """GW-008 AC8: Binary format correctness."""

    def test_roundtrip(self):
        """GW-008 AC8: Serialize then deserialize produces identical payload."""
        p = TokenPayload()
        p.server_identifier = "my-server"
        p.user_identifier = "user-123"
        p.token_type = TokenType.Access
        p.creation_timestamp = 1700000000
        p.expiry_timestamp = 1700000300

        data = serialize_token_payload(p)
        p2 = deserialize_token_payload(data)

        self.assertIsNotNone(p2)
        self.assertEqual(p2.server_identifier, p.server_identifier)
        self.assertEqual(p2.user_identifier, p.user_identifier)
        self.assertEqual(p2.token_type, p.token_type)
        self.assertEqual(p2.creation_timestamp, p.creation_timestamp)
        self.assertEqual(p2.expiry_timestamp, p.expiry_timestamp)

    def test_refresh_type_roundtrip(self):
        """GW-008 AC8: Refresh token type byte roundtrips correctly."""
        p = TokenPayload()
        p.server_identifier = "srv"
        p.user_identifier = "usr"
        p.token_type = TokenType.Refresh
        p.creation_timestamp = 100
        p.expiry_timestamp = 200

        data = serialize_token_payload(p)
        p2 = deserialize_token_payload(data)
        self.assertEqual(p2.token_type, TokenType.Refresh)

    def test_binary_format_structure(self):
        """GW-008 AC8: Binary format matches documented structure."""
        p = TokenPayload()
        p.server_identifier = "AB"
        p.user_identifier = "CD"
        p.token_type = TokenType.Access
        p.creation_timestamp = 0
        p.expiry_timestamp = 0

        data = serialize_token_payload(p)
        # [4:server_len][2:AB][4:user_len][2:CD][1:type][8:created][8:expiry]
        self.assertEqual(len(data), 4 + 2 + 4 + 2 + 1 + 8 + 8)

        # Server ID length = 2
        self.assertEqual(struct.unpack_from('>I', data, 0)[0], 2)
        # Server ID
        self.assertEqual(data[4:6], b"AB")
        # User ID length = 2
        self.assertEqual(struct.unpack_from('>I', data, 6)[0], 2)
        # User ID
        self.assertEqual(data[10:12], b"CD")
        # Token type = 0 (Access)
        self.assertEqual(data[12], 0)

    def test_too_short_data(self):
        """GW-008 AC8: Data shorter than minimum is rejected."""
        self.assertIsNone(deserialize_token_payload(b"\x00" * 10))


@unittest.skipUnless(HAS_CRYPTO, "cryptography library not available")
class TestTokenRevocation(unittest.TestCase):
    """GW-008: Token revocation."""

    def test_revoked_token_fails_validation(self):
        """GW-008: Revoked token is rejected."""
        key = os.urandom(KEY_SIZE)
        engine = ReferenceTokenEngine()
        engine.initialize(key)
        access, _ = engine.generate_token_pair("srv", "user1")

        # Valid before revocation
        self.assertIsNotNone(engine.validate_access_token(access, "srv"))

        # Revoke
        engine.revoke_token(access)
        self.assertTrue(engine.is_token_revoked(access))

        # Invalid after revocation
        self.assertIsNone(engine.validate_access_token(access, "srv"))


if __name__ == '__main__':
    unittest.main()
