// Download Poly Haven (CC0) models + textures at 2k into a folder.
// node polyhaven_fetch.mjs <outDir>
import fs from 'node:fs';
import path from 'node:path';

const OUT = process.argv[2] || 'C:/Users/manri/Documents/UnrealProjects/Owlight/RawAssets/PolyHaven';
const RES = '2k';
const MODELS = `tree_stump_01 tree_stump_02 dead_tree_trunk dead_tree_trunk_02 pine_roots root_cluster_01
root_cluster_02 single_root bark_debris_01 dry_branches_medium_01 rock_moss_set_01 rock_moss_set_02
boulder_01 rock_07 rock_09 stone_01 fern_02 moss_01 grass_medium_01 grass_medium_02 shrub_01 shrub_02
shrub_03 shrub_04 nettle_plant celandine_01 tree_small_02 fir_tree_01 pine_tree_01 fir_sapling
fir_sapling_medium pine_sapling_medium pine_sapling_small`.split(/\s+/);
const TEXTURES = `forest_floor forrest_ground_01 forest_ground_04 forest_leaves_02 forest_leaves_03
forest_leaves_04 leaves_forest_ground mud_forest roots mossy_rock rock_pitted_mossy`.split(/\s+/);

const wanted = k => ['Diffuse','nor_dx','Rough','AO','Displacement'].includes(k) ||
  /_(diff|nor_dx|rough|ao|alpha|mask)$/i.test(k);
const extFor = k => /(nor_dx|alpha|mask)$/i.test(k) ? 'png' : 'jpg';

function pick(node, ext) {
  const out = [];
  const take = n => { const v = n[ext] || Object.values(n)[0]; if (v?.url) out.push(v.url); };
  if (node[RES]) take(node[RES]);
  else for (const sub of Object.values(node)) if (sub?.[RES]) take(sub[RES]);
  return out;
}

async function plan() {
  const jobs = [];
  for (const [kind, ids] of [['models', MODELS], ['textures', TEXTURES]]) {
    for (const id of ids) {
      const files = await (await fetch(`https://api.polyhaven.com/files/${id}`)).json();
      const urls = [];
      if (kind === 'models') {
        const fbx = files.fbx?.[RES]?.fbx?.url;
        if (!fbx) { console.log('NO FBX', id); continue; }
        urls.push(fbx);
      }
      for (const k of Object.keys(files)) if (wanted(k)) urls.push(...pick(files[k], extFor(k)));
      for (const u of urls) jobs.push({ id, kind, url: u, dest: path.join(OUT, kind, id, u.split('/').pop()) });
    }
  }
  return jobs;
}

async function download(job) {
  if (fs.existsSync(job.dest)) return 'skip';
  fs.mkdirSync(path.dirname(job.dest), { recursive: true });
  const r = await fetch(job.url);
  if (!r.ok) throw new Error(`${r.status} ${job.url}`);
  fs.writeFileSync(job.dest, Buffer.from(await r.arrayBuffer()));
  return 'ok';
}

const jobs = await plan();
console.log(`${jobs.length} files planned`);
let done = 0, failed = 0;
const queue = jobs.slice();
await Promise.all(Array.from({ length: 6 }, async () => {
  while (queue.length) {
    const j = queue.shift();
    try { await download(j); done++; }
    catch (e) { failed++; console.log('FAIL', j.url, e.message); }
    if ((done + failed) % 25 === 0) console.log(`${done + failed}/${jobs.length}`);
  }
}));
console.log(`done ${done} failed ${failed}`);
