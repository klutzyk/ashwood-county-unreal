"""Generate reviewable Phase 4.2 source recipes from the measured asset dump."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DUMP = ROOT / 'Saved/PinWright/asset-dumps/Game/_Main/PolyHaven/Textures'
OUT = Path(__file__).parent
sets = [('Timber', 'wood_floor', 200), ('RawTimber', 'raw_wooden_floor_01', 200),
        ('BeigeTile', 'floor_tiles_02', 200), ('WhiteTile', 'tiled_floor_001', 200),
        ('Plaster', 'painted_plaster_wall', 200), ('OldTimber', 'old_wooden_floor_03', 200),
        ('Carpet', 'dirty_carpet', 200)]
recipes = []
for label, prefix, size in sets:
    maps = {}
    for key, suffix in [('Albedo', 'diff'), ('Normal', 'nor_dx'), ('Roughness', 'rough'), ('AO', 'ao')]:
        candidates = [p for p in DUMP.rglob('texture.json') if p.parent.name in
                      (f'{prefix}_{suffix}_2k', f'{prefix}_diffuse_2k' if suffix == 'diff' else '')]
        assert len(candidates) == 1, (label, key, candidates)
        maps[key] = '/Game/_Main/PolyHaven/Textures/' + candidates[0].parent.relative_to(DUMP).as_posix()
    recipes.append(dict(name='MI_PH_' + label, textures=maps, size=size))
(OUT / 'phase42_material_recipes.json').write_text(json.dumps(recipes, indent=2))
default = recipes[0]['textures']
lines = ['entry material `/Game/_Main/Buildings/Materials/Interiors/M_PH_Residential` {',
         '    property MaterialDomain: MD_Surface', '    property BlendMode: BLEND_Opaque',
         '    property ShadingModel: MSM_DefaultLit',
         '    %scale = call `/Script/Engine.MaterialExpressionScalarParameter`(ParameterName: "TextureSizeCm", DefaultValue: 200.0)']
for key in default:
    sampler = 'SAMPLERTYPE_Normal' if key == 'Normal' else ('SAMPLERTYPE_Color' if key == 'Albedo' else 'SAMPLERTYPE_LinearColor')
    lines.append(f'    %{key}Tex = call `/Script/Engine.MaterialExpressionTextureObjectParameter`(ParameterName: "{key}", Texture: "{default[key]}", SamplerType: "{sampler}")')
    func = 'WorldAlignedNormal' if key == 'Normal' else 'WorldAlignedTexture'
    lines.append(f'    %{key} = function_call `/Engine/Functions/Engine_MaterialFunctions01/Texturing/{func}.{func}`(TextureObject: %{key}Tex, TextureSize: %scale)')
lines += ['    %floor = call `/Script/Engine.MaterialExpressionScalarParameter`(ParameterName: "MinimumRoughness", DefaultValue: 0.55)',
          '    %matte = call `/Script/Engine.MaterialExpressionMax`(A: %Roughness[2], B: %floor)',
          '    output BaseColor: %Albedo[2]', '    output Normal: %Normal[1]',
          '    output Roughness: %matte', '    output AmbientOcclusion: %AO[2]', '}']
(OUT / 'phase42_materials.mgir').write_text('\n'.join(lines))
print(json.dumps(recipes))
