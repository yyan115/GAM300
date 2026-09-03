#!/usr/bin/env python3
"""Remove Android asset data that the runtime never consumes."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


KTX1_IDENTIFIER = b"\xabKTX 11\xbb\r\n\x1a\n"
KTX1_HEADER_SIZE = 64
KTX1_FIELDS = struct.Struct("<13I")
MATERIAL_SIZE = struct.Struct("<Q")
MATERIAL_TEXTURE_TYPE = struct.Struct("<I")
MATERIAL_FIXED_PROPERTY_BYTES = 68
PACKED_ORM_TEXTURE_TYPE = 17
PBR_TEXTURE_TYPES = (3, 16, 15)  # AO, roughness, metallic -> RGB
GL_COMPRESSED_RGB8_ETC2 = 0x9274
GL_COMPRESSED_RGBA8_ETC2_EAC = 0x9278
GL_RGB = 0x1907
GL_RGBA = 0x1908
PACKED_ORM_MAX_CHANNEL_MAE = 8.0
PACKED_ORM_MAX_CHANNEL_P95 = 40


class PackedORMQualityError(RuntimeError):
    pass


def load_texture_metadata(
    asset_root: Path, source_resources: Path
) -> dict[str, tuple[str, Path]]:
    texture_metadata: dict[str, tuple[str, Path]] = {}
    resources = asset_root / "Resources"
    for meta_path in resources.rglob("*.meta"):
        try:
            document = json.loads(meta_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            continue

        android_path = document.get("AssetMetaData", {}).get("android_compiled")
        texture_type = document.get("TextureMetaData", {}).get("type")
        if isinstance(android_path, str) and android_path.endswith(".ktx"):
            source_path = source_resources / meta_path.relative_to(resources).with_suffix("")
            texture_metadata[android_path.replace("\\", "/")] = (
                str(texture_type or ""),
                source_path,
            )
    return texture_metadata


def mip_records(data: bytes, level_count: int, key_value_bytes: int, faces: int) -> list[tuple[int, int]]:
    records: list[tuple[int, int]] = []
    offset = KTX1_HEADER_SIZE + key_value_bytes
    for _ in range(level_count):
        start = offset
        if offset + 4 > len(data):
            raise ValueError("truncated mip-size field")
        image_size = struct.unpack_from("<I", data, offset)[0]
        offset += 4
        for _ in range(faces):
            offset += (image_size + 3) & ~3
        if offset > len(data):
            raise ValueError("truncated mip payload")
        records.append((start, offset))
    return records


def optimized_ktx(data: bytes, max_dimension: int) -> tuple[bytes, int]:
    if len(data) < KTX1_HEADER_SIZE or data[:12] != KTX1_IDENTIFIER:
        raise ValueError("not a KTX1 texture")

    fields = list(KTX1_FIELDS.unpack_from(data, 12))
    endianness = fields[0]
    width, height, depth = fields[6:9]
    array_elements, faces, level_count, key_value_bytes = fields[9:13]
    if endianness != 0x04030201:
        raise ValueError("big-endian KTX1 is unsupported")
    if depth or array_elements or faces != 1:
        raise ValueError("only ordinary 2D textures are supported")
    if level_count == 0:
        level_count = 1

    records = mip_records(data, level_count, key_value_bytes, faces)
    first_level = 0
    optimized_width = width
    optimized_height = height
    while first_level + 1 < level_count and (
        optimized_width > max_dimension or optimized_height > max_dimension
    ):
        first_level += 1
        optimized_width = max(1, optimized_width // 2)
        optimized_height = max(1, optimized_height // 2)

    if first_level == 0:
        return data, 0

    fields[6] = optimized_width
    fields[7] = optimized_height
    fields[11] = level_count - first_level
    metadata_end = KTX1_HEADER_SIZE + key_value_bytes
    output = bytearray(data[:metadata_end])
    KTX1_FIELDS.pack_into(output, 12, *fields)
    output.extend(data[records[first_level][0] :])
    return bytes(output), first_level


def source_has_nonopaque_alpha(source_path: Path) -> bool:
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is required to inspect source alpha") from error

    with Image.open(source_path) as source:
        if "A" not in source.getbands() and "transparency" not in source.info:
            return False
        alpha = source.convert("RGBA").getchannel("A")
        minimum, _ = alpha.getextrema()
        return minimum != 255


def strip_opaque_etc2_alpha(data: bytes) -> tuple[bytes, bool]:
    """Convert ETC2 RGBA to RGB by retaining each block's encoded RGB half."""

    if len(data) < KTX1_HEADER_SIZE or data[:12] != KTX1_IDENTIFIER:
        raise ValueError("not a KTX1 texture")

    fields = list(KTX1_FIELDS.unpack_from(data, 12))
    endianness = fields[0]
    internal_format = fields[4]
    base_internal_format = fields[5]
    width, height, depth = fields[6:9]
    array_elements, faces, level_count, key_value_bytes = fields[9:13]
    if internal_format != GL_COMPRESSED_RGBA8_ETC2_EAC:
        return data, False
    if endianness != 0x04030201:
        raise ValueError("big-endian KTX1 is unsupported")
    if base_internal_format not in (0, GL_RGBA):
        raise ValueError("unexpected ETC2 RGBA base format")
    if depth or array_elements or faces != 1:
        raise ValueError("only ordinary 2D textures are supported")
    if level_count == 0:
        level_count = 1

    metadata_end = KTX1_HEADER_SIZE + key_value_bytes
    if metadata_end > len(data):
        raise ValueError("truncated KTX metadata")
    fields[4] = GL_COMPRESSED_RGB8_ETC2
    fields[5] = 0 if base_internal_format == 0 else GL_RGB
    output = bytearray(data[:metadata_end])
    KTX1_FIELDS.pack_into(output, 12, *fields)

    offset = metadata_end
    level_width = width
    level_height = height
    for _ in range(level_count):
        if offset + 4 > len(data):
            raise ValueError("truncated mip-size field")
        image_size = struct.unpack_from("<I", data, offset)[0]
        offset += 4
        block_count = ((level_width + 3) // 4) * ((level_height + 3) // 4)
        expected_size = block_count * 16
        if image_size != expected_size or offset + image_size > len(data):
            raise ValueError("unexpected ETC2 RGBA mip size")

        payload = data[offset : offset + image_size]
        rgb_payload = b"".join(
            payload[block_offset + 8 : block_offset + 16]
            for block_offset in range(0, image_size, 16)
        )
        output.extend(struct.pack("<I", len(rgb_payload)))
        output.extend(rgb_payload)
        output.extend(b"\0" * ((-len(rgb_payload)) & 3))

        offset += (image_size + 3) & ~3
        level_width = max(1, level_width // 2)
        level_height = max(1, level_height // 2)

    if offset != len(data):
        raise ValueError("unexpected trailing KTX data")
    return bytes(output), True


def replace_file(path: Path, data: bytes) -> None:
    mode = path.stat().st_mode
    descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as output:
            output.write(data)
        os.chmod(temporary_name, mode)
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def material_texture_records(
    data: bytes,
) -> tuple[int, int, list[tuple[int, bytes]]]:
    """Return the texture-count offset, records end, and raw records."""

    if len(data) < MATERIAL_SIZE.size:
        raise ValueError("truncated material name length")
    name_length = MATERIAL_SIZE.unpack_from(data, 0)[0]
    offset = MATERIAL_SIZE.size + name_length + MATERIAL_FIXED_PROPERTY_BYTES
    if offset + MATERIAL_SIZE.size > len(data):
        raise ValueError("truncated material properties")

    count_offset = offset
    texture_count = MATERIAL_SIZE.unpack_from(data, offset)[0]
    offset += MATERIAL_SIZE.size
    records: list[tuple[int, bytes]] = []
    for _ in range(texture_count):
        if offset + MATERIAL_TEXTURE_TYPE.size + MATERIAL_SIZE.size > len(data):
            raise ValueError("truncated material texture record")
        texture_type = MATERIAL_TEXTURE_TYPE.unpack_from(data, offset)[0]
        offset += MATERIAL_TEXTURE_TYPE.size
        path_length = MATERIAL_SIZE.unpack_from(data, offset)[0]
        offset += MATERIAL_SIZE.size
        if offset + path_length > len(data):
            raise ValueError("truncated material texture path")
        records.append((texture_type, data[offset : offset + path_length]))
        offset += path_length
    return count_offset, offset, records


def decoded_texture_path(raw_path: bytes) -> str:
    return raw_path.decode("utf-8").rstrip("\0").replace("\\", "/")


def source_texture_path(texture_path: str, source_resources: Path) -> Path:
    marker = "Resources/"
    marker_position = texture_path.find(marker)
    if marker_position == -1:
        raise ValueError(f"texture path has no Resources prefix: {texture_path}")
    return source_resources / texture_path[marker_position + len(marker) :]


def texture_compile_settings(source_path: Path) -> tuple[bool, str]:
    meta_path = Path(str(source_path) + ".meta")
    document = json.loads(meta_path.read_text(encoding="utf-8"))
    texture_meta = document.get("TextureMetaData", {})
    return (
        bool(texture_meta.get("flipUVs", False)),
        str(texture_meta.get("textureWrapMode", "Clamp")),
    )


def collect_packed_orm_jobs(
    asset_root: Path,
    source_resources: Path,
) -> tuple[
    dict[str, dict[int, Path]],
    list[tuple[Path, str]],
    list[str],
]:
    jobs: dict[str, dict[int, Path]] = {}
    material_updates: list[tuple[Path, str]] = []
    skipped: list[str] = []
    materials_root = asset_root / "Resources" / "Materials"

    for material_path in materials_root.rglob("*_android.mat"):
        try:
            _, _, records = material_texture_records(material_path.read_bytes())
            source_records = {
                texture_type: decoded_texture_path(path)
                for texture_type, path in records
                if texture_type in PBR_TEXTURE_TYPES
            }
            if len(source_records) < 2:
                continue

            source_paths = {
                texture_type: source_texture_path(path, source_resources)
                for texture_type, path in source_records.items()
            }
            if any(not path.is_file() for path in source_paths.values()):
                missing = ", ".join(
                    str(path) for path in source_paths.values() if not path.is_file()
                )
                skipped.append(f"{material_path}: missing {missing}")
                continue

            settings = {texture_compile_settings(path) for path in source_paths.values()}
            if len(settings) != 1:
                skipped.append(f"{material_path}: incompatible texture metadata")
                continue

            # Hash canonical source identities, not absolute checkout paths.
            identity = "\0".join(
                source_records.get(texture_type, "")
                for texture_type in PBR_TEXTURE_TYPES
            )
            digest = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:24]
            relative_path = f"Resources/Textures/PackedORM/{digest}_android.ktx"
            jobs.setdefault(relative_path, source_paths)
            material_updates.append((material_path, relative_path))
        except (OSError, UnicodeDecodeError, ValueError, json.JSONDecodeError) as error:
            skipped.append(f"{material_path}: {error}")

    return jobs, material_updates, skipped


def next_box_mip(image):
    from PIL import Image

    width, height = image.size
    return image.resize(
        (max(1, width // 2), max(1, height // 2)),
        Image.Resampling.BOX,
    )


def encode_etc1_level(image, etc1tool: str, temporary_directory: Path) -> bytes:
    png_path = temporary_directory / "packed_orm.png"
    raw_path = temporary_directory / "packed_orm.raw"
    image.save(png_path, format="PNG")
    result = subprocess.run(
        [etc1tool, str(png_path), "--encodeNoHeader", "-o", str(raw_path)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"etc1tool failed: {result.stdout.strip()}")

    data = raw_path.read_bytes()
    width, height = image.size
    expected_size = ((width + 3) // 4) * ((height + 3) // 4) * 8
    if len(data) != expected_size:
        raise RuntimeError(
            f"unexpected ETC payload size {len(data)} (expected {expected_size})"
        )
    return data


def validate_packed_orm_quality(
    expected,
    encoded: bytes,
    authored_types: set[int],
    etc1tool: str,
    temporary_directory: Path,
) -> None:
    from PIL import Image, ImageChops

    width, height = expected.size
    pkm_path = temporary_directory / "packed_orm.pkm"
    decoded_path = temporary_directory / "packed_orm_decoded.png"
    pkm_path.write_bytes(
        b"PKM 10"
        + struct.pack(
            ">5H",
            0,
            (width + 3) & ~3,
            (height + 3) & ~3,
            width,
            height,
        )
        + encoded
    )
    result = subprocess.run(
        [etc1tool, str(pkm_path), "--decode", "-o", str(decoded_path)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"etc1tool validation failed: {result.stdout.strip()}")

    with Image.open(decoded_path) as decoded_source:
        decoded = decoded_source.convert("RGB")
        decoded.load()

    channel_for_type = {3: "R", 16: "G", 15: "B"}
    pixel_count = width * height
    percentile_target = (pixel_count * 95 + 99) // 100
    failures = []
    for texture_type in authored_types:
        channel = channel_for_type[texture_type]
        difference = ImageChops.difference(
            expected.getchannel(channel), decoded.getchannel(channel)
        )
        histogram = difference.histogram()
        mae = sum(value * count for value, count in enumerate(histogram)) / pixel_count
        cumulative = 0
        p95 = 255
        for value, count in enumerate(histogram):
            cumulative += count
            if cumulative >= percentile_target:
                p95 = value
                break
        if mae > PACKED_ORM_MAX_CHANNEL_MAE or p95 > PACKED_ORM_MAX_CHANNEL_P95:
            failures.append(f"{channel}: MAE={mae:.2f}, P95={p95}")

    if failures:
        raise PackedORMQualityError(", ".join(failures))


def build_packed_orm_ktx(
    source_paths: dict[int, Path],
    max_dimension: int,
    etc1tool: str,
) -> bytes:
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is required to build packed ORM textures") from error

    channels = {}
    dimensions = set()
    flip_settings = set()
    for texture_type, source_path in source_paths.items():
        with Image.open(source_path) as source:
            channel = source.convert("RGB").getchannel("R")
            channel.load()
        flip_uvs, _ = texture_compile_settings(source_path)
        if flip_uvs:
            channel = channel.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        channels[texture_type] = channel
        dimensions.add(channel.size)
        flip_settings.add(flip_uvs)

    if len(dimensions) != 1 or len(flip_settings) != 1:
        raise RuntimeError("packed ORM sources must have matching dimensions/settings")

    width, height = next(iter(dimensions))
    while width > max_dimension or height > max_dimension:
        channels = {
            texture_type: next_box_mip(channel)
            for texture_type, channel in channels.items()
        }
        width, height = next(iter(channels.values())).size

    default_ao = Image.new("L", (width, height), 255)
    default_roughness = Image.new("L", (width, height), 255)
    default_metallic = Image.new("L", (width, height), 0)
    image = Image.merge(
        "RGB",
        (
            channels.get(3, default_ao),
            channels.get(16, default_roughness),
            channels.get(15, default_metallic),
        ),
    )

    levels = []
    current = image
    with tempfile.TemporaryDirectory(prefix="gam300-orm-") as temporary_name:
        temporary_directory = Path(temporary_name)
        while True:
            encoded = encode_etc1_level(current, etc1tool, temporary_directory)
            if not levels:
                validate_packed_orm_quality(
                    current,
                    encoded,
                    set(source_paths),
                    etc1tool,
                    temporary_directory,
                )
            levels.append(encoded)
            if current.size == (1, 1):
                break
            current = next_box_mip(current)

    fields = (
        0x04030201,
        0,
        1,
        0,
        GL_COMPRESSED_RGB8_ETC2,
        0,
        width,
        height,
        0,
        0,
        1,
        len(levels),
        0,
    )
    output = bytearray(KTX1_IDENTIFIER)
    output.extend(KTX1_FIELDS.pack(*fields))
    for level in levels:
        output.extend(struct.pack("<I", len(level)))
        output.extend(level)
        output.extend(b"\0" * ((-len(level)) & 3))
    return bytes(output)


def set_packed_orm_record(material_path: Path, packed_path: str | None) -> None:
    data = material_path.read_bytes()
    count_offset, records_end, records = material_texture_records(data)
    records = [
        record for record in records if record[0] != PACKED_ORM_TEXTURE_TYPE
    ]
    if packed_path is not None:
        records.append((PACKED_ORM_TEXTURE_TYPE, packed_path.encode("utf-8")))

    output = bytearray(data[:count_offset])
    output.extend(MATERIAL_SIZE.pack(len(records)))
    for texture_type, path in records:
        output.extend(MATERIAL_TEXTURE_TYPE.pack(texture_type))
        output.extend(MATERIAL_SIZE.pack(len(path)))
        output.extend(path)
    output.extend(data[records_end:])

    # Parse the rebuilt file before replacing the working asset.
    _, verified_end, verified_records = material_texture_records(output)
    has_packed_record = any(
        texture_type == PACKED_ORM_TEXTURE_TYPE for texture_type, _ in verified_records
    )
    if verified_end > len(output) or has_packed_record != (packed_path is not None):
        raise RuntimeError(f"packed ORM material validation failed: {material_path}")
    replace_file(material_path, bytes(output))


def write_asset_manifest(asset_root: Path) -> None:
    manifest_path = asset_root / "asset_manifest.txt"
    paths = sorted(
        path.relative_to(asset_root).as_posix()
        for path in asset_root.rglob("*")
        if path.is_file() and path != manifest_path
    )
    contents = "".join(f"{path}\n" for path in paths).encode("utf-8")
    replace_file(manifest_path, contents)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--asset-root",
        type=Path,
        default=Path("AndroidProject/app/src/main/assets"),
        help="Android assets directory",
    )
    parser.add_argument("--max-dimension", type=int, default=1024)
    parser.add_argument(
        "--source-resources",
        type=Path,
        default=Path("Project/Resources"),
        help="desktop source Resources directory used to build packed ORM maps",
    )
    parser.add_argument(
        "--etc1tool",
        default=shutil.which("etc1tool"),
        help="Android SDK etc1tool executable",
    )
    parser.add_argument("--apply", action="store_true", help="rewrite eligible KTX files")
    args = parser.parse_args()

    if args.max_dimension < 1:
        parser.error("--max-dimension must be positive")
    asset_root = args.asset_root.resolve()
    resources = asset_root / "Resources"
    if not resources.is_dir():
        parser.error(f"missing Resources directory: {resources}")
    source_resources = args.source_resources.resolve()
    if not source_resources.is_dir():
        parser.error(f"missing source Resources directory: {source_resources}")

    texture_metadata = load_texture_metadata(asset_root, source_resources)
    changed_files = 0
    removed_bytes = 0
    stripped_alpha_files = 0
    stripped_alpha_bytes = 0
    skipped_sprites = 0
    for texture_path in resources.rglob("*.ktx"):
        relative_path = texture_path.relative_to(asset_root).as_posix()
        if relative_path.startswith("Resources/Textures/PackedORM/"):
            continue
        metadata = texture_metadata.get(relative_path)
        if metadata is None:
            raise RuntimeError(f"missing texture metadata for {relative_path}")
        texture_type, source_path = metadata
        if texture_type == "sprite":
            skipped_sprites += 1
            continue

        original = texture_path.read_bytes()
        optimized, _ = optimized_ktx(original, args.max_dimension)
        stripped_alpha = False
        alpha_input_size = len(optimized)
        internal_format = KTX1_FIELDS.unpack_from(optimized, 12)[4]
        if (
            internal_format == GL_COMPRESSED_RGBA8_ETC2_EAC
            and not source_has_nonopaque_alpha(source_path)
        ):
            optimized, stripped_alpha = strip_opaque_etc2_alpha(optimized)

        if optimized != original:
            changed_files += 1
            removed_bytes += len(original) - len(optimized)
        if stripped_alpha:
            stripped_alpha_files += 1
            stripped_alpha_bytes += alpha_input_size - len(optimized)
        if args.apply and optimized != original:
            replace_file(texture_path, optimized)
            verified = texture_path.read_bytes()
            verified_mips, verified_removed_levels = optimized_ktx(
                verified, args.max_dimension
            )
            verified_alpha, verified_stripped_alpha = strip_opaque_etc2_alpha(
                verified_mips
            )
            if (
                verified_removed_levels != 0
                or verified_stripped_alpha
                or verified_alpha != optimized
            ):
                raise RuntimeError(f"post-write validation failed for {relative_path}")

    animation_meshes = list((resources / "Animations").rglob("*_android.mesh"))
    animation_mesh_bytes = sum(path.stat().st_size for path in animation_meshes)

    packed_jobs, packed_materials, skipped_packed = collect_packed_orm_jobs(
        asset_root, source_resources
    )
    packed_bytes = 0
    successful_packed_paths: set[str] = set()
    quality_rejected_paths: set[str] = set()
    if args.apply:
        if not args.etc1tool:
            parser.error("--etc1tool is required with --apply")
        for index, (relative_path, source_paths) in enumerate(
            sorted(packed_jobs.items()), 1
        ):
            try:
                packed_data = build_packed_orm_ktx(
                    source_paths, args.max_dimension, args.etc1tool
                )
            except PackedORMQualityError as error:
                quality_rejected_paths.add(relative_path)
                print(f"Rejected packed ORM {relative_path}: {error}")
                continue
            output_path = asset_root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            if output_path.exists():
                replace_file(output_path, packed_data)
            else:
                output_path.write_bytes(packed_data)
            packed_bytes += len(packed_data)
            successful_packed_paths.add(relative_path)
            if index % 25 == 0 or index == len(packed_jobs):
                print(f"Packed ORM textures: {index}/{len(packed_jobs)}")

        for material_path, packed_path in packed_materials:
            set_packed_orm_record(
                material_path,
                packed_path if packed_path in successful_packed_paths else None,
            )

        packed_root = resources / "Textures" / "PackedORM"
        if packed_root.is_dir():
            for packed_path in packed_root.glob("*_android.ktx"):
                relative_path = packed_path.relative_to(asset_root).as_posix()
                if relative_path not in successful_packed_paths:
                    packed_path.unlink()

        for path in animation_meshes:
            path.unlink()
        write_asset_manifest(asset_root)

    action = "Removed" if args.apply else "Would remove"
    print(
        f"{action} {removed_bytes} bytes from {changed_files} non-sprite KTX files; "
        f"preserved {skipped_sprites} sprite textures"
    )
    print(
        f"{action} {stripped_alpha_bytes} bytes of opaque ETC2 alpha from "
        f"{stripped_alpha_files} textures without recompressing RGB"
    )
    print(
        f"{action} {animation_mesh_bytes} bytes from "
        f"{len(animation_meshes)} redundant animation meshes"
    )
    pack_action = "Packed" if args.apply else "Would pack"
    print(
        f"{pack_action} "
        f"{len(successful_packed_paths) if args.apply else len(packed_jobs)} "
        f"unique ORM textures for "
        f"{sum(path in successful_packed_paths for _, path in packed_materials) if args.apply else len(packed_materials)} materials"
        + (f" ({packed_bytes} bytes)" if args.apply else "")
    )
    if quality_rejected_paths:
        print(f"Preserved individual maps for {len(quality_rejected_paths)} quality-sensitive ORM tuples")
    if skipped_packed:
        print(f"Skipped {len(skipped_packed)} packed ORM candidates:")
        for reason in skipped_packed:
            print(f"  {reason}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
