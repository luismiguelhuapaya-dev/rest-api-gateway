"""
Shared test fixtures and helpers for the Dynamic REST API Gateway Server unit tests.

Provides Python reference implementations of data structures and utilities that
mirror the C++ gateway server code, enabling validation of protocol logic
without requiring the compiled C++ binary.
"""

import struct
import json
import re
import os
import time
from enum import IntEnum
from typing import Optional, Dict, List, Tuple, Any


# ---------------------------------------------------------------------------
# Enums mirroring gateway/Common.h
# ---------------------------------------------------------------------------

class HttpMethod(IntEnum):
    Get = 0
    Post = 1
    Put = 2
    Delete = 3
    Patch = 4
    Head = 5
    Options = 6
    Unknown = 7


class TokenType(IntEnum):
    Access = 0
    Refresh = 1


class ParameterType(IntEnum):
    String = 0
    Integer = 1
    Float = 2
    Boolean = 3
    Object = 4
    Array = 5


class ParameterLocation(IntEnum):
    Path = 0
    Query = 1
    Header = 2
    Body = 3


class FrameType(IntEnum):
    Registration = 0
    Unregistration = 1
    Request = 2
    Response = 3
    LoginResponse = 4
    TokenResponse = 5
    Heartbeat = 6
    Error = 7


class LogLevel(IntEnum):
    Debug = 0
    Info = 1
    Warning = 2
    Error = 3
    Fatal = 4


# ---------------------------------------------------------------------------
# Method string conversion (mirrors main.cpp)
# ---------------------------------------------------------------------------

_METHOD_TO_STRING = {
    HttpMethod.Get: "GET",
    HttpMethod.Post: "POST",
    HttpMethod.Put: "PUT",
    HttpMethod.Delete: "DELETE",
    HttpMethod.Patch: "PATCH",
    HttpMethod.Head: "HEAD",
    HttpMethod.Options: "OPTIONS",
    HttpMethod.Unknown: "UNKNOWN",
}

_STRING_TO_METHOD = {}
for _m, _s in _METHOD_TO_STRING.items():
    _STRING_TO_METHOD[_s] = _m
    _STRING_TO_METHOD[_s.lower()] = _m


def http_method_to_string(method: HttpMethod) -> str:
    return _METHOD_TO_STRING.get(method, "UNKNOWN")


def string_to_http_method(s: str) -> HttpMethod:
    return _STRING_TO_METHOD.get(s, HttpMethod.Unknown)


# ---------------------------------------------------------------------------
# HTTP request structure
# ---------------------------------------------------------------------------

class HttpRequest:
    def __init__(self):
        self.method: HttpMethod = HttpMethod.Unknown
        self.path: str = ""
        self.query_string: str = ""
        self.http_version: str = ""
        self.headers: Dict[str, str] = {}
        self.body: str = ""
        self.query_parameters: Dict[str, str] = {}
        self.path_parameters: Dict[str, str] = {}
        self.raw_request: str = ""
        self.is_complete: bool = False
        self.content_length: int = 0


# ---------------------------------------------------------------------------
# Parameter constraints and schema
# ---------------------------------------------------------------------------

class ParameterConstraints:
    def __init__(self):
        self.min_value: Optional[int] = None
        self.max_value: Optional[int] = None
        self.min_float_value: Optional[float] = None
        self.max_float_value: Optional[float] = None
        self.min_length: Optional[int] = None
        self.max_length: Optional[int] = None
        self.pattern: Optional[str] = None
        self.allowed_values: List[str] = []
        self.min_array_items: Optional[int] = None
        self.max_array_items: Optional[int] = None


class ParameterSchema:
    def __init__(self):
        self.name: str = ""
        self.parameter_type: ParameterType = ParameterType.String
        self.location: ParameterLocation = ParameterLocation.Query
        self.is_required: bool = False
        self.description: str = ""
        self.default_value: str = ""
        self.constraints: ParameterConstraints = ParameterConstraints()

    def is_valid(self) -> bool:
        return len(self.name) > 0


# ---------------------------------------------------------------------------
# Endpoint definition
# ---------------------------------------------------------------------------

