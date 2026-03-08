"""
Unit tests for the validation engine.
Tests mirror the behaviour implemented in src/validation/ValidationEngine.cpp.

Reference: GW-006 - Request Validation Engine
"""

import json
import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_helpers import (
    HttpRequest, HttpMethod, EndpointDefinition, ParameterSchema,
    ParameterConstraints, ParameterType, ParameterLocation,
    ValidationResult, ValidationError
)


# ---------------------------------------------------------------------------
# Minimal Python JSON value used by the validation engine
# ---------------------------------------------------------------------------

class JsonValue:
    """Lightweight stand-in for the C++ JsonValue class."""

    def __init__(self, raw=None):
        self._raw = raw

    @staticmethod
    def parse(text: str):
        try:
            return JsonValue(json.loads(text))
        except json.JSONDecodeError:
            return None

    def is_object(self):
        return isinstance(self._raw, dict)

    def is_array(self):
        return isinstance(self._raw, list)

    def has_member(self, key):
        return isinstance(self._raw, dict) and key in self._raw

    def get_member(self, key):
        return JsonValue(self._raw[key])

    def is_string(self):
        return isinstance(self._raw, str)

    def is_integer(self):
        return isinstance(self._raw, int) and not isinstance(self._raw, bool)

    def is_float(self):
        return isinstance(self._raw, float)

    def is_boolean(self):
        return isinstance(self._raw, bool)

    def get_string(self):
        return self._raw

    def get_integer(self):
        return self._raw

    def get_float(self):
        return self._raw

    def get_boolean(self):
        return self._raw


# ---------------------------------------------------------------------------
# Python reference validation engine mirroring ValidationEngine.cpp
# ---------------------------------------------------------------------------

def _validate_param_value(value, schema):
    """Validate a single parameter value against its schema."""
    from test_parameter_schema import validate_parameter_value
    return validate_parameter_value(value, schema)


def _validate_json_body_field(json_body: JsonValue, schema: ParameterSchema, errors: list) -> bool:
    """Mirror ValidationEngine::ValidateJsonBodyField."""
    if json_body.is_object():
        if json_body.has_member(schema.name):
            field = json_body.get_member(schema.name)
            type_valid = True
            field_str = ""

            if schema.parameter_type == ParameterType.String:
                if field.is_string():
                    field_str = field.get_string()
                else:
                    type_valid = False
            elif schema.parameter_type == ParameterType.Integer:
                if field.is_integer():
                    field_str = str(field.get_integer())
                else:
                    type_valid = False
            elif schema.parameter_type == ParameterType.Float:
                if field.is_float() or field.is_integer():
                    if field.is_float():
                        field_str = str(field.get_float())
                    else:
                        field_str = str(field.get_integer())
                else:
                    type_valid = False
            elif schema.parameter_type == ParameterType.Boolean:
                if field.is_boolean():
                    field_str = "true" if field.get_boolean() else "false"
                else:
                    type_valid = False
            elif schema.parameter_type == ParameterType.Array:
                if not field.is_array():
                    type_valid = False
            elif schema.parameter_type == ParameterType.Object:
                if not field.is_object():
                    type_valid = False

            if not type_valid:
                errors.append(ValidationError(schema.name, "Field type mismatch"))
                return False
            elif field_str and schema.parameter_type in (ParameterType.String, ParameterType.Integer, ParameterType.Float):
                ok, msg = _validate_param_value(field_str, schema)
                if not ok:
                    errors.append(ValidationError(schema.name, msg, field_str))
                    return False
        elif schema.is_required:
            errors.append(ValidationError(schema.name, "Required body field is missing"))
            return False
    elif schema.is_required:
        errors.append(ValidationError(schema.name, "Expected JSON object body"))
        return False
    return True


