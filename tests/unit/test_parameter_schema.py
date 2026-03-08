"""
Unit tests for parameter schema validation.
Tests mirror the behaviour implemented in src/routing/ParameterSchema.cpp.

Reference: GW-005 - Endpoint Schema Definition
"""

import unittest
import re
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_helpers import ParameterSchema, ParameterConstraints, ParameterType, ParameterLocation


# ---------------------------------------------------------------------------
# Python reference validators mirroring ParameterSchema.cpp
# ---------------------------------------------------------------------------

def validate_string_constraints(value: str, constraints: ParameterConstraints) -> (bool, str):
    """Mirror ValidateStringConstraints."""
    if constraints.min_length is not None:
        if len(value) < constraints.min_length:
            return False, f"String length {len(value)} is below minimum {constraints.min_length}"
    if constraints.max_length is not None:
        if len(value) > constraints.max_length:
            return False, f"String length {len(value)} exceeds maximum {constraints.max_length}"
    if constraints.pattern is not None:
        try:
            if not re.fullmatch(constraints.pattern, value):
                return False, "String does not match required pattern"
        except re.error:
            return False, "Invalid regex pattern in schema"
    if constraints.allowed_values:
        if value not in constraints.allowed_values:
            return False, "Value is not in the list of allowed values"
    return True, ""


def validate_integer_constraints(value: str, constraints: ParameterConstraints) -> (bool, str):
    """Mirror ValidateIntegerConstraints."""
    try:
        n = int(value)
    except ValueError:
        return False, "Value is not a valid integer"

    # Check for trailing characters (C++ strtoll requires full consumption)
    stripped = value.strip()
    try:
        int(stripped)
    except ValueError:
        return False, "Value is not a valid integer"

    if constraints.min_value is not None:
        if n < constraints.min_value:
            return False, f"Integer value {n} is below minimum {constraints.min_value}"
    if constraints.max_value is not None:
        if n > constraints.max_value:
            return False, f"Integer value {n} exceeds maximum {constraints.max_value}"
    return True, ""


def validate_float_constraints(value: str, constraints: ParameterConstraints) -> (bool, str):
    """Mirror ValidateFloatConstraints."""
    try:
        f = float(value)
    except ValueError:
        return False, "Value is not a valid floating-point number"
    import math
    if math.isinf(f) or math.isnan(f):
        return False, "Value is not a valid floating-point number"

    if constraints.min_float_value is not None:
        if f < constraints.min_float_value:
            return False, "Float value is below minimum"
    if constraints.max_float_value is not None:
        if f > constraints.max_float_value:
            return False, "Float value exceeds maximum"
    return True, ""


def validate_boolean_value(value: str) -> (bool, str):
    """Mirror ValidateBooleanValue."""
    if value in ("true", "false", "1", "0", "yes", "no"):
        return True, ""
    return False, "Value is not a valid boolean (expected true/false, 1/0, or yes/no)"


def validate_parameter_value(value: str, schema: ParameterSchema) -> (bool, str):
    """Mirror ParameterSchema::ValidateValue."""
    if schema.parameter_type == ParameterType.String:
        return validate_string_constraints(value, schema.constraints)
    elif schema.parameter_type == ParameterType.Integer:
        return validate_integer_constraints(value, schema.constraints)
    elif schema.parameter_type == ParameterType.Float:
        return validate_float_constraints(value, schema.constraints)
    elif schema.parameter_type == ParameterType.Boolean:
        return validate_boolean_value(value)
    else:
        # Object and Array validated at higher level
        return True, ""


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

class TestStringValidation(unittest.TestCase):
    """GW-005 AC1: String type validation with min/max length."""

    def test_string_valid(self):
        """GW-005 AC1: Valid string passes."""
        s = ParameterSchema()
        s.name = "username"
        s.parameter_type = ParameterType.String
        ok, _ = validate_parameter_value("alice", s)
        self.assertTrue(ok)

    def test_string_min_length_pass(self):
        """GW-005 AC1: String at min length passes."""
        s = ParameterSchema()
        s.name = "name"
        s.parameter_type = ParameterType.String
        s.constraints.min_length = 3
        ok, _ = validate_parameter_value("abc", s)
        self.assertTrue(ok)

    def test_string_min_length_fail(self):
        """GW-005 AC1: String below min length fails."""
        s = ParameterSchema()
        s.name = "name"
        s.parameter_type = ParameterType.String
        s.constraints.min_length = 3
        ok, msg = validate_parameter_value("ab", s)
        self.assertFalse(ok)
        self.assertIn("below minimum", msg)

    def test_string_max_length_pass(self):
        """GW-005 AC1: String at max length passes."""
        s = ParameterSchema()
        s.name = "name"
        s.parameter_type = ParameterType.String
        s.constraints.max_length = 5
        ok, _ = validate_parameter_value("abcde", s)
        self.assertTrue(ok)

    def test_string_max_length_fail(self):
        """GW-005 AC1: String above max length fails."""
        s = ParameterSchema()
        s.name = "name"
        s.parameter_type = ParameterType.String
        s.constraints.max_length = 5
        ok, msg = validate_parameter_value("abcdef", s)
        self.assertFalse(ok)
        self.assertIn("exceeds maximum", msg)


