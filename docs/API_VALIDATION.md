# API Validation Schema Reference

The gateway validates every incoming HTTP request against the parameter schemas declared by backends during endpoint registration. This document describes the schema definition format, supported data types, constraint options, and error response format.

## Overview

Validation happens at **Stage 4** of the request processing pipeline, after routing and authentication. The `ValidationEngine` checks parameters in all four locations:

1. **Path parameters** -- extracted from the URL path (e.g., `{user_id}` in `/api/users/{user_id}`)
2. **Query parameters** -- parsed from the URL query string (e.g., `?page=1&limit=20`)
3. **Header parameters** -- extracted from HTTP request headers
4. **Body parameters** -- parsed from the JSON request body

Validation is exhaustive: all parameters are checked and all errors are collected before returning a response. This allows clients to fix all issues in a single round-trip.

## Schema Definition Format

Each parameter is defined by a `ParameterSchema` object in the endpoint's `parameters` array:

```json
{
    "name": "<string>",
    "type": "<string>",
    "location": "<string>",
    "required": "<boolean>",
    "description": "<string>",
    "default": "<string>",
    "constraints": {
        "min_value": "<integer>",
        "max_value": "<integer>",
        "min_length": "<integer>",
        "max_length": "<integer>",
        "pattern": "<string>",
        "allowed_values": ["<string>", "..."]
    }
}
```

### Field Reference

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | string | Yes | The parameter name. Must match the path placeholder, query key, header name, or JSON body field name. |
| `type` | string | Yes | Data type. One of: `string`, `integer`, `float`, `boolean`, `object`, `array`. |
| `location` | string | Yes | Where the parameter is found. One of: `path`, `query`, `header`, `body`. |
| `required` | boolean | No | Whether the parameter must be present. Default: `false`. |
| `description` | string | No | Human-readable description (for documentation only, not used in validation). |
| `default` | string | No | Default value if the parameter is absent (for documentation only). |
| `constraints` | object | No | Validation constraints (see below). |

## Supported Data Types

### `string`

String parameters are validated as-is from the source location.

**Applicable constraints:**

| Constraint | Type | Description |
|---|---|---|
| `min_length` | integer | Minimum string length (inclusive). |
| `max_length` | integer | Maximum string length (inclusive). |
| `pattern` | string | ECMAScript regex pattern. The entire string must match (`regex_match`, not `regex_search`). |
| `allowed_values` | array of strings | Enumeration of permitted values. The value must exactly match one entry. |

**Example:**

```json
{
    "name": "username",
    "type": "string",
    "location": "body",
    "required": true,
    "constraints": {
        "min_length": 3,
        "max_length": 32,
        "pattern": "^[a-zA-Z0-9_]+$"
    }
}
```

### `integer`

Integer parameters are parsed as 64-bit signed integers (`int64`). The string representation must be a valid decimal integer with no trailing characters.

**Applicable constraints:**

| Constraint | Type | Description |
|---|---|---|
| `min_value` | integer | Minimum value (inclusive). |
| `max_value` | integer | Maximum value (inclusive). |

**Example:**

```json
{
    "name": "page",
    "type": "integer",
    "location": "query",
    "required": false,
    "constraints": {
        "min_value": 1,
        "max_value": 10000
    }
}
```

**Validation errors:**
- Non-numeric strings: `"Value is not a valid integer"`
- Below minimum: `"Integer value -1 is below minimum 0"`
- Above maximum: `"Integer value 200 exceeds maximum 100"`

### `float`

Float parameters are parsed as 64-bit double-precision floating-point numbers. `Inf` and `NaN` are rejected.

**Applicable constraints:**

| Constraint | Type | Description |
|---|---|---|
| `min_value` | number | Minimum value (inclusive). Compared as `double`. |
| `max_value` | number | Maximum value (inclusive). Compared as `double`. |

**Example:**

```json
{
    "name": "price",
    "type": "float",
    "location": "body",
    "required": true,
    "constraints": {
        "min_value": 0.01,
        "max_value": 999999.99
    }
}
```

**Validation errors:**
- Non-numeric strings: `"Value is not a valid floating-point number"`
- Below minimum: `"Float value is below minimum"`
- Above maximum: `"Float value exceeds maximum"`

### `boolean`

Boolean parameters accept the following string representations:

