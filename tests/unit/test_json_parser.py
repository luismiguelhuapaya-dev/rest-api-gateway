"""
Unit tests for JSON parsing.
Tests mirror the behaviour implemented in src/validation/JsonParser.cpp.

Reference: JSON parser used across GW-005, GW-006, GW-007, GW-013
"""

import json
import unittest
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))


# ---------------------------------------------------------------------------
# Python reference JSON parser mirroring JsonParser.cpp
# ---------------------------------------------------------------------------

class JsonValueType:
    Null = 0
    Boolean = 1
    Integer = 2
    Float = 3
    String = 4
    Array = 5
    Object = 6


class JsonValue:
    """Python reference implementation of the C++ JsonValue class."""

    def __init__(self, vtype=JsonValueType.Null, value=None):
        self.vtype = vtype
        self.value = value  # native Python value

    def is_null(self):
        return self.vtype == JsonValueType.Null

    def is_boolean(self):
        return self.vtype == JsonValueType.Boolean

    def is_integer(self):
        return self.vtype == JsonValueType.Integer

    def is_float(self):
        return self.vtype == JsonValueType.Float

    def is_string(self):
        return self.vtype == JsonValueType.String

    def is_array(self):
        return self.vtype == JsonValueType.Array

    def is_object(self):
        return self.vtype == JsonValueType.Object

    def get_boolean(self):
        return self.value

    def get_integer(self):
        return self.value

    def get_float(self):
        return self.value

    def get_string(self):
        return self.value

    def get_array_size(self):
        return len(self.value) if self.vtype == JsonValueType.Array else 0

    def get_array_element(self, idx):
        return self.value[idx] if idx < len(self.value) else JsonValue()

    def has_member(self, key):
        if self.vtype != JsonValueType.Object:
            return False
        return any(k == key for k, _ in self.value)

    def get_member(self, key):
        if self.vtype == JsonValueType.Object:
            for k, v in self.value:
                if k == key:
                    return v
        return JsonValue()

    def get_member_keys(self):
        if self.vtype == JsonValueType.Object:
            return [k for k, _ in self.value]
        return []


