"""Rejects injected engine objects and archives in the generated link contract.
The production check consumes CMake's actual codemodel and the linked module binary.
"""
import copy
import unittest
from check_game_module import validate


class GameModuleTests(unittest.TestCase):
    good = {"type": "MODULE_LIBRARY", "sources": [{"path": "game/src/module_entry.cpp"}],
            "link": {"commandFragments": [{"fragment": "bin/somnium_engine_shared.lib"}]}}

    def test_import_is_accepted(self):
        validate(self.good)

    def test_engine_sources_and_objects_are_rejected(self):
        for path in ["engine/core/src/log.cpp", "C:\\repo\\engine\\runtime\\engine.cpp.obj"]:
            target = copy.deepcopy(self.good)
            target["sources"].append({"path": path})
            with self.assertRaisesRegex(ValueError, "engine"):
                validate(target)

    def test_engine_archives_and_link_objects_are_rejected(self):
        for path in ["engine/core/somnium_core.lib", "libsomnium_runtime.a",
                     "engine/scene/CMakeFiles/somnium_scene.dir/world.cpp.o", "/WHOLEARCHIVE:x.lib"]:
            target = copy.deepcopy(self.good)
            target["link"]["commandFragments"].append({"fragment": path})
            with self.assertRaises(ValueError):
                validate(target)

    def test_import_cannot_be_missing(self):
        target = copy.deepcopy(self.good)
        target["link"]["commandFragments"] = []
        with self.assertRaises(ValueError):
            validate(target)


if __name__ == "__main__":
    unittest.main()