class EndpointDefinition:
    def __init__(self):
        self.path: str = ""
        self.method: HttpMethod = HttpMethod.Unknown
        self.backend_identifier: str = ""
        self.description: str = ""
        self.requires_authentication: bool = False
        self.parameter_schemas: List[ParameterSchema] = []
        self.path_segments: List[str] = []
        self.is_parameter_segment: List[bool] = []
        self.parameter_names: List[str] = []

    def is_valid(self) -> bool:
        return (len(self.path) > 0 and
                self.method != HttpMethod.Unknown and
                len(self.backend_identifier) > 0)

    def get_route_key(self) -> str:
        return http_method_to_string(self.method) + ":" + self.path

    def parse_path_segments(self):
        """Parse path into segments, detecting {param} placeholders."""
        self.path_segments = []
        self.is_parameter_segment = []
        self.parameter_names = []
        for seg in self.path.split('/'):
            if seg:
                self.path_segments.append(seg)
                if seg.startswith('{') and seg.endswith('}'):
                    self.is_parameter_segment.append(True)
                    self.parameter_names.append(seg[1:-1])
                else:
                    self.is_parameter_segment.append(False)
                    self.parameter_names.append("")

    def matches_path(self, request_path: str) -> Tuple[bool, Dict[str, str]]:
        """Check if request_path matches this endpoint pattern.
        Returns (matched, path_parameters).
        """
        req_segments = [s for s in request_path.split('/') if s]
        if len(req_segments) != len(self.path_segments):
            return False, {}

        params: Dict[str, str] = {}
        for i, seg in enumerate(self.path_segments):
            if self.is_parameter_segment[i]:
                params[self.parameter_names[i]] = req_segments[i]
            else:
                if seg != req_segments[i]:
                    return False, {}
        return True, params


# ---------------------------------------------------------------------------
# Validation structures
# ---------------------------------------------------------------------------

class ValidationError:
    def __init__(self, parameter_name="", error_message="", provided_value=""):
        self.parameter_name = parameter_name
        self.error_message = error_message
        self.provided_value = provided_value


class ValidationResult:
    def __init__(self):
        self.is_valid: bool = True
        self.errors: List[ValidationError] = []
        self.formatted_error_message: str = ""


# ---------------------------------------------------------------------------
# Token payload
# ---------------------------------------------------------------------------

class TokenPayload:
    def __init__(self):
        self.server_identifier: str = ""
        self.user_identifier: str = ""
        self.token_type: TokenType = TokenType.Access
        self.creation_timestamp: int = 0
        self.expiry_timestamp: int = 0


# ---------------------------------------------------------------------------
# Frame structure
# ---------------------------------------------------------------------------

class Frame:
    HEADER_SIZE = 12
    MAX_PAYLOAD_SIZE = 1048576

    def __init__(self):
        self.frame_type: FrameType = FrameType.Error
        self.request_identifier: int = 0
        self.payload_length: int = 0
        self.payload: str = ""


# ---------------------------------------------------------------------------
# Utility: base64url encode/decode (mirrors AesGcm.cpp)
# ---------------------------------------------------------------------------

_BASE64URL_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"


def base64url_encode(data: bytes) -> str:
    result = []
    for i in range(0, len(data), 3):
        triple = data[i] << 16
        if i + 1 < len(data):
            triple |= data[i + 1] << 8
        if i + 2 < len(data):
            triple |= data[i + 2]

        result.append(_BASE64URL_CHARS[(triple >> 18) & 0x3F])
        result.append(_BASE64URL_CHARS[(triple >> 12) & 0x3F])
        if i + 1 < len(data):
            result.append(_BASE64URL_CHARS[(triple >> 6) & 0x3F])
        if i + 2 < len(data):
            result.append(_BASE64URL_CHARS[triple & 0x3F])
    return "".join(result)


def base64url_decode(encoded: str) -> Optional[bytes]:
    lookup = {}
    for idx, ch in enumerate(_BASE64URL_CHARS):
        lookup[ch] = idx
    lookup['+'] = 62
    lookup['/'] = 63

    result = bytearray()
    accumulator = 0
    bits = 0
    for ch in encoded:
        if ch == '=':
            break
        val = lookup.get(ch)
        if val is None:
            return None
        accumulator = (accumulator << 6) | val
        bits += 6
        if bits >= 8:
            bits -= 8
            result.append((accumulator >> bits) & 0xFF)
    return bytes(result)