| Accepted values | Interpretation |
|---|---|
| `true`, `1`, `yes` | True |
| `false`, `0`, `no` | False |

**No additional constraints** are available for booleans.

**Example:**

```json
{
    "name": "verbose",
    "type": "boolean",
    "location": "query",
    "required": false
}
```

**Validation error:**
- Invalid value: `"Value is not a valid boolean (expected true/false, 1/0, or yes/no)"`

### `object`

Object parameters are validated only in the `body` location. The value must be a JSON object (`{}`). No deep schema validation is performed on the object's contents.

**Example:**

```json
{
    "name": "metadata",
    "type": "object",
    "location": "body",
    "required": false
}
```

**Validation error:**
- Wrong type: `"Field type mismatch"`

### `array`

Array parameters are validated only in the `body` location. The value must be a JSON array (`[]`). No validation is performed on array element types.

**Example:**

```json
{
    "name": "tags",
    "type": "array",
    "location": "body",
    "required": false
}
```

**Validation error:**
- Wrong type: `"Field type mismatch"`

## Validation by Location

### Path Parameters

Path parameters are extracted from the URL by matching against the endpoint's path pattern. For example, given the endpoint `/api/users/{user_id}/posts/{post_id}`, a request to `/api/users/abc123/posts/42` produces:

```json
{"user_id": "abc123", "post_id": "42"}
```

Path parameters are always strings at the extraction level. Type validation (e.g., checking that `post_id` is a valid integer) is performed by the constraint engine.

### Query Parameters

Query parameters are extracted from the URL query string. The URL `?page=1&limit=20&sort=name` produces:

```json
{"page": "1", "limit": "20", "sort": "name"}
```

Values are URL-decoded before validation.

### Header Parameters

Header parameters are matched by header name. Header names are normalized to lowercase before lookup. For example, a schema with `"name": "X-Request-ID"` will match the header `x-request-id`.

### Body Parameters

Body parameters are extracted from the JSON request body. The body must be a valid JSON object. Each schema entry names a top-level field in the object:

```json
// Request body
{
    "username": "johndoe",
    "email": "john@example.com",
    "age": 30
}
```

For body parameters, type checking is strict:
- A `string` schema requires the JSON value to be a string (not a number or boolean).
- An `integer` schema requires the JSON value to be an integer.
- A `float` schema accepts both integers and floating-point numbers.
- A `boolean` schema requires the JSON value to be `true` or `false`.
- An `object` schema requires the JSON value to be an object.
- An `array` schema requires the JSON value to be an array.

If the body is required but the request has no body, a `"Required body parameter is missing (no body provided)"` error is generated.

If the body is present but not valid JSON, a `"Request body is not valid JSON: <parser error>"` error is generated.

## Constraints Reference

### Complete Constraint Object

```json
{
    "min_value": 0,
    "max_value": 100,
    "min_length": 1,
    "max_length": 255,
    "pattern": "^[a-zA-Z]+$",
    "allowed_values": ["active", "inactive", "pending"]
}
```

All constraint fields are optional. Only applicable constraints are evaluated (e.g., `min_length` is ignored for `integer` parameters).

### Constraint Applicability Matrix

| Constraint | `string` | `integer` | `float` | `boolean` | `object` | `array` |
|---|---|---|---|---|---|---|
| `min_value` | -- | Yes | Yes | -- | -- | -- |
| `max_value` | -- | Yes | Yes | -- | -- | -- |
| `min_length` | Yes | -- | -- | -- | -- | -- |
| `max_length` | Yes | -- | -- | -- | -- | -- |
| `pattern` | Yes | -- | -- | -- | -- | -- |
| `allowed_values` | Yes | -- | -- | -- | -- | -- |

### Pattern Matching

Patterns use ECMAScript regex syntax (the C++ `std::regex` default). The pattern is matched against the entire string using `std::regex_match()`, meaning the pattern must describe the full value, not a substring.

**Examples:**

| Pattern | Matches | Does not match |
|---|---|---|
| `^[a-zA-Z0-9]+$` | `abc123` | `abc 123` (space) |
| `^[a-f0-9]{24}$` | `507f1f77bcf86cd799439011` | `507f` (too short) |
| `^[^@]+@[^@]+\.[^@]+$` | `user@example.com` | `user@` (no domain) |
| `^\d{4}-\d{2}-\d{2}$` | `2025-01-15` | `2025-1-15` (single digit) |

