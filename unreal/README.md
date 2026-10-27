# Unreal side (UE 5.8)

The Unreal project lives at `C:\Users\manri\Documents\UnrealProjects\Owlight` on the owner's PC. Its
`Content/` is not in git (Poly Haven scans, Megascans and the 21112 copies are several GB); this folder
holds everything that rebuilds it: the project file, `Config/`, `Source/` and the scripts.

## Scripts (`Scripts/`, run inside the editor through the Python remote-execution bridge)

```
node ue_py.mjs -c "exec(open(r'<proj>/Scripts/<script>.py', encoding='utf-8-sig').read(), {'<NS>': {...}})"
```

| Script | What it does | Options dict |
|---|---|---|
| `import_polyhaven.py` | Imports `RawAssets/PolyHaven/{models,textures}` into `/Game/PolyHaven`: LOD0 only, Nanite on, textures flagged (normal / grayscale), material instances per slot from two masters. | `PH`: `only=[ids]`, `split=[ids]` (FBX holds several variants), `force`, `nanite` |
| `fix_hornbeam.py` | Rebuilds bark + foliage materials for the Hornbeam skeletal trees copied from 21112 (their PVE master didn't copy). | — |
| `slime_build.py` | Builds `SM_SlimeShell` (sculpted dome via Geometry Script), `SM_SlimeCore`, `M_SlimeShell`, `M_SlimeCore`. Shell params: Color, Clarity, Heat, Glow, Wobble, RimColor, EmissiveScale. | — |
| `style_proof.py` | Rebuilds the `StyleProof` level: dusk light, fog, forest (`sp_forest_ph.py`), slimes, the pixel post-process (`M_PixelPass`, world grid vs finer slime grid by custom stencil). | `SP`: `world_px`, `slime_px`, `pixel`, `sun_yaw`, `sun_pitch`, `sun`, `exposure`, `ground`, `ground_tiling`, `seed`, `slime={...}`, `more_slimes=[{...}]`, `ph` |
| `mrq_shot.py` | Renders stills with Movie Render Queue (works while the window is unfocused) to `Saved/Screenshots/MRQ/`. | `MRQ`: `shots=[(name,(x,y,z),(pitch,yaw),fov)]`, `res`, `warm` |

`polyhaven_fetch.mjs` (Node) downloads the CC0 Poly Haven set used by the style proof into
`RawAssets/PolyHaven` (2k, FBX + diff/nor_dx/rough/ao/disp/alpha maps). `pine_tree_01` is excluded:
its 618 MB FBX ran the editor out of memory.

## Slime parameters ↔ sim

| Sim property | Visual |
|---|---|
| Clarity | shell opacity (`1 - 0.62·c`), refraction (`1 + 0.22·c`), gloss (roughness `0.62 - 0.52·c`), rim strength |
| Heat | base colour warms toward HeatColor, inner emissive, core emissive |
| Density | squash/widen (`sq = 1 - 0.7·(d-0.5)`), sinks a little, bigger core |
| Light (lantern types) | `Glow`: shell + core emissive in GlowColor |

Emissive is in scene units, so `EmissiveScale` is set per instance from the scene's exposure bias
(`0.03 · 2^(9.6 - bias)` reads as "about scene white").

## Style-proof findings (2026-10-27)

- Pixel grid 5 px (world) / 2 px (slime) at 1080p is the sweet spot; 3 px reads as a filter, 8 px loses the ferns.
- The two-grid rule via custom depth stencil works and is cheap.
- Scanned Poly Haven ground + ferns + logs under a Hornbeam canopy at a low 7-lux dusk sun gives the mood; the
  earlier Procedural Nature Pack layout read as a sandbox.
- The slime needs the core: a translucent dome alone reads as glass. Murky slimes must go matte or they turn tan.