def validate_request(request: HttpRequest, endpoint: EndpointDefinition) -> ValidationResult:
    """Mirror ValidationEngine::ValidateRequest."""
    result = ValidationResult()
    errors = []

    # Path parameters
    for s in endpoint.parameter_schemas:
        if s.location == ParameterLocation.Path:
            val = request.path_parameters.get(s.name)
            if val is not None:
                ok, msg = _validate_param_value(val, s)
                if not ok:
                    errors.append(ValidationError(s.name, msg, val))
                    result.is_valid = False
            elif s.is_required:
                errors.append(ValidationError(s.name, "Required path parameter is missing"))
                result.is_valid = False

    # Query parameters
    for s in endpoint.parameter_schemas:
        if s.location == ParameterLocation.Query:
            val = request.query_parameters.get(s.name)
            if val is not None:
                ok, msg = _validate_param_value(val, s)
                if not ok:
                    errors.append(ValidationError(s.name, msg, val))
                    result.is_valid = False
            elif s.is_required:
                errors.append(ValidationError(s.name, "Required query parameter is missing"))
                result.is_valid = False

    # Header parameters (headers stored lowercase)
    for s in endpoint.parameter_schemas:
        if s.location == ParameterLocation.Header:
            lower_name = s.name.lower()
            val = request.headers.get(lower_name)
            if val is not None:
                ok, msg = _validate_param_value(val, s)
                if not ok:
                    errors.append(ValidationError(s.name, msg, val))
                    result.is_valid = False
            elif s.is_required:
                errors.append(ValidationError(s.name, "Required header parameter is missing"))
                result.is_valid = False

    # Body parameters
    body_schemas = [s for s in endpoint.parameter_schemas if s.location == ParameterLocation.Body]
    if body_schemas:
        if not request.body:
            for s in body_schemas:
                if s.is_required:
                    errors.append(ValidationError(s.name, "Required body parameter is missing (no body provided)"))
                    result.is_valid = False
        else:
            jv = JsonValue.parse(request.body)
            if jv is not None:
                for s in body_schemas:
                    if not _validate_json_body_field(jv, s, errors):
                        result.is_valid = False
            else:
                errors.append(ValidationError("body", "Request body is not valid JSON"))
                result.is_valid = False

    result.errors = errors
    if not result.is_valid:
        result.formatted_error_message = _format_errors(errors)
    return result


def _format_errors(errors):
    """Mirror ValidationEngine::FormatValidationErrors."""
    parts = []
    for e in errors:
        entry = '{"parameter":"' + e.parameter_name + '","message":"' + e.error_message + '"'
        if e.provided_value:
            entry += ',"provided_value":"' + e.provided_value + '"'
        entry += '}'
        parts.append(entry)
    return '{"errors":[' + ','.join(parts) + ']}'


# ---------------------------------------------------------------------------
# Helper to build schemas
# ---------------------------------------------------------------------------

def _make_schema(name, ptype, location, required=False, **kwargs):
    s = ParameterSchema()
    s.name = name
    s.parameter_type = ptype
    s.location = location
    s.is_required = required
    s.constraints = ParameterConstraints()
    for k, v in kwargs.items():
        setattr(s.constraints, k, v)
    return s


def _make_endpoint(schemas):
    ep = EndpointDefinition()
    ep.path = "/test"
    ep.method = HttpMethod.Post
    ep.backend_identifier = "svc"
    ep.parameter_schemas = schemas
    ep.parse_path_segments()
    return ep


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

class TestPathParameterValidation(unittest.TestCase):
    """GW-006 AC1: Path parameter extraction and validation."""

    def test_valid_path_param(self):
        """GW-006 AC1: Valid path parameter passes validation."""
        ep = _make_endpoint([
            _make_schema("id", ParameterType.Integer, ParameterLocation.Path, required=True, min_value=1)
        ])
        req = HttpRequest()
        req.path_parameters = {"id": "42"}
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)

    def test_invalid_path_param_type(self):
        """GW-006 AC1: Path parameter with wrong type fails."""
        ep = _make_endpoint([
            _make_schema("id", ParameterType.Integer, ParameterLocation.Path, required=True)
        ])
        req = HttpRequest()
        req.path_parameters = {"id": "abc"}
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)

    def test_missing_required_path_param(self):
        """GW-006 AC1: Missing required path parameter fails."""
        ep = _make_endpoint([
            _make_schema("id", ParameterType.Integer, ParameterLocation.Path, required=True)
        ])
        req = HttpRequest()
        req.path_parameters = {}
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)
        self.assertIn("missing", result.errors[0].error_message.lower())


