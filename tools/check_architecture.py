# Checks project includes against the approved module graph.
# Public headers must remain free of SDL and logging implementation types.
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve()
allowed = {
    "core": set(),
    "platform": {"core"},
    "gpu": {"core", "platform"},
    "platform_imgui": {"core", "platform"},
    "gpu_imgui": {"core", "gpu"},
    "renderer": {"core", "gpu"},
    "assets": {"core"},
    "scene": {"core", "assets"},
    "input": {"core"},
    "debug_ui": {"core", "scene", "input", "platform_imgui", "gpu_imgui"},
    "runtime": {"core", "platform", "gpu", "renderer", "assets", "scene", "input", "debug_ui"},
}
errors = []
for module, dependencies in allowed.items():
    integration = module in {"platform_imgui", "gpu_imgui"}
    source_root = root / "engine" / (module.removesuffix("_imgui") if integration else module)
    if integration:
        source_root /= "integration"
    for path in source_root.rglob("*"):
        if path.suffix not in {".hpp", ".cpp", ".h", ".c"}:
            continue
        if module in {"platform", "gpu"} and path.relative_to(source_root).parts[0] == "integration":
            continue
        text = path.read_text(encoding="utf-8")
        public = "include" in path.relative_to(source_root).parts
        if public:
            # Remove comments before checking storage declarations. Function declarations
            # are permitted; mutable static/inline objects belong in an implementation unit.
            declarations = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
            for storage in re.finditer(r"\b(?:static|inline)\s+([^;{}]+)([;{}])", declarations):
                declaration = storage.group(1)
                head = declaration.split("=", 1)[0]
                function = "(" in head and not re.search(r"\(\s*[*&]", head)
                immutable = re.search(r"\bconstexpr\b", head) or (
                    re.search(r"\bconst\b", head) and "*" not in head and "&" not in head)
                if not function and not immutable:
                    errors.append(f"{path}: mutable static/inline state in public header")
        for dependency in re.findall(r"#include\s*[<\"]somnium/([^/]+)/", text):
            if dependency != module and dependency not in dependencies:
                errors.append(f"{path}: forbidden {module} -> {dependency}")
        if module == "runtime" and public and re.search(r"#include.*somnium/renderer/|\brenderer::", text):
            errors.append(f"{path}: renderer leaked into gameplay contract")
        if public and re.search(r"\bSDL_\w+|#include\s*[<\"](?:SDL3|spdlog)/", text):
            errors.append(f"{path}: third-party type in public interface")
        if module not in {"gpu", "gpu_imgui"} and re.search(r"#include\s*[<\"]SDL3/(?:SDL|SDL_gpu)\.h", text):
            errors.append(f"{path}: GPU/umbrella SDL header outside gpu")
        if module in {"platform", "gpu"} and re.search(r"#include.*imgui|#if.*(?:DEBUG_UI|IMGUI)", text, re.I):
            errors.append(f"{path}: optional UI leaked into base implementation")
        if module != "core" and "#include <spdlog/" in text:
            errors.append(f"{path}: logging implementation leaked")
        if module == "input" and re.search(r"#include\s*[<\"]SDL|\bSDL_\w+", text):
            errors.append(f"{path}: SDL leaked into CPU input mapping")
        if module not in {"platform", "platform_imgui"} and re.search(r"\bSDL_Event\b", text):
            errors.append(f"{path}: native event escaped its boundary")
        if re.search(r"\b(?:int|auto)\s+main\s*\(", text) and "tests" not in path.parts:
            errors.append(f"{path}: engine must not define main")
for path in (root / "game").rglob("*"):
    if path.suffix in {".hpp", ".cpp"}:
        text = path.read_text(encoding="utf-8")
        if re.search(r"#include.*somnium/(?:renderer|gpu)/|\b(?:renderer|gpu)::", text):
            errors.append(f"{path}: game depends on rendering implementation")
if errors:
    sys.exit("\n".join(errors))
print("Architecture include boundaries: OK")
