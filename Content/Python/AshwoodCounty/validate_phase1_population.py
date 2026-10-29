"""Read-only validation for the Ashwood Phase 1 population manifest.

Run from Unreal's Python console:
    exec(open(unreal.Paths.project_content_dir() +
         'Python/AshwoodCounty/validate_phase1_population.py').read())

This script deliberately performs no spawning, saving, landscape editing, PCG work,
or data-layer mutation. Physical placement is gated on production asset paths being
filled in and reviewed.
"""

import json
import os
import unreal


LOG_PREFIX = "AshwoodPhase1"


def _log(message):
    unreal.log(f"{LOG_PREFIX}: {message}")


def _error(message):
    unreal.log_error(f"{LOG_PREFIX}: {message}")


def _rects_overlap(a_min, a_max, b_min, b_max):
    return not (
        a_max[0] <= b_min[0]
        or a_min[0] >= b_max[0]
        or a_max[1] <= b_min[1]
        or a_min[1] >= b_max[1]
    )


def validate():
    manifest_path = os.path.join(
        unreal.Paths.project_content_dir(),
        "Python",
        "AshwoodCounty",
        "phase1_population_manifest.json",
    )
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    failures = []
    warnings = []

    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_editor_world()
    active_package = world.get_outermost().get_name() if world else "<no editor world>"
    if active_package != manifest["map"]:
        failures.append(
            f"Open {manifest['map']} before validation; active map is {active_package}."
        )

    for data_layer_path in manifest["required_data_layers"]:
        if not unreal.EditorAssetLibrary.does_asset_exist(data_layer_path):
            failures.append(f"Missing required data layer: {data_layer_path}")

    landscape_min = manifest["landscape_bounds"]["min"]
    landscape_max = manifest["landscape_bounds"]["max"]
    forest_min = manifest["protected_forest_bounds"]["min"]
    forest_max = manifest["protected_forest_bounds"]["max"]
    forbidden_prefixes = tuple(manifest["forbidden_asset_prefixes"])

    ready_assets = 0
    for site in manifest["sites"]:
        centre = site["centre"]
        half = site["footprint_half_extent"]
        site_min = [centre[0] - half[0], centre[1] - half[1]]
        site_max = [centre[0] + half[0], centre[1] + half[1]]

        if (
            site_min[0] < landscape_min[0]
            or site_min[1] < landscape_min[1]
            or site_max[0] > landscape_max[0]
            or site_max[1] > landscape_max[1]
        ):
            failures.append(f"{site['id']} extends outside the Landscape bounds.")

        if _rects_overlap(site_min, site_max, forest_min, forest_max):
            failures.append(f"{site['id']} overlaps the protected forest volume.")

        asset_path = site.get("asset_path")
        if not asset_path:
            warnings.append(f"{site['id']} has no production asset assigned.")
            continue
        if asset_path.startswith(forbidden_prefixes):
            failures.append(
                f"{site['id']} references protected forest/vegetation content: {asset_path}"
            )
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            failures.append(f"{site['id']} asset does not exist: {asset_path}")
            continue

        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
        class_name = str(asset_data.asset_class_path.asset_name)
        if class_name not in {"StaticMesh", "Blueprint", "World"}:
            failures.append(
                f"{site['id']} has unsupported site asset class {class_name}: {asset_path}"
            )
            continue
        ready_assets += 1

    _log(
        f"Validated {len(manifest['sites'])} planned sites; "
        f"{ready_assets} have verified production assets."
    )
    for message in warnings:
        unreal.log_warning(f"{LOG_PREFIX}: {message}")
    for message in failures:
        _error(message)

    if failures:
        _error(f"FAILED with {len(failures)} blocking issue(s). No changes were made.")
        return False
    if ready_assets != len(manifest["sites"]):
        _log("Layout checks passed, but physical population remains asset-gated.")
        return False

    _log("PASS: all sites and production assets are ready for reviewed placement.")
    return True


VALIDATION_RESULT = validate()