class TestQueryParameterValidation(unittest.TestCase):
    """GW-006 AC2: Query string parameter validation."""

    def test_valid_query_param(self):
        """GW-006 AC2: Valid query parameter passes."""
        ep = _make_endpoint([
            _make_schema("page", ParameterType.Integer, ParameterLocation.Query, required=True, min_value=1)
        ])
        req = HttpRequest()
        req.query_parameters = {"page": "5"}
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)

    def test_missing_required_query_param(self):
        """GW-006 AC2: Missing required query parameter fails."""
        ep = _make_endpoint([
            _make_schema("page", ParameterType.Integer, ParameterLocation.Query, required=True)
        ])
        req = HttpRequest()
        req.query_parameters = {}
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)

    def test_optional_query_param_absent(self):
        """GW-006 AC2: Missing optional query parameter passes."""
        ep = _make_endpoint([
            _make_schema("page", ParameterType.Integer, ParameterLocation.Query, required=False)
        ])
        req = HttpRequest()
        req.query_parameters = {}
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)


class TestHeaderParameterValidation(unittest.TestCase):
    """GW-006 AC3: Header parameter validation."""

    def test_valid_header_param(self):
        """GW-006 AC3: Valid header parameter passes."""
        ep = _make_endpoint([
            _make_schema("X-API-Key", ParameterType.String, ParameterLocation.Header,
                         required=True, min_length=8)
        ])
        req = HttpRequest()
        req.headers = {"x-api-key": "my-secret-key-12345"}
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)

    def test_missing_required_header(self):
        """GW-006 AC3: Missing required header parameter fails."""
        ep = _make_endpoint([
            _make_schema("X-API-Key", ParameterType.String, ParameterLocation.Header, required=True)
        ])
        req = HttpRequest()
        req.headers = {}
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)


