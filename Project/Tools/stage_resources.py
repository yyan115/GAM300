"""Stage desktop runtime assets after cooking, without duplicate source art."""

import argparse
import json
from pathlib import Path, PurePosixPath
import shutil


COOKED_EXTENSIONS = {'.png': '.dds', '.jpg': '.dds', '.jpeg': '.dds',
                     '.bmp': '.dds', '.ttf': '.font', '.obj': '.mesh'}


def compiled_path(resources, value):
    """Resolve metadata written relative to an editor build on either platform."""
    if not isinstance(value, str):
        raise ValueError('compiled path must be a string')
    parts = PurePosixPath(value.replace('\\', '/')).parts
    try:
        index = next(i for i, part in enumerate(parts) if part.lower() == 'resources')
    except StopIteration:
        raise ValueError(f'compiled path is outside Resources: {value}') from None
    relative = parts[index + 1:]
    if not relative or any(part == '..' for part in relative):
        raise ValueError(f'invalid compiled path: {value}')
    path = resources.joinpath(*relative).resolve()
    if not path.is_relative_to(resources.resolve()):
        raise ValueError(f'compiled path escapes Resources: {value}')
    return path


def plan_resources(resources):
    """Validate cooked replacements before copying anything to the package."""
    included, omitted, needed_cooked = [], [], set()
    for source in sorted(resources.rglob('*')):
        if not source.is_file():
            continue
        relative = source.relative_to(resources)
        extension = source.suffix.lower()
        if source.name.endswith('.scene.temp') or extension in {'.md', '.bat'}:
            omitted.append(source)
            continue
        expected = COOKED_EXTENSIONS.get(extension)
        # Animation clips still load their FBX through Assimp at runtime.
        if extension == '.fbx' and relative.parts[0].lower() == 'models':
            expected = '.mesh'
        if expected:
            meta_path = Path(str(source) + '.meta')
            try:
                meta = json.loads(meta_path.read_text(encoding='utf-8'))['AssetMetaData']
                if not isinstance(meta.get('guid'), str) or not meta['guid']:
                    raise ValueError('missing asset GUID')
                cooked = compiled_path(resources, meta['compiled'])
                if cooked == source.resolve() or cooked.suffix.lower() != expected:
                    raise ValueError(f'expected a distinct {expected} resource')
                if not cooked.is_file() or cooked.stat().st_size == 0:
                    raise ValueError(f'cooked resource missing or empty: {cooked}')
            except (OSError, ValueError, KeyError, TypeError) as error:
                raise ValueError(f'{relative}: {error}') from error
            needed_cooked.add(cooked)
            omitted.append(source)
        else:
            included.append(source)

    # A cooked resource is only worth shipping when its source was left out.
    # Model FBX are omitted, so their .mesh is the only copy of that geometry.
    # Animation FBX ship whole because Assimp reads the clip out of them at
    # runtime, and the .mesh the cooker wrote beside each one is never opened:
    # animation clips resolve through the meta's source path, never its
    # compiled path. That is 64 files and 407 MiB of the package.
    kept = []
    for source in included:
        if source.suffix.lower() == '.mesh' and source.resolve() not in needed_cooked:
            omitted.append(source)
            continue
        kept.append(source)
    return kept, omitted


def stage_resources(resources, destination):
    resources, destination = resources.resolve(), destination.resolve()
    if not resources.is_dir():
        raise ValueError(f'Resources directory not found: {resources}')
    if destination == resources or destination.is_relative_to(resources) or resources.is_relative_to(destination):
        raise ValueError('source and destination must be separate directories')
    if destination.exists() and any(destination.iterdir()):
        raise ValueError(f'destination must be empty: {destination}')
    included, omitted = plan_resources(resources)
    for source in included:
        target = destination / source.relative_to(resources)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    size = sum(path.stat().st_size for path in included)
    saved = sum(path.stat().st_size for path in omitted)
    print(f'Staged {len(included)} files ({size / 1048576:.1f} MiB); '
          f'left out {len(omitted)} source and unused files ({saved / 1048576:.1f} MiB).')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    try:
        stage_resources(args.resources, args.destination)
    except ValueError as error:
        parser.exit(1, f'Resource staging failed: {error}\n')