# ---------------------------------------------------------------------------
# Token payload serialization (mirrors TokenEngine.cpp)
# ---------------------------------------------------------------------------

def serialize_token_payload(payload: TokenPayload) -> bytes:
    """Serialize token payload to binary format matching C++ TokenEngine."""
    data = bytearray()

    # Server ID length (big-endian uint32)
    server_bytes = payload.server_identifier.encode('utf-8')
    data += struct.pack('>I', len(server_bytes))
    data += server_bytes

    # User ID length (big-endian uint32)
    user_bytes = payload.user_identifier.encode('utf-8')
    data += struct.pack('>I', len(user_bytes))
    data += user_bytes

    # Token type (1 byte: 0=access, 1=refresh)
    data += struct.pack('B', 0 if payload.token_type == TokenType.Access else 1)

    # Creation timestamp (big-endian int64)
    data += struct.pack('>q', payload.creation_timestamp)

    # Expiry timestamp (big-endian int64)
    data += struct.pack('>q', payload.expiry_timestamp)

    return bytes(data)


def deserialize_token_payload(data: bytes) -> Optional[TokenPayload]:
    """Deserialize token payload from binary format matching C++ TokenEngine."""
    if len(data) < 25:  # Minimum: 4 + 0 + 4 + 0 + 1 + 8 + 8
        return None

    offset = 0
    payload = TokenPayload()

    # Server ID length
    server_id_len = struct.unpack_from('>I', data, offset)[0]
    offset += 4

    if offset + server_id_len + 4 + 1 + 16 > len(data):
        return None

    # Server ID
    payload.server_identifier = data[offset:offset + server_id_len].decode('utf-8')
    offset += server_id_len

    # User ID length
    user_id_len = struct.unpack_from('>I', data, offset)[0]
    offset += 4

    if offset + user_id_len + 1 + 16 > len(data):
        return None

    # User ID
    payload.user_identifier = data[offset:offset + user_id_len].decode('utf-8')
    offset += user_id_len

    # Token type
    payload.token_type = TokenType.Access if data[offset] == 0 else TokenType.Refresh
    offset += 1

    if offset + 16 > len(data):
        return None

    # Creation timestamp
    payload.creation_timestamp = struct.unpack_from('>q', data, offset)[0]
    offset += 8

    # Expiry timestamp
    payload.expiry_timestamp = struct.unpack_from('>q', data, offset)[0]

    return payload


# ---------------------------------------------------------------------------
# Hex string parsing (mirrors Configuration.cpp)
# ---------------------------------------------------------------------------

def parse_hex_string(hex_str: str) -> Optional[bytes]:
    """Parse a 64-char hex string into 32 bytes, matching C++ ParseHexString."""
    if len(hex_str) != 64:
        return None
    try:
        return bytes.fromhex(hex_str)
    except ValueError:
        return None


# ---------------------------------------------------------------------------
# URL decoding (mirrors HttpParser.cpp)
# ---------------------------------------------------------------------------

def url_decode(encoded: str) -> str:
    """URL-decode a string, matching C++ HttpParser::UrlDecode."""
    result = []
    i = 0
    while i < len(encoded):
        if encoded[i] == '%' and i + 2 < len(encoded):
            hex_chars = encoded[i + 1:i + 3]
            try:
                result.append(chr(int(hex_chars, 16)))
                i += 3
                continue
            except ValueError:
                pass
        if encoded[i] == '+':
            result.append(' ')
        else:
            result.append(encoded[i])
        i += 1
    return "".join(result)


# ---------------------------------------------------------------------------
# Build a raw HTTP request string for testing
# ---------------------------------------------------------------------------

def build_raw_request(method: str, path: str, version: str = "HTTP/1.1",
                      headers: Optional[Dict[str, str]] = None,
                      body: str = "") -> str:
    """Construct a raw HTTP request string with CRLF line endings."""
    request_line = f"{method} {path} {version}\r\n"
    header_lines = ""
    if headers:
        for k, v in headers.items():
            header_lines += f"{k}: {v}\r\n"
    return request_line + header_lines + "\r\n" + body