class TestJsonBodyValidation(unittest.TestCase):
    """GW-006 AC4: JSON body field validation."""

    def test_valid_body_fields(self):
        """GW-006 AC4: Valid JSON body fields pass."""
        ep = _make_endpoint([
            _make_schema("name", ParameterType.String, ParameterLocation.Body, required=True),
            _make_schema("age", ParameterType.Integer, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"name": "Alice", "age": 30})
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)

    def test_missing_required_body_field(self):
        """GW-006 AC4: Missing required body field fails."""
        ep = _make_endpoint([
            _make_schema("name", ParameterType.String, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"other": "value"})
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)

    def test_no_body_with_required_fields(self):
        """GW-006 AC4: No body when body fields are required fails."""
        ep = _make_endpoint([
            _make_schema("name", ParameterType.String, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = ""
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)

    def test_invalid_json_body(self):
        """GW-006 AC4: Invalid JSON body fails."""
        ep = _make_endpoint([
            _make_schema("name", ParameterType.String, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = "not json{{{}"
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)


class TestNestedObjectBodyValidation(unittest.TestCase):
    """GW-006 AC5: Nested object body validation."""

    def test_object_field_valid(self):
        """GW-006 AC5: Object field in body passes when type matches."""
        ep = _make_endpoint([
            _make_schema("address", ParameterType.Object, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"address": {"street": "Main", "city": "NY"}})
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)

    def test_object_field_wrong_type(self):
        """GW-006 AC5: Non-object where Object expected fails."""
        ep = _make_endpoint([
            _make_schema("address", ParameterType.Object, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"address": "not an object"})
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)


class TestTypeMismatches(unittest.TestCase):
    """GW-006 AC6: Type mismatches."""

    def test_string_where_integer_expected(self):
        """GW-006 AC6: String value where integer field expected fails."""
        ep = _make_endpoint([
            _make_schema("count", ParameterType.Integer, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"count": "not a number"})
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)
        self.assertIn("type mismatch", result.errors[0].error_message.lower())

    def test_integer_where_boolean_expected(self):
        """GW-006 AC6: Integer where boolean expected fails (JSON type check)."""
        ep = _make_endpoint([
            _make_schema("active", ParameterType.Boolean, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"active": 1})  # int, not bool
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)


class TestConstraintViolations(unittest.TestCase):
    """GW-006 AC7: Constraint violations."""

    def test_string_too_long(self):
        """GW-006 AC7: String exceeding max_length fails."""
        ep = _make_endpoint([
            _make_schema("name", ParameterType.String, ParameterLocation.Body,
                         required=True, max_length=5)
        ])
        req = HttpRequest()
        req.body = json.dumps({"name": "toolong"})
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)

    def test_integer_out_of_range(self):
        """GW-006 AC7: Integer below minimum fails."""
        ep = _make_endpoint([
            _make_schema("age", ParameterType.Integer, ParameterLocation.Body,
                         required=True, min_value=18)
        ])
        req = HttpRequest()
        req.body = json.dumps({"age": 10})
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)


class TestStructuredErrorResponse(unittest.TestCase):
    """GW-006 AC8: Structured error response format."""

    def test_error_format_json(self):
        """GW-006 AC8: Formatted error is valid JSON with errors array."""
        ep = _make_endpoint([
            _make_schema("name", ParameterType.String, ParameterLocation.Body, required=True),
            _make_schema("age", ParameterType.Integer, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({})  # both fields missing
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)
        parsed = json.loads(result.formatted_error_message)
        self.assertIn("errors", parsed)
        self.assertIsInstance(parsed["errors"], list)
        self.assertEqual(len(parsed["errors"]), 2)
        for err in parsed["errors"]:
            self.assertIn("parameter", err)
            self.assertIn("message", err)


class TestUnknownFieldHandling(unittest.TestCase):
    """GW-006 AC9: Unknown field handling (extra fields are allowed / not validated)."""

    def test_extra_fields_in_body_pass(self):
        """GW-006 AC9: Extra fields in body do not cause validation failure."""
        ep = _make_endpoint([
            _make_schema("name", ParameterType.String, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"name": "Alice", "extra": "ignored"})
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)


class TestArrayBodyField(unittest.TestCase):
    """GW-006: Array body field validation."""

    def test_array_field_valid(self):
        """GW-006: Array field in body passes."""
        ep = _make_endpoint([
            _make_schema("tags", ParameterType.Array, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"tags": ["a", "b"]})
        result = validate_request(req, ep)
        self.assertTrue(result.is_valid)

    def test_array_field_wrong_type(self):
        """GW-006: Non-array where Array expected fails."""
        ep = _make_endpoint([
            _make_schema("tags", ParameterType.Array, ParameterLocation.Body, required=True)
        ])
        req = HttpRequest()
        req.body = json.dumps({"tags": "not-array"})
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)


class TestMultipleValidationErrors(unittest.TestCase):
    """GW-006: All validation errors are collected."""

    def test_multiple_errors(self):
        """GW-006: Multiple validation errors are reported together."""
        ep = _make_endpoint([
            _make_schema("q", ParameterType.String, ParameterLocation.Query, required=True),
            _make_schema("name", ParameterType.String, ParameterLocation.Body, required=True),
        ])
        req = HttpRequest()
        req.query_parameters = {}
        req.body = json.dumps({})
        result = validate_request(req, ep)
        self.assertFalse(result.is_valid)
        self.assertEqual(len(result.errors), 2)


if __name__ == '__main__':
    unittest.main()