class ReferenceJsonParser:
    """Python reference implementation mirroring JsonParser.cpp."""

    def __init__(self):
        self._input = ""
        self._pos = 0
        self._error_message = ""
        self._error_position = 0

    def parse(self, text: str):
        """Returns (success, JsonValue)."""
        self._input = text
        self._pos = 0
        self._error_message = ""
        self._error_position = 0

        self._skip_whitespace()
        if not self._has_more():
            self._set_error("Empty input")
            return False, JsonValue()

        ok, val = self._parse_value()
        if ok:
            self._skip_whitespace()
        return ok, val

    def get_error_message(self):
        return self._error_message

    def get_error_position(self):
        return self._error_position

    def _parse_value(self):
        self._skip_whitespace()
        if not self._has_more():
            self._set_error("Unexpected end of input")
            return False, JsonValue()

        ch = self._current()
        if ch == 'n':
            return self._parse_null()
        elif ch in ('t', 'f'):
            return self._parse_boolean()
        elif ch == '-' or ('0' <= ch <= '9'):
            return self._parse_number()
        elif ch == '"':
            return self._parse_string_value()
        elif ch == '[':
            return self._parse_array()
        elif ch == '{':
            return self._parse_object()
        else:
            self._set_error("Unexpected character")
            return False, JsonValue()

    def _parse_null(self):
        if self._input[self._pos:self._pos + 4] == "null":
            self._pos += 4
            return True, JsonValue(JsonValueType.Null)
        self._set_error("Invalid null value")
        return False, JsonValue()

    def _parse_boolean(self):
        if self._input[self._pos:self._pos + 4] == "true":
            self._pos += 4
            return True, JsonValue(JsonValueType.Boolean, True)
        if self._input[self._pos:self._pos + 5] == "false":
            self._pos += 5
            return True, JsonValue(JsonValueType.Boolean, False)
        self._set_error("Invalid boolean value")
        return False, JsonValue()

    def _parse_number(self):
        start = self._pos
        is_float = False

        if self._has_more() and self._current() == '-':
            self._next()
        while self._has_more() and '0' <= self._current() <= '9':
            self._next()
        if self._has_more() and self._current() == '.':
            is_float = True
            self._next()
            while self._has_more() and '0' <= self._current() <= '9':
                self._next()
        if self._has_more() and self._current() in ('e', 'E'):
            is_float = True
            self._next()
            if self._has_more() and self._current() in ('+', '-'):
                self._next()
            while self._has_more() and '0' <= self._current() <= '9':
                self._next()

        if self._pos > start:
            num_str = self._input[start:self._pos]
            if is_float:
                return True, JsonValue(JsonValueType.Float, float(num_str))
            else:
                return True, JsonValue(JsonValueType.Integer, int(num_str))
        self._set_error("Invalid number")
        return False, JsonValue()

    def _parse_raw_string(self):
        if not self._has_more() or self._current() != '"':
            self._set_error("Expected string")
            return False, ""
        self._next()  # skip opening quote
        result = []
        while self._has_more() and self._current() != '"':
            if self._current() == '\\':
                self._next()
                if not self._has_more():
                    self._set_error("Unexpected end of string")
                    return False, ""
                esc = self._current()
                if esc == '"':
                    result.append('"')
                elif esc == '\\':
                    result.append('\\')
                elif esc == '/':
                    result.append('/')
                elif esc == 'b':
                    result.append('\b')
                elif esc == 'f':
                    result.append('\f')
                elif esc == 'n':
                    result.append('\n')
                elif esc == 'r':
                    result.append('\r')
                elif esc == 't':
                    result.append('\t')
                elif esc == 'u':
                    if self._pos + 4 < len(self._input):
                        hex_str = self._input[self._pos + 1:self._pos + 5]
                        cp = int(hex_str, 16)
                        result.append(chr(cp))
                        self._pos += 4
                    else:
                        self._set_error("Invalid unicode escape")
                        return False, ""
                else:
                    self._set_error("Invalid escape character")
                    return False, ""
                self._next()
            else:
                result.append(self._current())
                self._next()

        if self._has_more() and self._current() == '"':
            self._next()  # skip closing quote
        else:
            self._set_error("Unterminated string")
            return False, ""
        return True, "".join(result)

    def _parse_string_value(self):
        ok, s = self._parse_raw_string()
        if ok:
            return True, JsonValue(JsonValueType.String, s)
        return False, JsonValue()

    def _parse_array(self):
        if not self._has_more() or self._current() != '[':
            return False, JsonValue()
        self._next()
        self._skip_whitespace()

        elements = []
        if self._has_more() and self._current() == ']':
            self._next()
            return True, JsonValue(JsonValueType.Array, elements)

        while self._has_more():
            self._skip_whitespace()
            ok, val = self._parse_value()
            if not ok:
                return False, JsonValue()
            elements.append(val)
            self._skip_whitespace()
            if self._has_more() and self._current() == ',':
                self._next()
            elif self._has_more() and self._current() == ']':
                self._next()
                break
            else:
                self._set_error("Expected ',' or ']' in array")
                return False, JsonValue()

        return True, JsonValue(JsonValueType.Array, elements)

    def _parse_object(self):
        if not self._has_more() or self._current() != '{':
            return False, JsonValue()
        self._next()
        self._skip_whitespace()

        members = []
        if self._has_more() and self._current() == '}':
            self._next()
            return True, JsonValue(JsonValueType.Object, members)

        while self._has_more():
            self._skip_whitespace()
            ok, key = self._parse_raw_string()
            if not ok:
                return False, JsonValue()
            self._skip_whitespace()
            if not self._has_more() or self._current() != ':':
                self._set_error("Expected ':' after object key")
                return False, JsonValue()
            self._next()
            self._skip_whitespace()
            ok, val = self._parse_value()
            if not ok:
                return False, JsonValue()
            members.append((key, val))
            self._skip_whitespace()
            if self._has_more() and self._current() == ',':
                self._next()
            elif self._has_more() and self._current() == '}':
                self._next()
                break
            else:
                self._set_error("Expected ',' or '}' in object")
                return False, JsonValue()

        return True, JsonValue(JsonValueType.Object, members)

    def _skip_whitespace(self):
        while self._has_more() and self._current() in (' ', '\t', '\n', '\r'):
            self._next()

    def _current(self):
        return self._input[self._pos] if self._pos < len(self._input) else '\0'

    def _next(self):
        self._pos += 1

    def _has_more(self):
        return self._pos < len(self._input)

    def _set_error(self, msg):
        self._error_message = msg
        self._error_position = self._pos


# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

