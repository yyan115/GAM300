#!/usr/bin/env python3
"""
Custom git merge driver for .scene files.

Performs three-way semantic merge on scene JSON files by entity GUID,
so that independent entity edits on different branches merge cleanly
instead of producing line-based conflicts.

Usage (called by git automatically):
    python tools/scene_merge.py %O %A %B %P

    %O = ancestor (base)
    %A = ours (result is written back here)
    %B = theirs
    %P = path name (for diagnostics)

Exit codes:
    0 = clean merge
    1 = conflicts remain (conflict markers written into %A)
"""

import json
import sys
import copy


def load_scene(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def write_scene(path, data):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
        f.write("\n")


def entity_guid(entity):
    return entity.get("guid", "")


def is_prefab_entity(entity):
    return "PrefabPath" in entity


def build_entity_dict(entities):
    """Build a dict of guid -> entity for fast lookup."""
    d = {}
    for e in entities:
        guid = entity_guid(e)
        if guid:
            d[guid] = e
    return d


def get_component_dict(entity):
    """Return the components of an entity as a dict keyed by component name.

    For regular entities, components live under "components".
    For prefab entities, overrides live under "ComponentOverrides" as a list
    of single-key dicts.
    """
    if "components" in entity:
        return dict(entity["components"])
    if "ComponentOverrides" in entity:
        merged = {}
        for override in entity["ComponentOverrides"]:
            merged.update(override)
        return merged
    return {}


def set_components(entity, comp_dict):
    """Write merged components back into the entity."""
    if "components" in entity:
        entity["components"] = comp_dict
    elif "ComponentOverrides" in entity:
        entity["ComponentOverrides"] = [{k: v} for k, v in comp_dict.items()]


def build_children_dict(entity):
    """Build guid -> child dict from prefab Children array (recursive)."""
    children = entity.get("Children", [])
    d = {}
    for c in children:
        guid = entity_guid(c)
        if guid:
            d[guid] = c
    return d


def merge_children(base_entity, ours_entity, theirs_entity):
    """Three-way merge of prefab Children arrays by GUID. Returns (merged_children, had_conflict)."""
    base_children = build_children_dict(base_entity) if base_entity else {}
    ours_children = build_children_dict(ours_entity) if ours_entity else {}
    theirs_children = build_children_dict(theirs_entity) if theirs_entity else {}

    all_guids = list(dict.fromkeys(
        list(ours_children.keys()) + list(theirs_children.keys()) + list(base_children.keys())
    ))

    merged = []
    had_conflict = False

    for guid in all_guids:
        in_base = guid in base_children
        in_ours = guid in ours_children
        in_theirs = guid in theirs_children

        if in_ours and not in_theirs and not in_base:
            merged.append(ours_children[guid])
        elif in_theirs and not in_ours and not in_base:
            merged.append(theirs_children[guid])
        elif in_base and not in_ours and in_theirs:
            if theirs_children[guid] == base_children[guid]:
                continue  # removed by ours, unchanged in theirs
            else:
                merged.append(theirs_children[guid])  # keep theirs' modification
        elif in_base and in_ours and not in_theirs:
            if ours_children[guid] == base_children[guid]:
                continue  # removed by theirs, unchanged in ours
            else:
                merged.append(ours_children[guid])  # keep ours' modification
        elif in_ours and in_theirs:
            base_child = base_children.get(guid)
            child_result, child_conflict = merge_single_entity(
                base_child, ours_children[guid], theirs_children[guid]
            )
            if child_conflict:
                had_conflict = True
            merged.append(child_result)

    return merged, had_conflict


def merge_single_entity(base, ours, theirs):
    """Three-way merge a single entity by component. Returns (merged_entity, had_conflict)."""
    result = copy.deepcopy(ours)
    had_conflict = False

    base_comps = get_component_dict(base) if base else {}
    ours_comps = get_component_dict(ours)
    theirs_comps = get_component_dict(theirs)

    all_comp_names = list(dict.fromkeys(
        list(ours_comps.keys()) + list(theirs_comps.keys()) + list(base_comps.keys())
    ))

    merged_comps = {}
    for name in all_comp_names:
        in_base = name in base_comps
        in_ours = name in ours_comps
        in_theirs = name in theirs_comps

        base_val = base_comps.get(name)
        ours_val = ours_comps.get(name)
        theirs_val = theirs_comps.get(name)

        if in_ours and not in_theirs and not in_base:
            merged_comps[name] = ours_val  # added by ours
        elif in_theirs and not in_ours and not in_base:
            merged_comps[name] = theirs_val  # added by theirs
        elif in_base and not in_ours:
            if in_theirs and theirs_val != base_val:
                merged_comps[name] = theirs_val  # ours removed, theirs modified -> keep theirs
            # else: removed by ours, accept removal
        elif in_base and not in_theirs:
            if in_ours and ours_val != base_val:
                merged_comps[name] = ours_val  # theirs removed, ours modified -> keep ours
            # else: removed by theirs, accept removal
        elif in_ours and in_theirs:
            ours_changed = (ours_val != base_val) if in_base else True
            theirs_changed = (theirs_val != base_val) if in_base else True

            if not ours_changed and not theirs_changed:
                merged_comps[name] = ours_val  # no change
            elif ours_changed and not theirs_changed:
                merged_comps[name] = ours_val  # only ours changed
            elif theirs_changed and not ours_changed:
                merged_comps[name] = theirs_val  # only theirs changed
            elif ours_val == theirs_val:
                merged_comps[name] = ours_val  # both changed identically
            else:
                # True conflict: both modified the same component differently
                had_conflict = True
                merged_comps[name] = {
                    "CONFLICT": True,
                    "base": base_val,
                    "ours": ours_val,
                    "theirs": theirs_val,
                }

    set_components(result, merged_comps)

    # Merge prefab Children recursively
    if "Children" in ours or "Children" in theirs:
        children, child_conflict = merge_children(base, ours, theirs)
        if child_conflict:
            had_conflict = True
        if children:
            result["Children"] = children
        elif "Children" in result:
            del result["Children"]

    # Merge scalar fields that might differ (e.g. "Name" on prefab instances)
    for field in ("Name",):
        base_val = base.get(field) if base else None
        ours_val = ours.get(field)
        theirs_val = theirs.get(field)
        if ours_val != theirs_val:
            if base_val == ours_val:
                result[field] = theirs_val
            # else keep ours (or both changed: ours wins for simple scalars)

    return result, had_conflict


def merge_entities(base_entities, ours_entities, theirs_entities):
    """Three-way merge of entity arrays by GUID.

    Returns (merged_list, had_conflict).
    """
    base_dict = build_entity_dict(base_entities)
    ours_dict = build_entity_dict(ours_entities)
    theirs_dict = build_entity_dict(theirs_entities)

    # Collect all GUIDs, preserving ours' order then appending theirs' new ones
    seen = set()
    ordered_guids = []
    for e in ours_entities:
        g = entity_guid(e)
        if g and g not in seen:
            ordered_guids.append(g)
            seen.add(g)
    for e in theirs_entities:
        g = entity_guid(e)
        if g and g not in seen:
            ordered_guids.append(g)
            seen.add(g)

    merged = []
    had_conflict = False

    for guid in ordered_guids:
        in_base = guid in base_dict
        in_ours = guid in ours_dict
        in_theirs = guid in theirs_dict

        if in_ours and in_theirs:
            if in_base:
                ours_changed = ours_dict[guid] != base_dict[guid]
                theirs_changed = theirs_dict[guid] != base_dict[guid]

                if not ours_changed and not theirs_changed:
                    merged.append(ours_dict[guid])
                elif ours_changed and not theirs_changed:
                    merged.append(ours_dict[guid])
                elif theirs_changed and not ours_changed:
                    merged.append(theirs_dict[guid])
                elif ours_dict[guid] == theirs_dict[guid]:
                    merged.append(ours_dict[guid])
                else:
                    # Both modified the same entity -- merge by component
                    result, conflict = merge_single_entity(
                        base_dict[guid], ours_dict[guid], theirs_dict[guid]
                    )
                    if conflict:
                        had_conflict = True
                    merged.append(result)
            else:
                # Both added same GUID (unlikely) -- keep ours
                merged.append(ours_dict[guid])

        elif in_ours and not in_theirs:
            if in_base:
                if ours_dict[guid] == base_dict[guid]:
                    pass  # theirs deleted, ours unchanged -> delete
                else:
                    merged.append(ours_dict[guid])  # ours modified, theirs deleted -> keep ours
            else:
                merged.append(ours_dict[guid])  # added by ours

        elif in_theirs and not in_ours:
            if in_base:
                if theirs_dict[guid] == base_dict[guid]:
                    pass  # ours deleted, theirs unchanged -> delete
                else:
                    merged.append(theirs_dict[guid])  # theirs modified, ours deleted -> keep theirs
            else:
                merged.append(theirs_dict[guid])  # added by theirs

    return merged, had_conflict


def merge_lighting(base_light, ours_light, theirs_light):
    """Three-way merge of lightingSystem dict per key."""
    if ours_light == theirs_light:
        return ours_light, False
    if base_light == ours_light:
        return theirs_light, False
    if base_light == theirs_light:
        return ours_light, False

    # Both changed: merge per key, ours wins on per-key conflicts
    merged = copy.deepcopy(ours_light)
    for key in set(list(ours_light.keys()) + list(theirs_light.keys())):
        base_val = base_light.get(key) if base_light else None
        ours_val = ours_light.get(key)
        theirs_val = theirs_light.get(key)
        if ours_val == theirs_val:
            merged[key] = ours_val
        elif base_val == ours_val:
            merged[key] = theirs_val
        elif base_val == theirs_val:
            merged[key] = ours_val
        else:
            merged[key] = ours_val  # true conflict on lighting key: ours wins
    return merged, False


def merge_scenes(base, ours, theirs):
    """Three-way merge of entire scene files. Returns (merged_scene, had_conflict)."""
    had_conflict = False

    # Entities
    merged_entities, entity_conflict = merge_entities(
        base.get("entities", []),
        ours.get("entities", []),
        theirs.get("entities", []),
    )
    if entity_conflict:
        had_conflict = True

    # Layers: take ours (rarely changes)
    merged_layers = ours.get("layers", base.get("layers", []))

    # Lighting system: three-way per key
    merged_lighting, _ = merge_lighting(
        base.get("lightingSystem", {}),
        ours.get("lightingSystem", {}),
        theirs.get("lightingSystem", {}),
    )

    merged = {
        "entities": merged_entities,
        "layers": merged_layers,
        "lightingSystem": merged_lighting,
    }

    return merged, had_conflict


def main():
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <base> <ours> <theirs> [path]", file=sys.stderr)
        sys.exit(1)

    base_path = sys.argv[1]
    ours_path = sys.argv[2]
    theirs_path = sys.argv[3]
    scene_name = sys.argv[4] if len(sys.argv) > 4 else "unknown"

    try:
        base = load_scene(base_path)
        ours = load_scene(ours_path)
        theirs = load_scene(theirs_path)
    except (json.JSONDecodeError, FileNotFoundError) as e:
        print(f"scene_merge: failed to parse scene files for '{scene_name}': {e}", file=sys.stderr)
        sys.exit(1)

    merged, had_conflict = merge_scenes(base, ours, theirs)

    write_scene(ours_path, merged)

    if had_conflict:
        print(f"scene_merge: CONFLICT in '{scene_name}' - search for '\"CONFLICT\": true' in the merged file", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"scene_merge: cleanly merged '{scene_name}'", file=sys.stderr)
        sys.exit(0)


if __name__ == "__main__":
    main()
