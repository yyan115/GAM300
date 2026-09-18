"""Check that cooking preserves authored materials.

A clean desktop cook must leave every authored material as it was, and an
Android cook must not write anything next to the sources at all.
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

# Models whose materials have been lost in a cook before.
#
# The environment models lost their texture maps and colours in a clean cook.
#
# KusaneV6@Roll lost its metallic map. Its material file is named with '_' for
# '@', as the writer sanitises it, while the importer looked for the '@' name,
# never found it, and imported over the authored file. The FBX also stores its
# texture paths with backslashes, which Linux did not split, so the import
# found no texture and wrote the material without one.
FIXTURES = (
    (Path("Models/Environment"), "WoodenBarrier"),
    (Path("Models/Environment"), "WeaponHolder"),
    (Path("Models/Environment"), "WallTop"),
    (Path("Animations/Player"), "KusaneV6@Roll"),
)


def material_stem(model_name):
    """The name the engine gives an imported model's material files."""
    return re.sub(r"[^A-Za-z0-9 ._-]", "_", model_name)


def read_material(path):
    data = path.read_bytes()
    offset = 0

    def unpack(fmt):
        nonlocal offset
        values = struct.unpack_from("<" + fmt, data, offset)
        offset += struct.calcsize("<" + fmt)
        return values[0] if len(values) == 1 else values

    def string():
        nonlocal offset
        length = unpack("Q")
        value = data[offset:offset + length].decode("utf-8")
        offset += length
        return value

    # The desktop .mat format stores size_t lengths, 17 property floats,
    # texture type/path pairs, and optional tiling/offset values.
    name = string()
    properties = unpack("17f")
    textures = []
    for _ in range(unpack("Q")):
        texture_type = unpack("i")
        textures.append((texture_type, string()))
    return name, properties, sorted(textures), data[offset:]


def copy_fixtures(source, resources):
    """Copy the fixture models, their materials and the materials' textures.

    Returns each material's decoded contents, keyed by its path relative to
    the resource root. Models are created first to exercise discovery before
    all materials have been registered.
    """
    def copy(relative):
        target = resources / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, target)

    for folder, name in FIXTURES:
        for extension in (".fbx", ".fbx.meta"):
            copy(folder / (name + extension))

    expected = {}
    for _, name in FIXTURES:
        materials = list((source / "Materials").glob(material_stem(name) + "_*.mat"))
        assert materials, f"Missing fixture materials for {name}"
        for material in materials:
            relative = material.relative_to(source)
            expected[relative] = read_material(material)
            copy(relative)
            copy(Path(str(relative) + ".meta"))
            for _, texture in expected[relative][2]:
                relative_texture = Path(texture[texture.index("Resources/") + 10:])
                copy(relative_texture)
                copy(Path(str(relative_texture) + ".meta"))
    return expected


def cook(cooker, resources, working, android=False):
    env = os.environ.copy()
    if os.name != "nt":
        env["LD_LIBRARY_PATH"] = str(cooker.parent) + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    command = [str(cooker), "--resources", str(resources)] + (["--android"] if android else [])
    result = subprocess.run(command, cwd=working, env=env, capture_output=True,
                            text=True, errors="replace")
    print(result.stdout)
    if result.returncode:
        raise RuntimeError(f"AssetCooker exited {result.returncode}: {result.stderr}")


def check_desktop_cook(cooker, source):
    """A clean desktop cook leaves every authored material as it was."""
    with tempfile.TemporaryDirectory(prefix="kusane-material-test-") as temp:
        root = Path(temp)
        resources = root / "Resources"
        working = root / "Build" / "Release"
        working.mkdir(parents=True)

        expected = copy_fixtures(source, resources)
        # An absolute resource root, as the release build passes.
        cook(cooker, resources, working)

        for relative, original in expected.items():
            name, *authored = read_material(resources / relative)
            assert authored == list(original[1:]), \
                f"Cooking changed the authored material: {relative}"
            # The writer names a material after its file. Some committed
            # materials still carry an older name inside, which a cook
            # replaces; that is the one change a cook may make.
            assert name == relative.stem, \
                f"Cooked material {relative} is named {name!r}, not after its file"
        assert len(list(resources.rglob("*.mesh"))) == len(FIXTURES)
        assert list(resources.rglob("*.dds")), "No textures were cooked"
        print(f"Preserved {len(expected)} authored materials through a clean cook.")


def check_android_cook(cooker, source):
    """An Android cook writes only into the Android project.

    It starts where the editor is when Compile Assets for Android runs: the
    authored files as they are in the repository, with desktop outputs beside
    them. Then nothing under the resource root may be written to, not a
    source, not a .meta, not a desktop output, even with the same bytes.
    """
    with tempfile.TemporaryDirectory(prefix="kusane-android-cook-test-") as temp:
        # Laid out like the repository. The engine finds the resources at
        # ../../Resources and the Android project at ../../../AndroidProject
        # from the working directory, and this keeps both inside the
        # temporary folder.
        root = Path(temp)
        resources = root / "Project" / "Resources"
        working = root / "Project" / "Build" / "Release"
        working.mkdir(parents=True)

        copy_fixtures(source, resources)
        cook(cooker, resources, working)
        # A desktop cook may itself rewrite an authored file, which would hide
        # the Android cook doing the same. Put the authored files back.
        copy_fixtures(source, resources)

        def state(path):
            return path.stat().st_mtime_ns, path.read_bytes()

        before = {path: state(path) for path in resources.rglob("*") if path.is_file()}

        cook(cooker, resources, working, android=True)

        after = {path for path in resources.rglob("*") if path.is_file()}
        added = sorted(str(path.relative_to(resources)) for path in after - set(before))
        changed = sorted(str(path.relative_to(resources)) for path, was in before.items()
                         if not path.exists() or state(path) != was)
        assert not added and not changed, \
            f"The Android cook wrote into the resource root. Added: {added}. Changed: {changed}"
        android_meshes = list((root / "AndroidProject").rglob("*_android.mesh"))
        assert len(android_meshes) == len(FIXTURES), \
            f"Expected {len(FIXTURES)} Android meshes, found {len(android_meshes)}"
        print("An Android cook left the resource root untouched.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cooker", type=Path, required=True)
    parser.add_argument("--resources", type=Path,
                        default=Path(__file__).resolve().parents[1] / "Resources")
    args = parser.parse_args()
    cooker = args.cooker.resolve()
    source = args.resources.resolve()

    check_desktop_cook(cooker, source)
    check_android_cook(cooker, source)


if __name__ == "__main__":
    main()
