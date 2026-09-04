"""A small JSON Schema validator for the schemas committed in `schemas/`.

The repository intentionally has no Python package dependencies: the acceptance
runner and the metadata tools must work with a bare CPython 3.13 host. This
module therefore implements only the keyword subset those schemas use.

Unsupported keywords raise `SchemaError` instead of being ignored, so a schema
cannot silently stop being enforced.
"""

import re

ANNOTATIONS = frozenset({"$schema", "$id", "title", "description", "$comment", "$defs", "examples", "default"})
SUPPORTED = frozenset({
    "$ref",
    "additionalProperties",
    "allOf",
    "const",
    "enum",
    "items",
    "minItems",
    "minLength",
    "minimum",
    "oneOf",
    "pattern",
    "properties",
    "required",
    "type",
})
TYPES = {
    "object": dict,
    "array": list,
    "string": str,
    "boolean": bool,
    "null": type(None),
}


class SchemaError(RuntimeError):
    """The schema uses a construct this validator does not implement."""


def _is_type(value, name):
    if name == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if name == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    expected = TYPES.get(name)
    if expected is None:
        raise SchemaError(f"unsupported type {name!r}")
    return isinstance(value, expected)


def _resolve(pointer, root):
    if not pointer.startswith("#/"):
        raise SchemaError(f"unsupported reference {pointer!r}")
    node = root
    for token in pointer[2:].split("/"):
        token = token.replace("~1", "/").replace("~0", "~")
        if not isinstance(node, dict) or token not in node:
            raise SchemaError(f"reference {pointer!r} does not resolve")
        node = node[token]
    return node


def _check(instance, schema, root, path, errors):
    if isinstance(schema, bool):
        if not schema:
            errors.append(f"{path}: value is not allowed here")
        return
    unknown = set(schema) - SUPPORTED - ANNOTATIONS
    if unknown:
        raise SchemaError(f"{path}: unsupported keywords {sorted(unknown)}")

    if "$ref" in schema:
        _check(instance, _resolve(schema["$ref"], root), root, path, errors)

    if "type" in schema:
        names = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(_is_type(instance, name) for name in names):
            errors.append(f"{path}: expected type {'|'.join(names)}")
            return

    if "const" in schema and instance != schema["const"]:
        errors.append(f"{path}: expected {schema['const']!r}")
    if "enum" in schema and instance not in schema["enum"]:
        errors.append(f"{path}: expected one of {schema['enum']!r}")

    if isinstance(instance, str):
        if "pattern" in schema and not re.search(schema["pattern"], instance):
            errors.append(f"{path}: {instance!r} does not match {schema['pattern']!r}")
        if "minLength" in schema and len(instance) < schema["minLength"]:
            errors.append(f"{path}: shorter than {schema['minLength']} characters")
    if isinstance(instance, (int, float)) and not isinstance(instance, bool):
        if "minimum" in schema and instance < schema["minimum"]:
            errors.append(f"{path}: less than {schema['minimum']}")

    if isinstance(instance, list):
        if "minItems" in schema and len(instance) < schema["minItems"]:
            errors.append(f"{path}: fewer than {schema['minItems']} items")
        if "items" in schema:
            for index, item in enumerate(instance):
                _check(item, schema["items"], root, f"{path}[{index}]", errors)

    if isinstance(instance, dict):
        for name in schema.get("required", []):
            if name not in instance:
                errors.append(f"{path}: missing required property {name!r}")
        properties = schema.get("properties", {})
        for name, value in instance.items():
            if name in properties:
                _check(value, properties[name], root, f"{path}.{name}", errors)
            elif "additionalProperties" in schema:
                _check(value, schema["additionalProperties"], root, f"{path}.{name}", errors)

    for branch in schema.get("allOf", []):
        _check(instance, branch, root, path, errors)
    if "oneOf" in schema:
        matches = 0
        for branch in schema["oneOf"]:
            branch_errors = []
            _check(instance, branch, root, path, branch_errors)
            matches += not branch_errors
        if matches != 1:
            errors.append(f"{path}: matched {matches} of {len(schema['oneOf'])} allowed forms, expected 1")


def errors_for(instance, schema):
    """Return every validation error for an instance, as readable strings."""
    found = []
    _check(instance, schema, schema, "$", found)
    return found


def validate(instance, schema, label="instance"):
    """Raise `ValueError` describing every validation error, if there are any."""
    found = errors_for(instance, schema)
    if found:
        raise ValueError(f"{label} does not match its schema:\n  " + "\n  ".join(found))