If the regex pattern itself is invalid, the validation fails with `"Invalid regex pattern in schema"`.

## Error Response Format

When one or more validation errors occur, the gateway returns HTTP `400 Bad Request` with a JSON body:

```json
{
    "errors": [
        {
            "parameter": "<string: parameter name>",
            "message": "<string: human-readable error description>",
            "provided_value": "<string: optional, the value that was provided>"
        }
    ]
}
```

The `provided_value` field is included only when the parameter was present but invalid. It is omitted when the parameter is missing.

### Error Messages

| Condition | Error Message |
|---|---|
| Required path param missing | `Required path parameter is missing` |
| Required query param missing | `Required query parameter is missing` |
| Required header param missing | `Required header parameter is missing` |
| Required body field missing | `Required body field is missing` |
| No body provided | `Required body parameter is missing (no body provided)` |
| Body not JSON | `Request body is not valid JSON: <parser error>` |
| Body not an object | `Expected JSON object body` |
| Type mismatch (body) | `Field type mismatch` |
| String too short | `String length N is below minimum M` |
| String too long | `String length N exceeds maximum M` |
| Pattern mismatch | `String does not match required pattern` |
| Invalid regex | `Invalid regex pattern in schema` |
| Not in allowed values | `Value is not in the list of allowed values` |
| Not a valid integer | `Value is not a valid integer` |
| Integer below min | `Integer value N is below minimum M` |
| Integer above max | `Integer value N exceeds maximum M` |
| Not a valid float | `Value is not a valid floating-point number` |
| Float below min | `Float value is below minimum` |
| Float above max | `Float value exceeds maximum` |
| Not a valid boolean | `Value is not a valid boolean (expected true/false, 1/0, or yes/no)` |

## Complete Example

### Endpoint Registration

```json
{
    "path": "/api/products",
    "method": "POST",
    "description": "Create a new product",
    "requires_auth": true,
    "parameters": [
        {
            "name": "name",
            "type": "string",
            "location": "body",
            "required": true,
            "constraints": {
                "min_length": 1,
                "max_length": 200
            }
        },
        {
            "name": "sku",
            "type": "string",
            "location": "body",
            "required": true,
            "constraints": {
                "pattern": "^[A-Z]{2,4}-[0-9]{4,8}$"
            }
        },
        {
            "name": "price",
            "type": "float",
            "location": "body",
            "required": true,
            "constraints": {
                "min_value": 0.01,
                "max_value": 999999.99
            }
        },
        {
            "name": "category",
            "type": "string",
            "location": "body",
            "required": true,
            "constraints": {
                "allowed_values": ["electronics", "clothing", "food", "furniture"]
            }
        },
        {
            "name": "quantity",
            "type": "integer",
            "location": "body",
            "required": false,
            "default": "0",
            "constraints": {
                "min_value": 0,
                "max_value": 1000000
            }
        },
        {
            "name": "tags",
            "type": "array",
            "location": "body",
            "required": false
        },
        {
            "name": "attributes",
            "type": "object",
            "location": "body",
            "required": false
        }
    ]
}
```

### Valid Request

```http
POST /api/products HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json

{
    "name": "Wireless Mouse",
    "sku": "ELEC-12345",
    "price": 29.99,
    "category": "electronics",
    "quantity": 500,
    "tags": ["wireless", "mouse", "peripheral"],
    "attributes": {
        "color": "black",
        "weight_grams": 85
    }
}
```

### Invalid Request

```http
POST /api/products HTTP/1.1
Authorization: Bearer <token>
Content-Type: application/json

{
    "name": "",
    "sku": "invalid-sku",
    "price": -5.00,
    "category": "toys"
}
```

### Error Response

```http
HTTP/1.1 400 Bad Request
Content-Type: application/json

{
    "errors": [
        {
            "parameter": "name",
            "message": "String length 0 is below minimum 1",
            "provided_value": ""
        },
        {
            "parameter": "sku",
            "message": "String does not match required pattern",
            "provided_value": "invalid-sku"
        },
        {
            "parameter": "price",
            "message": "Float value is below minimum",
            "provided_value": "-5.000000"
        },
        {
            "parameter": "category",
            "message": "Value is not in the list of allowed values",
            "provided_value": "toys"
        }
    ]
}
```