class TestBasicTypes(unittest.TestCase):
    """JSON parser: basic types."""

    def test_null(self):
        """JSON: Parse null value."""
        p = ReferenceJsonParser()
        ok, val = p.parse("null")
        self.assertTrue(ok)
        self.assertTrue(val.is_null())

    def test_true(self):
        """JSON: Parse true boolean."""
        p = ReferenceJsonParser()
        ok, val = p.parse("true")
        self.assertTrue(ok)
        self.assertTrue(val.is_boolean())
        self.assertTrue(val.get_boolean())

    def test_false(self):
        """JSON: Parse false boolean."""
        p = ReferenceJsonParser()
        ok, val = p.parse("false")
        self.assertTrue(ok)
        self.assertTrue(val.is_boolean())
        self.assertFalse(val.get_boolean())

    def test_positive_integer(self):
        """JSON: Parse positive integer."""
        p = ReferenceJsonParser()
        ok, val = p.parse("42")
        self.assertTrue(ok)
        self.assertTrue(val.is_integer())
        self.assertEqual(val.get_integer(), 42)

    def test_negative_integer(self):
        """JSON: Parse negative integer."""
        p = ReferenceJsonParser()
        ok, val = p.parse("-7")
        self.assertTrue(ok)
        self.assertTrue(val.is_integer())
        self.assertEqual(val.get_integer(), -7)

    def test_float(self):
        """JSON: Parse float."""
        p = ReferenceJsonParser()
        ok, val = p.parse("3.14")
        self.assertTrue(ok)
        self.assertTrue(val.is_float())
        self.assertAlmostEqual(val.get_float(), 3.14)

    def test_float_with_exponent(self):
        """JSON: Parse float with exponent."""
        p = ReferenceJsonParser()
        ok, val = p.parse("1e10")
        self.assertTrue(ok)
        self.assertTrue(val.is_float())
        self.assertAlmostEqual(val.get_float(), 1e10)

    def test_string(self):
        """JSON: Parse string."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"hello"')
        self.assertTrue(ok)
        self.assertTrue(val.is_string())
        self.assertEqual(val.get_string(), "hello")

    def test_empty_string(self):
        """JSON: Parse empty string."""
        p = ReferenceJsonParser()
        ok, val = p.parse('""')
        self.assertTrue(ok)
        self.assertTrue(val.is_string())
        self.assertEqual(val.get_string(), "")


class TestNestedObjects(unittest.TestCase):
    """JSON parser: nested objects."""

    def test_simple_object(self):
        """JSON: Parse simple object."""
        p = ReferenceJsonParser()
        ok, val = p.parse('{"key":"value"}')
        self.assertTrue(ok)
        self.assertTrue(val.is_object())
        self.assertTrue(val.has_member("key"))
        self.assertEqual(val.get_member("key").get_string(), "value")

    def test_nested_object(self):
        """JSON: Parse nested object."""
        p = ReferenceJsonParser()
        ok, val = p.parse('{"outer":{"inner":42}}')
        self.assertTrue(ok)
        outer = val.get_member("outer")
        self.assertTrue(outer.is_object())
        self.assertEqual(outer.get_member("inner").get_integer(), 42)

    def test_empty_object(self):
        """JSON: Parse empty object."""
        p = ReferenceJsonParser()
        ok, val = p.parse('{}')
        self.assertTrue(ok)
        self.assertTrue(val.is_object())
        self.assertEqual(val.get_member_keys(), [])

    def test_multiple_members(self):
        """JSON: Parse object with multiple members."""
        p = ReferenceJsonParser()
        ok, val = p.parse('{"a":1,"b":2,"c":3}')
        self.assertTrue(ok)
        self.assertEqual(val.get_member("a").get_integer(), 1)
        self.assertEqual(val.get_member("b").get_integer(), 2)
        self.assertEqual(val.get_member("c").get_integer(), 3)


class TestArrays(unittest.TestCase):
    """JSON parser: arrays."""

    def test_simple_array(self):
        """JSON: Parse simple array."""
        p = ReferenceJsonParser()
        ok, val = p.parse('[1, 2, 3]')
        self.assertTrue(ok)
        self.assertTrue(val.is_array())
        self.assertEqual(val.get_array_size(), 3)
        self.assertEqual(val.get_array_element(0).get_integer(), 1)

    def test_empty_array(self):
        """JSON: Parse empty array."""
        p = ReferenceJsonParser()
        ok, val = p.parse('[]')
        self.assertTrue(ok)
        self.assertTrue(val.is_array())
        self.assertEqual(val.get_array_size(), 0)

    def test_mixed_type_array(self):
        """JSON: Parse array with mixed types."""
        p = ReferenceJsonParser()
        ok, val = p.parse('[1, "two", true, null]')
        self.assertTrue(ok)
        self.assertEqual(val.get_array_size(), 4)
        self.assertTrue(val.get_array_element(0).is_integer())
        self.assertTrue(val.get_array_element(1).is_string())
        self.assertTrue(val.get_array_element(2).is_boolean())
        self.assertTrue(val.get_array_element(3).is_null())

    def test_nested_array(self):
        """JSON: Parse nested array."""
        p = ReferenceJsonParser()
        ok, val = p.parse('[[1,2],[3,4]]')
        self.assertTrue(ok)
        self.assertEqual(val.get_array_size(), 2)
        inner = val.get_array_element(0)
        self.assertTrue(inner.is_array())
        self.assertEqual(inner.get_array_size(), 2)


class TestEscapedCharacters(unittest.TestCase):
    """JSON parser: escaped characters."""

    def test_escaped_quote(self):
        """JSON: Parse escaped double quote."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"hello \\"world\\""')
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), 'hello "world"')

    def test_escaped_backslash(self):
        """JSON: Parse escaped backslash."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"path\\\\file"')
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), "path\\file")

    def test_escaped_newline(self):
        """JSON: Parse escaped newline."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"line1\\nline2"')
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), "line1\nline2")

    def test_escaped_tab(self):
        """JSON: Parse escaped tab."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"col1\\tcol2"')
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), "col1\tcol2")

    def test_escaped_slash(self):
        """JSON: Parse escaped forward slash."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"a\\/b"')
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), "a/b")