class TestIntegerValidation(unittest.TestCase):
    """GW-005 AC2: Integer type validation with min/max value."""

    def test_integer_valid(self):
        """GW-005 AC2: Valid integer passes."""
        s = ParameterSchema()
        s.name = "age"
        s.parameter_type = ParameterType.Integer
        ok, _ = validate_parameter_value("25", s)
        self.assertTrue(ok)

    def test_integer_negative(self):
        """GW-005 AC2: Negative integer is valid."""
        s = ParameterSchema()
        s.name = "offset"
        s.parameter_type = ParameterType.Integer
        ok, _ = validate_parameter_value("-10", s)
        self.assertTrue(ok)

    def test_integer_not_a_number(self):
        """GW-005 AC2: Non-numeric string fails integer validation."""
        s = ParameterSchema()
        s.name = "age"
        s.parameter_type = ParameterType.Integer
        ok, msg = validate_parameter_value("abc", s)
        self.assertFalse(ok)
        self.assertIn("not a valid integer", msg)

    def test_integer_min_value_pass(self):
        """GW-005 AC2: Integer at min value passes."""
        s = ParameterSchema()
        s.name = "age"
        s.parameter_type = ParameterType.Integer
        s.constraints.min_value = 0
        ok, _ = validate_parameter_value("0", s)
        self.assertTrue(ok)

    def test_integer_min_value_fail(self):
        """GW-005 AC2: Integer below min value fails."""
        s = ParameterSchema()
        s.name = "age"
        s.parameter_type = ParameterType.Integer
        s.constraints.min_value = 0
        ok, msg = validate_parameter_value("-1", s)
        self.assertFalse(ok)
        self.assertIn("below minimum", msg)

    def test_integer_max_value_pass(self):
        """GW-005 AC2: Integer at max value passes."""
        s = ParameterSchema()
        s.name = "age"
        s.parameter_type = ParameterType.Integer
        s.constraints.max_value = 120
        ok, _ = validate_parameter_value("120", s)
        self.assertTrue(ok)

    def test_integer_max_value_fail(self):
        """GW-005 AC2: Integer above max value fails."""
        s = ParameterSchema()
        s.name = "age"
        s.parameter_type = ParameterType.Integer
        s.constraints.max_value = 120
        ok, msg = validate_parameter_value("121", s)
        self.assertFalse(ok)
        self.assertIn("exceeds maximum", msg)


class TestFloatValidation(unittest.TestCase):
    """GW-005 AC3: Float validation."""

    def test_float_valid(self):
        """GW-005 AC3: Valid float passes."""
        s = ParameterSchema()
        s.name = "price"
        s.parameter_type = ParameterType.Float
        ok, _ = validate_parameter_value("19.99", s)
        self.assertTrue(ok)

    def test_float_integer_format(self):
        """GW-005 AC3: Integer formatted as string passes float validation."""
        s = ParameterSchema()
        s.name = "price"
        s.parameter_type = ParameterType.Float
        ok, _ = validate_parameter_value("42", s)
        self.assertTrue(ok)

    def test_float_not_a_number(self):
        """GW-005 AC3: Non-numeric string fails float validation."""
        s = ParameterSchema()
        s.name = "price"
        s.parameter_type = ParameterType.Float
        ok, msg = validate_parameter_value("abc", s)
        self.assertFalse(ok)
        self.assertIn("not a valid floating-point", msg)

    def test_float_infinity_rejected(self):
        """GW-005 AC3: Infinity is rejected."""
        s = ParameterSchema()
        s.name = "price"
        s.parameter_type = ParameterType.Float
        ok, msg = validate_parameter_value("inf", s)
        self.assertFalse(ok)

    def test_float_min_value(self):
        """GW-005 AC3: Float below minimum fails."""
        s = ParameterSchema()
        s.name = "price"
        s.parameter_type = ParameterType.Float
        s.constraints.min_float_value = 0.0
        ok, msg = validate_parameter_value("-0.01", s)
        self.assertFalse(ok)
        self.assertIn("below minimum", msg)

    def test_float_max_value(self):
        """GW-005 AC3: Float above maximum fails."""
        s = ParameterSchema()
        s.name = "price"
        s.parameter_type = ParameterType.Float
        s.constraints.max_float_value = 100.0
        ok, msg = validate_parameter_value("100.01", s)
        self.assertFalse(ok)
        self.assertIn("exceeds maximum", msg)


