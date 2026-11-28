# Converts compiler-local SPIR-V type IDs into a stable structural layout contract.
# Debug names never affect compatibility; offsets, types, strides and bindings do.
import hashlib
import json


def canonical_bytes(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True) + "\n").encode("ascii")


def normalize(reflection, *, stage, entrypoint, counts):
    if not isinstance(reflection, dict) or not isinstance(reflection.get("types", {}), dict):
        raise ValueError("Invalid structural reflection object")
    entries = reflection.get("entryPoints")
    mode = {"vertex": "vert", "fragment": "frag"}[stage]
    if entries != [{"name": entrypoint, "mode": mode}]:
        raise ValueError("Unexpected reflected shader entry point")
    types = reflection.get("types", {})

    def visit(value, chain=()):
        if isinstance(value, list):
            # Member/array order is structural, never sorted here.
            return [visit(item, chain) for item in value]
        if not isinstance(value, dict):
            return value
        result = {}
        for key, item in value.items():
            if key == "name":
                continue
            if key == "type" and not isinstance(item, str):
                raise ValueError("Invalid reflected type")
            if key == "type" and item.startswith("_") and item not in types:
                raise ValueError("Unresolved reflected type")
            if key == "type" and item in types:
                if item in chain:
                    raise ValueError("Recursive reflected type")
                result[key] = visit(types[item], (*chain, item))
            else:
                result[key] = visit(item, chain)
        return result

    result = {"version": 1, "stage": stage, "entrypoint": entrypoint, "counts": counts}
    for key, value in reflection.items():
        if key in {"entryPoints", "types"}:
            continue
        normalized = visit(value)
        # Resource/interface declaration order is irrelevant; member order is not.
        result[key] = sorted(normalized, key=canonical_bytes) if isinstance(normalized, list) else normalized
    return result


def digest(value):
    return hashlib.sha256(canonical_bytes(value)).hexdigest()