class TestUnicode(unittest.TestCase):
    """JSON parser: unicode escape sequences."""

    def test_unicode_ascii(self):
        """JSON: Parse unicode escape for ASCII character."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"\\u0041"')  # 'A'
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), "A")

    def test_unicode_non_ascii(self):
        """JSON: Parse unicode escape for non-ASCII character."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"\\u00e9"')  # e with acute accent
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), "\u00e9")

    def test_unicode_cjk(self):
        """JSON: Parse unicode escape for CJK character."""
        p = ReferenceJsonParser()
        ok, val = p.parse('"\\u4e16"')  # Chinese character
        self.assertTrue(ok)
        self.assertEqual(val.get_string(), "\u4e16")


class TestMalformedJson(unittest.TestCase):
    """JSON parser: malformed JSON rejection."""

    def test_empty_input(self):
        """JSON: Empty input is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse("")
        self.assertFalse(ok)
        self.assertIn("Empty", p.get_error_message())

    def test_truncated_object(self):
        """JSON: Truncated object is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse('{"key": "val"')
        self.assertFalse(ok)

    def test_truncated_array(self):
        """JSON: Truncated array is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse('[1, 2')
        self.assertFalse(ok)

    def test_missing_colon(self):
        """JSON: Object with missing colon is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse('{"key" "value"}')
        self.assertFalse(ok)

    def test_trailing_comma_object(self):
        """JSON: Trailing comma in object is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse('{"a":1,}')
        self.assertFalse(ok)

    def test_trailing_comma_array(self):
        """JSON: Trailing comma in array is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse('[1,]')
        self.assertFalse(ok)

    def test_invalid_literal(self):
        """JSON: Invalid literal is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse('undefined')
        self.assertFalse(ok)

    def test_unterminated_string(self):
        """JSON: Unterminated string is rejected."""
        p = ReferenceJsonParser()
        ok, _ = p.parse('"no closing quote')
        self.assertFalse(ok)


class TestWhitespaceHandling(unittest.TestCase):
    """JSON parser: whitespace handling."""

    def test_leading_whitespace(self):
        """JSON: Leading whitespace is handled."""
        p = ReferenceJsonParser()
        ok, val = p.parse("  42  ")
        self.assertTrue(ok)
        self.assertEqual(val.get_integer(), 42)

    def test_whitespace_in_object(self):
        """JSON: Whitespace around object members is handled."""
        p = ReferenceJsonParser()
        ok, val = p.parse('  {  "key"  :  "value"  }  ')
        self.assertTrue(ok)
        self.assertTrue(val.is_object())

    def test_whitespace_in_array(self):
        """JSON: Whitespace around array elements is handled."""
        p = ReferenceJsonParser()
        ok, val = p.parse('  [  1  ,  2  ,  3  ]  ')
        self.assertTrue(ok)
        self.assertEqual(val.get_array_size(), 3)


if __name__ == '__main__':
    unittest.main()