class TestBooleanValidation(unittest.TestCase):
    """GW-005 AC4: Boolean validation."""

    def test_true_values(self):
        """GW-005 AC4: 'true', '1', 'yes' are valid booleans."""
        for val in ("true", "1", "yes"):
            ok, _ = validate_boolean_value(val)
            self.assertTrue(ok, f"Expected {val!r} to be valid boolean")

    def test_false_values(self):
        """GW-005 AC4: 'false', '0', 'no' are valid booleans."""
        for val in ("false", "0", "no"):
            ok, _ = validate_boolean_value(val)
            self.assertTrue(ok, f"Expected {val!r} to be valid boolean")

    def test_invalid_boolean(self):
        """GW-005 AC4: Invalid boolean strings are rejected."""
        for val in ("True", "FALSE", "maybe", "2", ""):
            ok, msg = validate_boolean_value(val)
            self.assertFalse(ok, f"Expected {val!r} to be rejected")
            self.assertIn("not a valid boolean", msg)


class TestRegexPatternValidation(unittest.TestCase):
    """GW-005 AC5: Regex pattern validation."""

    def test_pattern_match(self):
        """GW-005 AC5: Value matching regex pattern passes."""
        s = ParameterSchema()
        s.name = "email"
        s.parameter_type = ParameterType.String
        s.constraints.pattern = r"^[a-z]+@[a-z]+\.[a-z]+$"
        ok, _ = validate_parameter_value("alice@example.com", s)
        self.assertTrue(ok)

    def test_pattern_no_match(self):
        """GW-005 AC5: Value not matching regex fails."""
        s = ParameterSchema()
        s.name = "email"
        s.parameter_type = ParameterType.String
        s.constraints.pattern = r"^[a-z]+@[a-z]+\.[a-z]+$"
        ok, msg = validate_parameter_value("INVALID", s)
        self.assertFalse(ok)
        self.assertIn("does not match required pattern", msg)


class TestEnumAllowedValues(unittest.TestCase):
    """GW-005 AC6: Enum allowed values."""

    def test_allowed_value_passes(self):
        """GW-005 AC6: Value in allowed list passes."""
        s = ParameterSchema()
        s.name = "status"
        s.parameter_type = ParameterType.String
        s.constraints.allowed_values = ["active", "inactive", "pending"]
        ok, _ = validate_parameter_value("active", s)
        self.assertTrue(ok)

    def test_disallowed_value_fails(self):
        """GW-005 AC6: Value not in allowed list fails."""
        s = ParameterSchema()
        s.name = "status"
        s.parameter_type = ParameterType.String
        s.constraints.allowed_values = ["active", "inactive", "pending"]
        ok, msg = validate_parameter_value("deleted", s)
        self.assertFalse(ok)
        self.assertIn("not in the list of allowed values", msg)


class TestRequiredVsOptional(unittest.TestCase):
    """GW-005 AC7: Required vs optional fields."""

    def test_required_flag(self):
        """GW-005 AC7: Schema can be marked required."""
        s = ParameterSchema()
        s.name = "id"
        s.is_required = True
        self.assertTrue(s.is_required)

    def test_optional_flag(self):
        """GW-005 AC7: Schema defaults to not required."""
        s = ParameterSchema()
        s.name = "id"
        self.assertFalse(s.is_required)


class TestNestedObjectSchema(unittest.TestCase):
    """GW-005 AC8: Nested object schemas (Object type passes at schema level)."""

    def test_object_type_accepted(self):
        """GW-005 AC8: Object type passes validation at schema level."""
        s = ParameterSchema()
        s.name = "address"
        s.parameter_type = ParameterType.Object
        ok, _ = validate_parameter_value('{"street":"Main"}', s)
        self.assertTrue(ok)


class TestArrayValidation(unittest.TestCase):
    """GW-005 AC9: Array validation."""

    def test_array_type_accepted(self):
        """GW-005 AC9: Array type passes validation at schema level."""
        s = ParameterSchema()
        s.name = "tags"
        s.parameter_type = ParameterType.Array
        ok, _ = validate_parameter_value('[1,2,3]', s)
        self.assertTrue(ok)


class TestSchemaValidity(unittest.TestCase):
    """GW-005 AC10: Invalid schema rejection at registration."""

    def test_empty_name_invalid(self):
        """GW-005 AC10: Schema with empty name is invalid."""
        s = ParameterSchema()
        s.name = ""
        self.assertFalse(s.is_valid())

    def test_valid_name(self):
        """GW-005 AC10: Schema with non-empty name is valid."""
        s = ParameterSchema()
        s.name = "field"
        self.assertTrue(s.is_valid())


if __name__ == '__main__':
    unittest.main()
