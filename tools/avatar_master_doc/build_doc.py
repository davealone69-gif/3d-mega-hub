#!/usr/bin/env python3
"""Builds docs/AVATAR_ASSETS_ANIMATION_MASTER.html.

    python3 tools/avatar_master_doc/build_doc.py

Two inputs:
  * the working tree: every source link is a permanent link to the commit that is checked out,
    with its line number found by searching the file, so the links follow the code;
  * doc_extra.json next to this script: the evidence record of 2026-11-15 (preset generation runs,
    the joint list of an exported avatar, CI runs with their jobs, the emulator telemetry, the
    host test result, the donor scan). It is a record, not something this script measures.

Statuses in the document are statements about that evidence. Running this script on a later commit
refreshes the links and nothing else: the page then says which commit the evidence belongs to.
Before changing a status, measure again and update doc_extra.json.

The script prints how many source links did not resolve and how many search patterns matched no
line; both must be 0. It needs Python 3 and git, nothing else."""
import html, json, os, re, subprocess, sys, datetime

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = subprocess.run(['git', 'rev-parse', '--show-toplevel'], cwd=HERE, capture_output=True, text=True).stdout.strip()
EXTRA_PATH = os.path.join(HERE, 'doc_extra.json')
CODE_SHA = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True).stdout.strip()
REPO = 'https://github.com/davealone69-gif/3d-mega-hub'
BRANCH = 'arena/1a5b3043-3d-mega-hub'
CPP = 'avatarforge-engine/src/main/cpp'
KT_ENGINE = 'avatarforge-engine/src/main/java/com/davemega/hub/avatarforge'
KT_APP = 'app/src/main/java/com/davemega/hub'
TESTCPP = 'avatarforge-engine/src/test/cpp'
EXTRA = json.load(open(EXTRA_PATH)) if os.path.exists(EXTRA_PATH) else {}
EVIDENCE_SHA = EXTRA.get('ci_sha', '')
STALE = bool(EVIDENCE_SHA) and EVIDENCE_SHA != CODE_SHA
BUILT = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%d %H:%M UTC')
_APK = EXTRA.get('apk', {})
APK_TAG = _APK.get('tag', 'phone-test-041e31d')
APK_FILE = _APK.get('file', '3D-Mega-Hub-debug-041e31d.apk')
APK_BYTES = int(_APK.get('bytes', 57079246))
APK_SHA = _APK.get('sha256', '285360bec87908b7d9d44cf2a861d4e7d095253a1a064fa8a467486a98aa97c2')
APK_RUN = int(_APK.get('run', 43686767877))
def _git(*a): return subprocess.run(['git', *a], cwd=ROOT, capture_output=True, text=True).stdout
_refs = _git('for-each-ref', '--format=%(refname)').split()
HIST = (f"{int(_git('rev-list', '--all', '--count'))} commits on "
        f"{len([r for r in _refs if r.startswith('refs/remotes/origin/') and not r.endswith('/HEAD')])} branches and "
        f"{len([r for r in _refs if r.startswith('refs/tags/')])} tags")

_cache = {}
def lines(path):
    if path not in _cache:
        try: _cache[path] = open(os.path.join(ROOT, path), errors='replace').read().split('\n')
        except OSError: _cache[path] = None
    return _cache[path]
def L(path, pattern):
    ls = lines(path)
    if ls is None: return None
    rx = re.compile(pattern)
    for i, l in enumerate(ls, 1):
        if rx.search(l): return i
    return None
def url(path, line=None):
    return f'{REPO}/blob/{CODE_SHA}/{path}' + (f'#L{line}' if line else '')
MISSES = []
def src(path, pattern=None, text=None):
    """A link to a source file, at the first line matching pattern."""
    if lines(path) is None:
        return f'<span class="bad">{html.escape(path)} (not found)</span>'
    line = L(path, pattern) if pattern else None
    if pattern and line is None: MISSES.append((path.split('/')[-1], pattern, text))
    label = text or (os.path.basename(path) + (f':{line}' if line else ''))
    return f'<a class="src" href="{url(path, line)}" target="_blank" rel="noopener">{html.escape(label)}</a>'
E = html.escape

LABELS = {'ok': 'REAL + VERIFIED', 'src': 'IMPLEMENTED IN SOURCE', 'part': 'PARTIAL', 'unv': 'UNVERIFIED', 'miss': 'MISSING', 'blk': 'BLOCKED'}
def st(key, note=''):
    return f'<span class="st {key}" data-st="{key}">{LABELS[key]}</span>' + (f'<span class="note"> {note}</span>' if note else '')
def ev(code):
    return f'<a class="ev" href="#ev-{code}">{code}</a>'
def table(head, rows, cls=''):
    h = ''.join(f'<th>{c}</th>' for c in head)
    b = ''.join('<tr>' + ''.join(f'<td>{c}</td>' for c in r) + '</tr>' for r in rows)
    return f'<div class="tw"><table class="{cls}"><thead><tr>{h}</tr></thead><tbody>{b}</tbody></table></div>'
def details(title, body, open_=False, id_=None):
    return f'<details{" open" if open_ else ""}{f" id={id_!r}" if id_ else ""}><summary>{title}</summary><div class="db">{body}</div></details>'

sections = []
def section(id_, title, body):
    sections.append((id_, title, body))

EMU = EXTRA.get('emu', {})
EMU_POSE = (f"{EMU['initialPoseMs']:.1f} ms" if EMU else 'an unrecorded time')
CTEST = EXTRA.get('ctest', {})
CT = f"{CTEST.get('passed', '?')} of {CTEST.get('total', '?')}"
DONORS = EXTRA.get('donors', {})
MX = EXTRA.get('matrix', {})          # name -> dict(rc, secs, triangles, note)
ANIM = EXTRA.get('anim', {})          # built-in animation on the CI emulator: run, clips, played, frames, changedPixels
ANIM_OK = bool(ANIM.get('changedPixels'))
NEWAPK = EXTRA.get('apk', {})         # the phone-test APK of the code this document describes
GUIDE = 'the owner\'s "Create Your Avatar" guide images'
CI = EXTRA.get('ci', [])              # list of dicts
def mx(name):
    m = MX.get(name)
    if not m: return st('unv', 'not generated in this session')
    if m.get('rc') != 0: return st('miss', f'generation failed (rc={m.get("rc")}) ' + ev('H7'))
    base = f'generated on host in {m["secs"]} s, {m["triangles"]:,} triangles ' + ev('H7')
    note = m.get('note', '')
    return st(m.get('grade', 'ok'), base + (f'. <b>Seen in the render:</b> {E(note)}' if note else ''))

# ----------------------------------------------------------------------------------------------- overview
PHONE_OLD = 'phone test of 2026-11-15 used the APK built from 797d156, i.e. the generator before these changes'
overview = f'''
<p class="lead">One inventory of what the avatar side of 3D Mega Hub actually contains: which appearance options exist and
where, what the rig and the animation library are, what the prepared avatar <code>mylove_rig_prepared.glb</code> contains,
what has been proved to work and on what, and what is missing. Every status uses one of six labels and names its evidence.</p>
<div class="cards">
 <div class="card"><b>1</b><span>model file in the repository<br><small>female.glb, a static sample</small></span></div>
 <div class="card"><b>13 + 10 + 4</b><span>hair, outfit and footwear presets<br><small>all procedural, none are files</small></span></div>
 <div class="card"><b>16</b><span>native animation entries<br><small>15 clips + rest, authored in C++</small></span></div>
 <div class="card"><b>71 / 52</b><span>joints / facial blendshapes<br><small>in every generated avatar</small></span></div>
 <div class="card"><b>20 / 0</b><span>joints / clips in the prepared avatar<br><small>a skin, but nothing to play</small></span></div>
 <div class="card warn"><b>0</b><span>animations seen playing on a phone<br><small>{("one pose change proved on the CI emulator" if ANIM_OK else "and none on the emulator yet")}</small></span></div>
</div>
<h3>The short version</h3>
<ul>
 <li><b>Appearance.</b> There is no library of hair, clothing or skin files and there never was ({ev("G2")}): every option is a
  <em>procedural preset</em> inside the AvatarForge C++ engine. The app offers all of them in a creator laid out after {GUIDE}:
  a rail of sections beside the avatar, a picture of every preset, colour dots and sliders ({st("src")}; compiled, linted and unit-tested by CI {ev("C3")}, not yet run on a phone).
  Every preset is also reachable from a prompt ({st("ok")} {ev("H8")}).</li>
 <li><b>Animation.</b> The engine has a real skeleton, skin weights and 15 authored clips, and writes all of them into every GLB it
  exports. Since <code>b636f23</code> an avatar keeps them when its GLB is loaded back ({st("ok")} {ev("H5")}). Since <code>a5e93a8</code> the GLES renderer
  plays them itself by GPU skinning, and the creator has the controls: a strip of animations under the avatar and a Motion section with play, pause, stop, loop and speed
  ({(st("ok", "one pose change proved through the production view on the CI emulator") + " " + ev("E2")) if ANIM_OK else st("src")}). <b>Nobody has watched one play on a phone.</b></li>
 <li><b>Why the earlier build crashed.</b> The owner's phone report on the <code>041e31d</code> build was "animation crashes and no built in controls" ({ev("P2")}). The only way that build
  offered to see a generated avatar move was to send it to the Filament preview, whose loader reserves morph storage for every vertex of every target: several hundred megabytes for these
  avatars. That road is closed: the preview is never given such a file ({st("ok")} unit tests, {ev("C3")}). The cause was found by reading the code; no crash log was sent, so it is the likely cause, not a proved one.</li>
 <li><b>Imported GLB animation.</b> A file from elsewhere that carries its own clips opens in the Filament preview, which has playback controls ({st("src")}). They have
  never been seen working. The prepared avatar has no clips in it, so it cannot test them; a file that can, the Khronos sample
  <code>CesiumMan.glb</code>, sits in one of the owner's other repositories (<a href="#donors">donor projects</a>). The phone report does not say whether it was tried.</li>
 <li><b>Prepared avatar.</b> Retrieved from Dropbox and inspected byte for byte: 144,073 triangles, one WebP-textured material,
  a 20-joint skin whose own metadata says <i>"approximate_unverified … Rig deformation and retargeting have not been tested"</i>,
  <b>0 animation clips</b>, no morph targets. It displays as a static model. See <a href="#prepared">the compatibility report</a>.</li>
 <li><b>Avatar quality.</b> The generator's face, eyes, hairline and garment edges were rebuilt in this session; renders are in
  <a href="#gallery">the gallery</a>. They are host software renders of the engine's output, not phone captures. Every preset was generated and looked at:
  all 27 hair, outfit and footwear presets look like what they are named in those renders ({ev("H7")}); 14 of them were rebuilt in <code>9519eae</code> and five were added in <code>2920265</code>.</li>
 <li><b>The owner's picture reference.</b> On 2026-11-15 the owner sent three screenshots of a dark picture creator and eight body references with the words "These are very important". Since <code>2920265</code>
  the creator offers the choices those screenshots show, each connected to the engine: six starting looks ("Ethnicity"), body type, breast size, butt size, hair colour and fourteen hair styles as picture cards, a Face section
  with eye colour and seven face sliders, a swimwear outfit, and a dark look ({st("src")}; every card generated on the host and looked at {ev("H9")}; never seen on a phone). Bodies and faces of the quality of those references are <b>not</b> reached.</li>
 <li><b>Other projects.</b> {DONORS.get("scanned", "?")} of the owner's repositories were searched. There is no wardrobe or animation library in any of them;
  there are {DONORS.get("total_model_files", "?")} GLB files, four of which carry a skeleton and a clip (<a href="#donors">donor projects</a>).</li>
</ul>
<h3>The six labels</h3>
<p>{st("ok")} seen working, with the evidence named. &nbsp; {st("src")} the code exists and compiles; nothing more is claimed. &nbsp;
{st("part")} some of it works. &nbsp; {st("unv")} nobody has checked. &nbsp; {st("miss")} it does not exist. &nbsp; {st("blk")} cannot proceed without something outside the code.</p>
<p class="small">Click a label in the bar at the top to show only rows with that status. Evidence codes such as {ev("H5")} link to
<a href="#evidence">the evidence register</a>. Source links are permanent links to commit <code>{CODE_SHA[:7]}</code>.</p>
'''
section('overview', 'Overview', overview)

# ----------------------------------------------------------------------------------------------- source map
rows = [
 ['Preset enumerations (C ABI)', src(f'{CPP}/include/afcore.h', r'typedef enum af_hair_style', 'afcore.h: af_hair_style') + ' · ' + src(f'{CPP}/include/afcore.h', r'typedef enum af_outfit', 'af_outfit') + ' · ' + src(f'{CPP}/include/afcore.h', r'typedef enum af_footwear', 'af_footwear') + ' · ' + src(f'{CPP}/include/afcore.h', r'typedef struct af_spec', 'af_spec'), 'The numbers every layer agrees on.'],
 ['Preset enumerations (Kotlin)', src(f'{KT_ENGINE}/AvatarSpec.kt', r'enum class HairStyle', 'AvatarSpec.kt: HairStyle') + ' · ' + src(f'{KT_ENGINE}/AvatarSpec.kt', r'enum class Outfit', 'Outfit') + ' · ' + src(f'{KT_ENGINE}/AvatarSpec.kt', r'^data class AvatarSpec', 'AvatarSpec'), 'What the app passes to the engine.'],
 ['Body, face, hands, feet (shape)', src(f'{CPP}/src/af_field.cpp', r'^float BodyField::torso\(', 'af_field.cpp: torso') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::skull', 'skull') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::faceFeatures', 'faceFeatures') + ' · ' + src(f'{CPP}/src/af_anatomy.cpp', r'Anatomy::solve', 'af_anatomy.cpp: solve'), 'Signed-distance functions; proportions from the spec.'],
 ['Hair (shape)', src(f'{CPP}/src/af_field.cpp', r'^float BodyField::hairShape', 'af_field.cpp: hairShape'), 'One function, one <code>switch</code> over the thirteen styles.'],
 ['Clothing, footwear, armour (shape)', src(f'{CPP}/src/af_field.cpp', r'^float BodyField::clothUpper', 'clothUpper') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::clothLower', 'clothLower') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::shoeShape', 'shoeShape') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::gearShape', 'gearShape'), 'Garments are offsets of the body, not separate meshes.'],
 ['Which material a surface point gets', src(f'{CPP}/src/af_field.cpp', r'^int BodyField::classifyRegion', 'af_field.cpp: classifyRegion') + ' · ' + src(f'{CPP}/src/af_mesh.h', r'REGION_SKIN = 0', 'af_mesh.h: regions'), '13 regions: skin, two eyes, teeth, tongue, lips, hair, lash/brow, nail, upper cloth, lower cloth, shoe, gear.'],
 ['Surface extraction', src(f'{CPP}/src/af_body.cpp', r'^bool buildBodyMesh', 'af_body.cpp: buildBodyMesh') + ' · ' + src(f'{CPP}/src/af_isosurface.cpp', None, 'af_isosurface.cpp'), 'Lattice sampling, marching tetrahedra, refinement, projection.'],
 ['UV atlas and texture bake', src(f'{CPP}/src/af_uv.cpp', None, 'af_uv.cpp') + ' · ' + src(f'{CPP}/src/af_texture.cpp', r'^void evaluateSkin', 'af_texture.cpp: evaluateSkin') + ' · ' + src(f'{CPP}/src/af_texture.cpp', r'^void evaluateEye', 'evaluateEye') + ' · ' + src(f'{CPP}/src/af_texture.cpp', r'^void evaluateCloth', 'evaluateCloth'), 'Five baked maps: base colour, normal, metallic-roughness, occlusion, subsurface.'],
 ['Skeleton, weights, blendshapes, clips', src(f'{CPP}/src/af_rig.cpp', r'void Rig::build\w*\(', 'af_rig.cpp: skeleton') + ' · ' + src(f'{CPP}/src/af_rig.cpp', r'void Rig::buildBlendshapes', 'buildBlendshapes') + ' · ' + src(f'{CPP}/src/af_rig.cpp', r'"idle"', 'clip library'), 'All authored in code; no animation files.'],
 ['Prompt words to presets', src(f'{CPP}/src/af_prompt.cpp', r'kTshirt\[\]', 'af_prompt.cpp: outfit words') + ' · ' + src(f'{CPP}/src/af_prompt.cpp', r'w == "ponytail"', 'hair words') + ' · ' + src(f'{CPP}/src/af_prompt.cpp', r'kPale\[\]', 'skin words'), 'A keyword parser, not a language model.'],
 ['Engine entry points', src(f'{CPP}/src/af_engine.cpp', r'^int32_t af_engine_run_all', 'af_engine.cpp: run_all') + ' · ' + src(f'{CPP}/src/af_engine.cpp', r'^int32_t af_engine_import_glb\(', 'import_glb') + ' · ' + src(f'{CPP}/src/af_engine.cpp', r'^int32_t af_engine_evaluate_animation', 'evaluate_animation') + ' · ' + src(f'{CPP}/src/af_engine.cpp', r'^int32_t af_engine_apply_clip', 'apply_clip'), ''],
 ['GLB writer and reader', src(f'{CPP}/src/af_glb.cpp', None, 'af_glb.cpp') + ' · ' + src(f'{CPP}/src/af_glb_read.cpp', None, 'af_glb_read.cpp'), 'Export: skin, 71 joints, 52 sparse morph targets, 15 animations, 5 PNG maps.'],
 ['Native renderer (GLES)', src(f'{CPP}/src/render/af_gl.cpp', None, 'render/af_gl.cpp') + ' · ' + src(f'{CPP}/src/render/af_render.cpp', None, 'af_render.cpp') + ' · ' + src(f'{CPP}/src/render/af_render_capi.cpp', None, 'af_render_capi.cpp'), 'Draws generated and imported avatars. Also plays an avatar\'s animations by GPU skinning (since 210ae9a). An older CPU clip player is still in the source and nothing calls it.'],
 ['App: Avatar Studio screen', src(f'{KT_APP}/AvatarStudioScreen.kt', r'^fun AvatarStudioScreen', 'AvatarStudioScreen.kt') + ' · ' + src(f'{KT_APP}/AppearanceControls.kt', r'^fun AppearanceControls', 'AppearanceControls.kt') + ' · ' + src(f'{KT_APP}/GlbPreviewRouting.kt', r'^object GlbPreviewRouting', 'GlbPreviewRouting.kt'), 'Controls, generation, preview choice.'],
 ['App: the two previews', src(f'{KT_ENGINE}/AvatarForgeSurfaceView.kt', None, 'AvatarForgeSurfaceView.kt') + ' · ' + src(f'{KT_APP}/FilamentPreviewView.kt', r'fun selectAnimation', 'FilamentPreviewView.kt: animation controls'), 'Native GLES view, which also plays an avatar\'s own animations; Filament gltfio view for files from elsewhere that carry clips.'],
 ['App: creator panels and preset pictures', src(f'{KT_APP}/AvatarCreatorPanels.kt', r'^fun CreatorRail', 'AvatarCreatorPanels.kt') + ' · ' + src(f'{KT_APP}/AvatarCreatorPanels.kt', r'^object PresetThumbnails', 'PresetThumbnails') + ' · <code>app/src/main/res/drawable-nodpi/preset_*.jpg</code>', 'The rail, the preset cards, colour dots and the section panels; 51 pictures rendered by the host tool from the engine\'s own output.'],
 ['App to engine bridge', src(f'{KT_ENGINE}/AvatarForgeEngine.kt', r'suspend fun generate', 'AvatarForgeEngine.kt: generate'), 'create → analyze_prompt → run_all → validate → export_glb.'],
 ['Host tools', src(f'{TESTCPP}/host_avatar_render.cpp', None, 'host_avatar_render.cpp') + ' · ' + src(f'{TESTCPP}/host_avatar_probe.cpp', None, 'host_avatar_probe.cpp') + ' · ' + src(f'{TESTCPP}/animation_roundtrip_test.cpp', None, 'animation_roundtrip_test.cpp'), 'Generate, import, report, draw and test without a phone.'],
]
section('map', 'Where everything is', '<p>The avatar is made by one engine, <b>AvatarForge</b>, vendored into this repository as C++ under <code>avatarforge-engine/</code>. '
        'Nothing below is a separate application.</p>' + table(['What', 'Source', 'Note'], rows, 'filter'))

# ----------------------------------------------------------------------------------------------- hair
HAIR = [(0, 'NONE', 'None (bald)', 'bald, shaved', 'hair_0_none'), (1, 'BUZZ', 'Buzz', 'buzz, buzzcut', 'hair_1_buzz'), (2, 'SHORT', 'Short', 'short hair', 'hair_2_short'),
        (3, 'MEDIUM', 'Medium (to the shoulders)', 'medium / default', 'hair_3_medium'), (4, 'LONG', 'Long (&quot;Straight&quot; in the creator)', 'long, flowing, waist, shoulder-length hair', 'hair_4_long'),
        (5, 'PONYTAIL', 'Ponytail', 'ponytail', 'hair_5_ponytail'), (6, 'BUN', 'Bun', 'bun', 'hair_6_bun'), (7, 'BRAIDS', 'Braids, two plaits (&quot;Twin braids&quot;)', 'braid, braids, locks, dreadlocks', 'hair_7_braids'),
        (8, 'AFRO', 'Afro', 'afro', 'hair_8_afro'), (9, 'MOHAWK', 'Mohawk', 'mohawk', 'hair_9_mohawk'),
        (10, 'BOB', 'Bob', 'bob', 'hair_10_bob'), (11, 'BANGS', 'Bangs', 'bangs, fringe', 'hair_11_bangs'), (12, 'CURLY', 'Curly', 'curls', 'hair_12_curly'),
        (13, 'BOX_BRAIDS', 'Box braids (&quot;Braids&quot; in the creator)', '', 'hair_13_box_braids')]
NEWPICK = st('src', 'a picture card in the creator since 2920265; compiled, linted and unit-tested by CI ' + ev('C3') + '; not run on a phone')
PICKER = st('src', 'a picture card in the creator since 210ae9a (a text picker since f90f326); compiled, linted and unit-tested by CI ' + ev('C3') + '; not run on a phone') 
CHIP = st('src', 'a chip in the creator\'s Body section since 210ae9a (a text picker since f90f326); compiled, linted and unit-tested by CI ' + ev('C3') + '; not run on a phone')
rows = []
for n, enum, label, words, key in HAIR:
    rows.append([f'<b>{label}</b><br><code>AF_HAIR_{enum} = {n}</code>',
                 'Procedural preset<br><small>a branch of one distance function; no file, no size</small>',
                 src(f'{CPP}/src/af_field.cpp', r'^float BodyField::hairShape', 'hairShape()') + '<br>' + src(f'{CPP}/include/afcore.h', rf'AF_HAIR_{enum}\b', 'enum') + ' · ' + src(f'{KT_ENGINE}/AvatarSpec.kt', rf'\b{enum}\({n}\)', 'Kotlin'),
                 f'<small>{E(words) if words else "none"}</small>',
                 (NEWPICK if n >= 10 else PICKER) + '<br>' + (st('ok', 'a prompt selects it: host test ' + ev('H8')) if words else st('miss', 'no prompt word; the creator only')),
                 mx(key) if key else st('unv', 'not generated in this session'),
                 st('unv', PHONE_OLD if n == 3 else 'never generated on the phone')])
hair_html = ('<p>Thirteen styles plus "none". Each is a <b>procedural preset</b>: a case in <code>hairShape()</code> that describes the hair as a solid '
             'volume around the head. There are no hair meshes, cards or strands, and no hair texture files; colour and fibre shading come from the bake. '
             'Bob, bangs, curly and box braids were added in <code>2920265</code> for the owner\'s picture reference; the same commit merged the integration branch\'s hair work '
             '(a widow\'s point and temple recession in the hairline, a coil-scale field on the afro, clump channels down the back of hanging hair, a root-to-tip colour gradient) - '
             'the README says how each of those commits was carried over. '
             'Hair colour is a separate value (<code>hair_tone</code>), see <a href="#params">appearance parameters</a>.</p>'
             + table(['Style', 'What it is', 'Source', 'Prompt words', 'Connected to the app', 'Generated and drawn (host)', 'On the Samsung phone'], rows, 'filter')
             + '<p class="small">Licence and provenance: written for this project inside AvatarForge (repository <code>davealone69-gif/AvatarForge</code>); the repository carries no LICENSE file, so no licence is stated.</p>')
section('hair', 'Hair presets', hair_html)

# ----------------------------------------------------------------------------------------------- outfits
OUT = [(0, 'NONE', 'None (unclothed base)', 'nude, naked, bare, unclothed', '-', None),
       (1, 'TSHIRT', 'T-shirt (with jeans)', 'tshirt, t-shirt, tee, shirt', 'jeans', 'outfit_1_tshirt'),
       (2, 'DRESS', 'Dress (full length)', 'dress, gown', 'dress lining', 'outfit_2_dress'),
       (3, 'SUIT', 'Suit', 'suit, tuxedo, business', 'trousers', 'outfit_3_suit'),
       (4, 'JEANS_JACKET', 'Jeans and jacket', 'jeans, denim, jacket, hoodie, sweater', 'jeans', 'outfit_4_jeans_jacket'),
       (5, 'ARMOR_PLATE', 'Plate armour', 'armour, armor, plate, cuirass, knight, breastplate', 'greaves', 'outfit_5_armor_plate'),
       (6, 'ARMOR_LEATHER', 'Leather armour', 'leather, ranger, rogue', 'leather legs', 'outfit_6_armor_leather'),
       (7, 'SPACESUIT', 'Spacesuit', 'spacesuit, space, astronaut, hazmat, exosuit', 'suit legs', 'outfit_7_spacesuit'),
       (8, 'ATHLETIC', 'Athletic', 'sportswear, athletic, gym, running', 'shorts', 'outfit_8_athletic'),
       (9, 'COAT', 'Coat', 'coat, trench, overcoat, cloak', 'trousers', 'outfit_9_coat'),
       (10, 'SWIMWEAR', 'Swimwear (two-piece)', 'swimwear, swimsuit, bikini', 'briefs; trunks on a male body, which has no top', 'outfit_10_swimwear')]
rows = []
for n, enum, label, words, lower, key in OUT:
    app = (st('blk', 'not offered: the unclothed base needs the adult-mode confirmation the app does not expose') if n == 0 else (NEWPICK if n >= 10 else PICKER) + '<br>' + st('ok', 'a prompt selects it: host test ' + ev('H8')))
    rows.append([f'<b>{label}</b><br><code>AF_OUTFIT_{enum} = {n}</code>',
                 'Procedural preset<br><small>upper garment + lower garment' + (' + plates/hardware' if n in (5, 6, 7) else '') + '; no file</small>',
                 src(f'{CPP}/src/af_field.cpp', r'^float BodyField::clothUpper', 'clothUpper()') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::clothLower', 'clothLower()') + (' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::gearShape', 'gearShape()') if n in (5, 6, 7) else '')
                 + '<br>' + src(f'{CPP}/include/afcore.h', rf'AF_OUTFIT_{enum}\b', 'enum') + ' · ' + src(f'{KT_ENGINE}/AvatarSpec.kt', rf'\b{enum}\({n}\)', 'Kotlin'),
                 f'<small>{E(words)}</small><br><small>lower: {E(lower)}</small>', app,
                 mx(key) if key else st('unv', 'not generated in this session'),
                 st('unv', 'the phone avatar of 2026-11-15 wore this outfit, built by the old generator' if n == 4 else 'never generated on the phone')])
FOOT = [(0, 'NONE', 'None (bare feet)', '-', 'footwear_0_none'), (1, 'SNEAKER', 'Sneaker', 'sneakers, trainers, shoes, runners', 'footwear_1_sneaker'),
        (2, 'BOOT', 'Boot', 'boots, boot', 'footwear_2_boot'), (3, 'SABATON', 'Sabaton', 'sabatons, greaves', 'footwear_3_sabaton'), (4, 'HEEL', 'Heel', 'heels, stilettos, high-heels', 'footwear_4_heel')]
frows = []
for n, enum, label, words, key in FOOT:
    frows.append([f'<b>{label}</b><br><code>AF_FOOTWEAR_{enum} = {n}</code>', 'Procedural preset', src(f'{CPP}/src/af_field.cpp', r'^float BodyField::shoeShape', 'shoeShape()') + ' · ' + src(f'{CPP}/include/afcore.h', rf'AF_FOOTWEAR_{enum}\b', 'enum'),
                  f'<small>{E(words)}</small>', PICKER + ('<br>' + st('ok', 'a prompt selects it: host test ' + ev('H8')) if n else ''), mx(key) + '<br><small>generated with the athletic outfit; feet drawn close up</small>', st('unv', 'never generated on the phone')])
section('clothing', 'Clothing and footwear presets',
        '<p>Ten outfits plus the unclothed base, and four kinds of footwear plus bare feet. A garment here is the body\'s own surface moved outwards by the cloth\'s '
        'thickness and kept inside the garment\'s outline. That makes every garment fit every body, and it also means a garment <b>cannot be taken off one avatar and put on another</b>: '
        'there is no garment mesh to move. Clothing colour is <code>cloth_tone</code>; armour colour and metalness are <code>gear_tone</code> and <code>gear_metalness</code>. '
        'Since <code>9519eae</code> the parts inside a garment (the shirt and tie in a jacket\'s opening, stitching, buttons, soles, laces, the joints between plates) are painted by the texture bake from ' + src(f'{CPP}/src/af_field.cpp', r'^int BodyField::apparelPart', 'apparelPart()') + ', '
        'armour and space-suit hardware is built per kit in ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::gearField', 'gearField()') + ', each outfit has its own colours while the clothing colour is left at "Auto", '
        'and heels pitch the foot about a raised ankle (' + src(f'{CPP}/src/af_field.cpp', r'^Vec3 BodyField::footLocal', 'footLocal()') + ').</p>'
        + table(['Outfit', 'What it is', 'Source', 'Prompt words', 'Connected to the app', 'Generated and drawn (host)', 'On the Samsung phone'], rows, 'filter')
        + '<h3>Footwear</h3>' + table(['Footwear', 'What it is', 'Source', 'Prompt words', 'Connected to the app', 'Generated and drawn (host)', 'On the Samsung phone'], frows, 'filter'))

# ----------------------------------------------------------------------------------------------- parameters
SPEC_H = lambda f: src(f'{CPP}/include/afcore.h', f if f.startswith('typedef') else rf'^\s+\w+\s+[^/]*\b{f}\s*[;,]', 'afcore.h')
SPEC_K = lambda f: src(f'{KT_ENGINE}/AvatarSpec.kt', rf'val {f}\b', 'AvatarSpec.kt')
NOUI = st('miss', 'no control; reachable only by code')
rows = [
 ['Skin tone (melanin)', '<code>skin_melanin</code> 0–1, default 0.35', SPEC_H('skin_melanin') + ' · ' + SPEC_K('skinMelanin') + ' · ' + src(f'{CPP}/src/af_texture.cpp', r'^void evaluateSkin', 'evaluateSkin()'), st('src', 'slider since f90f326') + '<br>' + st('ok', 'prompt words pale / olive / tan / brown / dark'), st('part', 'default value drawn ' + ev('H4') + '; other values not drawn'), st('unv')],
 ['Skin colour (explicit)', '<code>skin_tone[3]</code> linear RGB, 0 = derive from melanin', SPEC_H(r'skin_tone\[3\]') + ' · ' + SPEC_K('skinTone'), NOUI, st('unv'), st('unv')],
 ['Hair colour', '<code>hair_tone[3]</code> linear RGB, 0 = engine default', SPEC_H(r'hair_tone\[3\]') + ' · ' + SPEC_K('hairTone'), st('src', '12 colour tiles since 2920265 (brown, black, blonde, ginger and pink first, as in the picture reference); 8 swatches since f90f326') + '<br>' + st('ok', 'prompt colour words'), st('part', 'default dark brown drawn ' + ev('H4') + '; swatch values not drawn'), st('unv')],
 ['Clothing colour', '<code>cloth_tone[3]</code>', SPEC_H(r'cloth_tone\[3\]') + ' · ' + SPEC_K('clothTone') + ' · ' + src(f'{CPP}/src/af_texture.cpp', r'spec\.cloth_tone\[0\]', 'bake'), st('src', '7 swatches since f90f326') + '<br>' + st('ok', 'prompt colour before a garment word'), st('part', '"black t-shirt" drawn black ' + ev('H4') + '; which garments a swatch recolours is not checked'), st('unv')],
 ['Armour colour, metalness', '<code>gear_tone[3]</code>, <code>gear_metalness</code>', SPEC_H(r'gear_tone\[3\]') + ' · ' + SPEC_K('gearTone'), NOUI, st('unv'), st('unv')],
 ['Cloth roughness', '<code>cloth_roughness</code>', SPEC_H('cloth_roughness') + ' · ' + SPEC_K('clothRoughness'), NOUI, st('unv'), st('unv')],
 ['Body: sex', '<code>sex</code>: unspecified, male, female', SPEC_H(r'typedef enum af_sex') + ' · ' + src(f'{KT_ENGINE}/AvatarSpec.kt', r'enum class Sex', 'Kotlin'), CHIP, st('part', 'female drawn ' + ev('H4') + '; male and unspecified not drawn since the generator changes'), st('unv', PHONE_OLD)],
 ['Body type', '<code>build</code>: average, slim, athletic, muscular, heavy, curvy (curvy added in 2920265), sent together with the <code>muscle</code> and <code>body_fat</code> values that go with the name. Six body types in the creator: Slim, Athletic, Average, Curvy, Plus size, Muscular', SPEC_H(r'typedef enum af_build') + ' · ' + src(f'{KT_ENGINE}/AvatarSpec.kt', r'enum class Build', 'Kotlin') + ' · ' + src(f'{KT_APP}/AppearanceControls.kt', r'^enum class BodyType', 'BodyType'), NEWPICK, st('ok', 'all six generated in swimwear and looked at ' + ev('H9') + '; the curvy build\'s smaller waist-to-hip ratio is a host test ' + ev('H1')), st('unv', PHONE_OLD)],
 ['Body: height', '<code>height_m</code>, default 1.75', SPEC_H('height_m') + ' · ' + SPEC_K('heightM'), st('src', 'slider 1.45–2.05 m since f90f326') + '<br>' + st('ok', '"175 cm tall" in a prompt ' + ev('H1')), st('ok', '1.75 m asked, 1.769 m built ' + ev('H2')), st('unv')],
 ['Breast size', '<code>bust</code> 0–1. Five sizes in the creator: Flat 0.04, Small 0.24, Average 0.45, Big 0.70, Huge 1.00. Until 2920265 the engine read this field and then overwrote it for every female body, so it did nothing; a measured bust girth (<code>chest_cm</code>) still decides when one is given', SPEC_H('bust') + ' · ' + SPEC_K('bust') + ' · ' + src(f'{CPP}/src/af_anatomy.cpp', r'spec\.chest_cm > 1\.0f && adult', 'af_anatomy.cpp') + ' · ' + src(f'{KT_APP}/AppearanceControls.kt', r'^enum class BustSize', 'BustSize'), NEWPICK + '<br><small>shown for a female or unspecified body</small>', st('ok', 'five sizes generated and looked at ' + ev('H9') + '; each stands further forward than the one before, 64 mm from flat to huge: host test ' + ev('H1')), st('unv')],
 ['Butt size', '<code>glute</code> -1..+1 about the build\'s own (0), new in 2920265. Five sizes in the creator: Small -0.90, Slim -0.45, Athletic 0, Big +0.50, Huge +1.00', SPEC_H('glute') + ' · ' + SPEC_K('glute') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'const float g = m_\.glute;', 'af_field.cpp: torso()') + ' · ' + src(f'{KT_APP}/AppearanceControls.kt', r'^enum class GluteSize', 'GluteSize'), NEWPICK, st('ok', 'five sizes generated and looked at ' + ev('H9') + '; each stands further back than the one before, 65 mm from small to huge: host test ' + ev('H1')), st('unv')],
 ['Body: age', '<code>age_years</code> 16–70, default 30. Bust and glute are honoured for 18 and over only: below that the engine uses its defaults whatever is asked (host test ' + ev('H1') + ')', SPEC_H('age_years') + ' · ' + SPEC_K('ageYears') + ' · ' + src(f'{CPP}/src/af_anatomy.cpp', r'const bool adult = m\.age >= 18\.0f;', 'af_anatomy.cpp'), st('src', 'no control: every avatar the creator builds is 30, unit-tested ' + ev('C3')), st('unv', 'not drawn at any other age'), st('unv')],
 ['Body: 13 tape measurements', 'neck, chest, underbust, waist, hip, thigh, calf, bicep, forearm, wrist, shoulder width, inseam, head circumference (cm)', SPEC_H('neck_cm') + ' · ' + SPEC_K('neckCm'), NOUI, st('unv', 'a perimeter test exists in the engine; not run as part of this inventory'), st('unv')],
 ['Face shape', '<code>face[8]</code>, new in 2920265: seven offsets from the default face, each -1..+1 with 0 the default - nose width, nose bridge, lip fullness, eye tilt, eye opening, cheekbones, jaw width (the eighth slot is reserved)', src(f'{CPP}/include/afcore.h', r'^typedef enum af_face_param', 'af_face_param') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::faceFeatures', 'faceFeatures()') + ' · ' + src(f'{CPP}/src/af_field.cpp', r'^float BodyField::skull', 'skull()') + ' · ' + src(f'{KT_ENGINE}/AvatarSpec.kt', r'^enum class FaceParam', 'FaceParam'), st('src', 'seven sliders in the creator\'s Face section since 2920265; compiled and unit-tested by CI ' + ev('C3') + '; not run on a phone'), st('part', 'six combinations drawn as the six looks ' + ev('H9') + '; the offsets are clamped, both ends of every one still give a head, and the nose-width offset changes the width of the nose: host test ' + ev('H1') + '. No offset was drawn alone at each end'), st('unv')],
 ['Eye colour', '<code>eye_tone[3]</code> linear RGB, new in 2920265; 0 = derived from the skin tone as before', SPEC_H(r'eye_tone\[3\]') + ' · ' + SPEC_K('eyeTone') + ' · ' + src(f'{CPP}/src/af_texture.cpp', r'ctx\.spec->eye_tone', 'evaluateEye()'), st('src', '7 colour tiles in the Face section since 2920265'), st('part', 'blue, dark brown, brown and hazel drawn on the six looks ' + ev('H9') + '; green and grey not drawn'), st('unv')],
 ['Starting looks (&quot;Ethnicity&quot;)', 'Not an engine field: six presets in the app - White, Asian, Black, Latina / Latino, South Asian, Middle Eastern - each of which sets the skin tone, the hair colour, the eye colour and the seven face offsets, and nothing else. Every value can be changed afterwards', src(f'{KT_APP}/AppearanceControls.kt', r'^enum class Ethnicity', 'Ethnicity') + ' · ' + src(f'{KT_APP}/AvatarCreatorPanels.kt', r'^fun LookPanel', 'LookPanel'), NEWPICK, st('ok', 'all six generated with exactly the preset\'s values and looked at ' + ev('H9')), st('unv')],
 ['Photo input', '<code>af_engine_analyze_images</code>: framing, proportions and colours only', src(f'{CPP}/include/afcore.h', r'^int32_t af_engine_analyze_images', 'afcore.h'), st('miss', 'not offered in the app; the engine states it does not reconstruct a face'), st('unv'), st('unv')],
 ['Seed', '<code>seed</code>, default 1337', SPEC_H('seed') + ' · ' + SPEC_K('seed'), NOUI, st('unv'), st('unv')],
]
MATS = [('skin', 'evaluateSkin'), ('eye_left / eye_right', 'evaluateEye'), ('teeth', 'evaluateTeeth'), ('tongue', 'evaluateTongue'), ('lips', 'evaluateLips'), ('nail', 'evaluateNail'),
        ('cloth_upper / cloth_lower', 'evaluateCloth'), ('shoe', 'evaluateShoe'), ('gear', 'evaluateGear')]
mrows = [[f'<code>{m}</code>', src(f'{CPP}/src/af_texture.cpp', rf'^void {fn}\(', f'{fn}()'), 'Procedural: colour, roughness and relief computed per texel'] for m, fn in MATS]
mrows.append(['<code>hair</code>, <code>lash</code> (brows, lash line)', src(f'{CPP}/src/af_texture.cpp', r'^void evaluateSurface', 'evaluateSurface()'), 'Procedural'])
params = ('<p>Everything else that changes how an avatar looks is a number in one structure, <code>af_spec</code> / <code>AvatarSpec</code>. '
          'The table says, for each, whether the app lets you set it.</p>'
          + table(['Parameter', 'Field and range', 'Source', 'Connected to the app', 'Generated and drawn (host)', 'On the Samsung phone'], rows, 'filter')
          + '<h3>Materials and textures</h3><p>There are <b>no texture image files</b> in the repository. For every avatar the engine bakes five maps '
          '(base colour, normal, metallic-roughness, occlusion, subsurface) from procedural material functions and embeds them as PNG in the GLB. '
          'One material per region; the exported file has 13 materials and 9–10 primitives.</p>'
          + table(['Region', 'Material function', 'Kind'], mrows, 'filter'))
section('params', 'Skin, colours, body and face', params)

# ----------------------------------------------------------------------------------------------- blendshapes
BS = 'browDownLeft browDownRight browInnerUp browOuterUpLeft browOuterUpRight cheekPuff cheekSquintLeft cheekSquintRight eyeBlinkLeft eyeBlinkRight eyeLookDownLeft eyeLookDownRight eyeLookInLeft eyeLookInRight eyeLookOutLeft eyeLookOutRight eyeLookUpLeft eyeLookUpRight eyeSquintLeft eyeSquintRight eyeWideLeft eyeWideRight jawForward jawLeft jawOpen jawRight mouthClose mouthDimpleLeft mouthDimpleRight mouthFrownLeft mouthFrownRight mouthFunnel mouthLeft mouthLowerDownLeft mouthLowerDownRight mouthPressLeft mouthPressRight mouthPucker mouthRight mouthRollLower mouthRollUpper mouthShrugLower mouthShrugUpper mouthSmileLeft mouthSmileRight mouthStretchLeft mouthStretchRight mouthUpperUpLeft mouthUpperUpRight noseSneerLeft noseSneerRight tongueOut'.split()
bs = ('<p>Every generated avatar carries <b>52 facial blendshapes</b> with the ARKit names, built by ' + src(f'{CPP}/src/af_rig.cpp', r'void Rig::buildBlendshapes', 'Rig::buildBlendshapes()')
      + ' and written to the GLB as sparse morph targets. The engine can apply weights (' + src(f'{CPP}/include/afcore.h', r'af_engine_apply_blendshapes', 'af_engine_apply_blendshapes') + ').</p>'
      + table(['Fact', 'Status'], [
          ['52 channels exist in the generated avatar and in its GLB', st('ok', 'counted by the engine and by the GLB test ' + ev('H2') + ' ' + ev('H1'))],
          ['The shapes look right (a blink closes the eye, a smile is a smile)', st('unv', 'no blendshape has been drawn with a non-zero weight since the face was rebuilt; the mouth is now closed by default, which the jaw and mouth shapes were not re-checked against')],
          ['A control in the app to move them', st('miss')],
          ['The native renderer applies them while drawing', st('miss', 'the renderer draws the stored vertices only')],
          ['Body morph targets on the bundled sample <code>female.glb</code> (7: muscle, weight, height, shoulder width, hip width, bust, limb length)', st('ok', 'present in the file ' + ev('G1')) + '<br>' + st('miss', 'no control in the app drives them')],
        ], 'filter')
      + details('The 52 names', '<p class="mono">' + ' · '.join(BS) + '</p>'))
section('blend', 'Morph targets and blendshapes', bs)

# ----------------------------------------------------------------------------------------------- animation
CLIPS = [(0, 'rest', 0.0, 'the bind pose', False), (1, 'idle', 4.0, 'looping motion', True), (2, 'breathe', 5.0, 'looping motion', True), (3, 'walk', 1.1, 'looping motion', True),
         (5, 'jog', 0.72, 'looping motion', True), (4, 'run', 0.62, 'looping motion', True), (6, 'squat', 2.4, 'motion', True), (7, 'jump', 1.6, 'motion, moves the root', True),
         (8, 'wave', 2.2, 'motion', True), (9, 'punch', 0.9, 'motion', True), (10, 'kick', 1.1, 'motion', True), (11, 't_pose', 0.1, 'a held pose', True), (12, 'a_pose', 0.1, 'a held pose', True),
         (13, 'sit', 0.1, 'a held pose', True), (14, 'turn_360', 2.0, 'motion', True), (15, 'pushup', 2.0, 'motion', True)]
rows = []
for cid, name, dur, kind, exported in CLIPS:
    rows.append([f'<b>{name}</b><br><code>AF_CLIP_{name.upper()} = {cid}</code>', f'{dur:.2f} s<br><small>{kind}</small>',
                 src(f'{CPP}/src/af_rig.cpp', rf'"{name}"', 'af_rig.cpp') + ' · ' + src(f'{CPP}/include/afcore.h', rf'AF_CLIP_{name.upper()}\b', 'enum'),
                 (st('ok', 'in the exported GLB, 30 Hz ' + ev('H3')) if exported else '<small>not exported: it is the bind pose</small>'),
                 st('ok', 'evaluated after import and compared with the generating engine at three times, within 0.2 mm ' + ev('H5')) if exported else st('ok', 'identity matrices ' + ev('H5')),
                 st('ok', 'one frame drawn on the host at 0.30 s ' + ev('H4')) if name == 'walk' else st('unv', 'no frame drawn'),
                 st('miss', 'no playback in the app')])
anim = ('<p>The sixteen native entries. They are <b>authored in C++</b> as keyframed joint rotations (' + src(f'{CPP}/src/af_rig.cpp', r'"idle"', 'the clip library')
        + '), not loaded from animation files. The order in an exported GLB is idle, breathe, walk, jog, run, squat, jump, wave, punch, kick, pushup, t_pose, a_pose, sit, turn_360.</p>'
        + table(['Clip', 'Length', 'Source', 'Written to the GLB', 'Engine plays it back correctly (host test)', 'A posed frame has been looked at', 'Visible playback in the app'], rows, 'filter')
        + '<p class="callout"><b>Read this row of facts together.</b> The clips exist, are exported, survive import and deform the mesh correctly in an automated test. '
          'One still frame of the walk has been drawn on the host' + ((', and on the CI emulator the production view showed a different picture once an animation was started ' + ev('E2')) if ANIM_OK else '') + '. '
          '<b>No clip has been watched playing on a phone.</b> The player and its controls are described under <a href="#systems">animation systems</a>.</p>')
section('anim', 'Animation inventory', anim)

# ----------------------------------------------------------------------------------------------- systems
def row5(what, source, ui, test, runtime, phone):
    return [what, source, ui, test, runtime, phone]
FIL = st('src', 'integration commits 749ec1e…8b80b07') 
rows = [
 row5('<b>Skeleton</b>: 71 joints, parent links, rest pose', st('ok', src(f'{CPP}/src/af_rig.cpp', r'void Rig::build\w*\(', 'af_rig.cpp')), '<small>n/a</small>', st('ok', ev('H5') + ' ' + ev('H1')), st('ok', 'listed from the engine ' + ev('H3')), st('part', 'the exported file carried 71 joints; the renderer reported 0 because import dropped them (old build) ' + ev('P1'))),
 row5('<b>Skin weights</b>: 4 joints per vertex', st('ok', src(f'{CPP}/src/af_rig.cpp', r'computeSkinWeights|skinWeights|void Rig::skin', 'af_rig.cpp')), '<small>n/a</small>', st('ok', 'weights normalised, every vertex weighted ' + ev('H5')), st('ok', '621,802 of 621,802 vertices weighted ' + ev('H3')), st('unv')),
 row5('<b>Animation clips</b> (the 15 above)', st('ok'), st('src', 'a strip of them under the avatar and the Motion section ' + src(f'{KT_APP}/AvatarStudioScreen.kt', r'animation strip', 'AvatarStudioScreen.kt')), st('ok', ev('H5')), (st('ok', 'listed and one played through the production view ' + ev('E2')) if ANIM_OK else st('part', 'one posed frame drawn ' + ev('H4'))), st('unv')),
 row5('<b>Keyframes</b>: 30 Hz rotation tracks, one root translation track', st('ok'), '<small>n/a</small>', st('ok', '708 rotation keys read from the file equal engine playback ' + ev('H5')), st('ok', ev('H5')), st('unv')),
 row5('<b>Export</b>: skin, joints, inverse bind matrices, clips in the GLB', st('ok', src(f'{CPP}/src/af_glb.cpp', None, 'af_glb.cpp')), st('ok', 'every generation exports'), st('ok', ev('H5') + ' ' + ev('H1')), st('ok', ev('H2')), st('ok', 'the phone wrote an 83 MB GLB with the old generator ' + ev('P1'))),
 row5('<b>Re-import of an AvatarForge GLB</b> keeps skeleton and clips', st('ok', src(f'{CPP}/src/af_engine.cpp', r'adoptImportedRig', 'af_engine.cpp: adoptImportedRig') + ' since b636f23'), '<small>automatic</small>', st('ok', ev('H5')), st('ok', '71 joints, 15 animations after import ' + ev('H3')), st('unv', 'the published APK has not been run on the phone')),
 row5('<b>Import of a foreign rigged GLB</b> (other skeleton)', st('ok', 'imports as a static model, by design, with a warning'), '<small>automatic</small>', st('ok', ev('H5') + ' ' + ev('H1')), st('ok', 'the prepared avatar: 0 joints kept ' + ev('H6')), st('ok', 'the prepared avatar was displayed ' + ev('P1'))),
 row5('<b>Pose evaluation</b>: skin matrices for a clip and a time', st('ok', src(f'{CPP}/include/afcore.h', r'^int32_t af_engine_evaluate_animation', 'af_engine_evaluate_animation') + ' since b636f23'), st('src', 'called by the renderer for every frame of an animation ' + src(f'{CPP}/src/render/af_gl.cpp', r'^void GlRenderer::updateSkinnedAnimation', 'updateSkinnedAnimation')), st('ok', ev('H5')), st('ok', ev('H5') + ((' ' + ev('E2')) if ANIM_OK else '')), st('unv')),
 row5('<b>CPU skinning</b> of the whole mesh', st('ok', src(f'{CPP}/src/af_engine.cpp', r'^int32_t af_engine_apply_clip', 'af_engine_apply_clip')), '<small>used by the clip player below</small>', st('ok', ev('H5')), st('ok', 'a posed frame on the host ' + ev('H4') + '; on the CI emulator one pose of the whole mesh took ' + EMU_POSE + ' ' + ev('E1')), st('unv', 'cost per pose not measured on the phone')),
 row5('<b>CPU clip player in the native renderer</b>: re-poses the mesh on the CPU every frame and uploads the vertices again', st('src', src(f'{CPP}/src/render/af_render_capi.cpp', r'^void af_renderer_set_clip', 'af_renderer_set_clip') + ' · ' + src(f'{CPP}/src/render/af_gl.cpp', r'^void GlRenderer::updateAnimation', 'GlRenderer::updateAnimation') + ' · ' + src(f'{CPP}/src/render/af_render.h', r'virtual void setClip', 'Renderer::setClip')), st('miss', 'left unconnected on purpose: it uploads the whole vertex buffer every frame, 55 MB for the default avatar. The app uses GPU skinning, next row'), st('miss'), st('unv', 'never called'), st('unv')),
 row5('<b>GPU skinning in the native renderer</b>: the bind-pose vertices stay on the GPU, one skin matrix per joint is uploaded each frame (8 KB)', st('ok' if ANIM_OK else 'src', src(f'{CPP}/src/render/af_gl.cpp', r'^void GlRenderer::updateSkinnedAnimation', 'af_gl.cpp') + ' · ' + src(f'{CPP}/src/render/af_shaders_gl.h', r'uniform AfSkinBlock', 'forward.vert and shadow.vert') + ' · ' + src(f'{CPP}/src/render/af_render.cpp', r'bool supportsSkinnedPlayback\(\) const override', 'the renderer wrapper') + '<br><small>the two vertex shaders were edited by hand in a generated header and compiled with glslang on the host</small>'), st('src', 'every animation control uses it'), st('ok' if ANIM_OK else 'src', 'instrumented ' + src('app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt', r'animatedFramesPresented', 'GeneratedAvatarRuntimeTest')), (st('ok', 'CI emulator ' + ev('E2')) if ANIM_OK else st('unv')), st('unv')),
 row5('<b>Playback controls, native path</b>: select, play, pause, stop, loop, speed, seek', st('src', src(f'{CPP}/src/render/af_render.h', r'virtual bool  setAnimation', 'Renderer::setAnimation') + ' · ' + src(f'{CPP}/avatarforge_renderer_jni.cpp', r'AvatarForgeSurfaceView_nativeSetAnimation\(', 'JNI') + ' · ' + src(f'{KT_ENGINE}/AvatarForgeSurfaceView.kt', r'fun playAnimation', 'AvatarForgeSurfaceView.playAnimation')), st('src', 'the strip under the stage and the Motion section: select, play / pause, stop, loop, speed. Seek exists in the view but has no slider'), (st('part', 'selecting an animation is tested ' + ev('E2') + '; pause, loop, speed and seek are not') if ANIM_OK else st('miss')), (st('part', 'select only ' + ev('E2')) if ANIM_OK else st('unv')), st('unv')),
 row5('<b>Playback of an imported GLB\'s own clips</b> (Filament gltfio Animator)', FIL + ' ' + src(f'{KT_APP}/FilamentPreviewView.kt', r'fun selectAnimation', 'FilamentPreviewView.kt'), st('src', 'previous / play-pause / stop-reset / next, seek slider, loop, 0.5–2× ' + src(f'{KT_APP}/AvatarStudioScreen.kt', r'Animations in this file ·', 'AvatarStudioScreen.kt')), st('part', 'which preview a file opens in, and which files that preview refuses, are unit-tested (GlbPreviewRoutingTest); playback itself has no test'), st('unv', 'never run with a GLB that has clips'), st('unv')),
 row5('<b>Timeline</b>', FIL + ' (seek slider, Filament path) · native path: the position is reported, no slider', st('src'), st('miss'), st('unv'), st('unv')),
 row5('<b>Looping, playback speed</b>', FIL + ' (Filament path) · ' + st('src', 'native path, since 210ae9a'), st('src'), st('miss'), st('unv'), st('unv')),
 row5('<b>Animation blending</b> (cross-fade between clips)', st('miss'), st('miss'), st('miss'), st('miss'), st('miss')),
 row5('<b>Retargeting</b> (engine clips onto another skeleton)', st('miss'), st('miss'), st('miss'), st('miss'), st('miss')),
 row5('<b>Facial animation</b> (blendshape tracks)', st('part', 'weights can be applied; no clip contains blendshape tracks'), st('miss'), st('unv'), st('unv'), st('miss')),
 row5('<b>Animation controller / state machine</b>', st('miss'), st('miss'), st('miss'), st('miss'), st('miss')),
]
systems = ('<p>Five separate questions for each system, because they have five different answers.</p>'
           + table(['System', 'Implemented in source', 'Connected to UI', 'Automated test', 'Seen at runtime (host or emulator)', 'On the Samsung SM-S921B'], rows, 'filter')
           + '<h3>How an avatar moves, in one paragraph</h3><p>A generated avatar is drawn by the native GLES renderer. Its vertex buffer already carried four joints and four weights per vertex; '
             'since <code>210ae9a</code> the two vertex shaders read them and blend each vertex by a uniform block of one skin matrix per joint, which the engine fills for the chosen animation and time '
             '(<code>af_engine_evaluate_animation</code>). The renderer keeps the clock. Playing an animation therefore uploads 8 KB a frame, not the mesh. '
             'While one plays, nothing is culled by bind-pose bounds and the clip planes and shadow map are fitted to wider bounds. The app lists the animations the renderer reports, starts one from a strip under the avatar, and has play / pause, stop, loop and speed in the Motion section. '
             + (('On the CI emulator the production view listed ' + str(ANIM.get('clips', '?')) + ' animations, played <code>' + E(str(ANIM.get('played', '?'))) + '</code>, presented ' + str(ANIM.get('frames', '?')) + ' frames without a GL error and the picture changed by ' + f"{int(ANIM.get('changedPixels', 0)):,}" + ' pixels ' + ev('E2') + '. That proves the path and the picture; a software emulator at one frame every several seconds says nothing about smoothness. ') if ANIM_OK else 'No run has shown it yet. ')
             + 'The first build of this, <code>210ae9a</code>, listed no animations at all: the renderer the app talks to is a wrapper that forwards each call to the GLES backend, and the new calls were not forwarded. The instrumented test caught it; <code>a5e93a8</code> forwards them. '
             'The older CPU player (<code>af_renderer_set_clip</code>) is still in the source and still unconnected: it re-poses and re-uploads every vertex every frame. '
             'An imported GLB with its own clips goes to the Filament preview instead, where the controls exist in source but have not been run. '
             'A generated avatar must never go there: Filament\'s loader sizes its morph-target storage for every primitive, vertex and target at once, which for these avatars is several hundred megabytes. '
             'Until <code>210ae9a</code> a button offered exactly that, and it is the likely cause of the crash the owner reported. ' + src(f'{KT_APP}/GlbPreviewRouting.kt', r'fun animationPreviewRefusal', 'GlbPreviewRouting.animationPreviewRefusal') + ' now decides, and the preview refuses such a file itself.</p>')
section('systems', 'Animation systems: source, UI, tests, runtime, phone', systems)

# ----------------------------------------------------------------------------------------------- rig
JOINTS = EXTRA.get('joints', [])
jrows = [[str(i), f'<code>{n}</code>', (f'<code>{JOINTS[p][0]}</code>' if p >= 0 else '—')] for i, (n, p) in enumerate(JOINTS)]
rig = ('<p>The AvatarForge skeleton, read back from the engine after importing a generated GLB ' + ev('H3') + '. 71 joints: a root and hips, five spine joints, two neck joints, head, jaw and two eyes; '
       'per arm a clavicle, two upper-arm and two forearm segments, a hand, two twist joints and 16 finger joints; per leg thigh, shin, foot, toe and a thigh twist joint.</p>'
       + table(['Property', 'Value', 'Status'], [
           ['Joints', '71', st('ok', ev('H3'))], ['Rest pose', 'A-pose: arms 12° from the body, 14° on the female body ' + src(f'{CPP}/src/af_field.cpp', r'const float abduct = radians', 'af_field.cpp'), st('ok', ev('H4'))],
           ['Vertex influences', '4 per vertex, 16-bit joint indices, float weights', st('ok', ev('H5'))],
           ['Inverse bind matrices', '71, written to the GLB', st('ok', ev('H5'))],
           ['Joint naming', 'lower case with <code>_L</code> / <code>_R</code> suffix', st('ok', ev('H3'))],
           ['Compatibility with other humanoid rigs (Mixamo, VRM, Unity Humanoid)', 'no mapping table exists', st('miss')],
         ], 'filter')
       + details(f'All {len(JOINTS)} joints with their parents', table(['#', 'Joint', 'Parent'], jrows, 'filter compact')))
section('rig', 'Rig and skeleton', rig)

# ----------------------------------------------------------------------------------------------- GLB assets
rows = [
 ['<b>female.glb</b><br><small>bundled sample</small>', src('app/src/main/assets/models/female.glb', None, 'app/src/main/assets/models/female.glb') + f'<br><a href="{REPO}/raw/{CODE_SHA}/app/src/main/assets/models/female.glb">download</a>',
  '418,312 B<br><small class="mono">sha256 f2002ff4…bae779</small>', '1 mesh, 3,014 vertices, 5,520 triangles, 2 materials (Skin, Cyberware), <b>7 body morph targets</b>, 0 skins, 0 animations, 0 images. Generator string "3DoubleD mesh forge".',
  st('ok', 'file inspected ' + ev('G1')), 'A static fixture for the Filament preview. Not a wardrobe, not an animation pack. <b>Provenance</b> ' + src('THIRD_PARTY_NOTICES.md', r'Female base-mesh fixture', 'THIRD_PARTY_NOTICES.md') + ': from <code>davealone69-gif/3DDD</code> at <code>be5bf17</code>, whose README releases its generated base bodies under CC0 (read for this document). At the code commit the notice called the file unchanged with a matching Git blob; commit <code>98dd9ec</code> had re-written its header, so it is not byte-identical: see ' + ev('G1') + '. The notice is corrected in the commit that adds this document.'],
 ['<b>Generated avatar</b><br><small>made on demand, never stored in Git</small>', 'written by ' + src(f'{CPP}/src/af_glb.cpp', None, 'af_glb.cpp') + ' to app-private storage',
  '66,730,796 B on the host for the diagnostic spec<br><small class="mono">sha256 02986fb8…1c7909</small>', '623,493 vertices, 1,148,034 triangles, 9 primitives, 13 materials, 5 PNG maps, 1 skin with 71 joints, 52 sparse morph targets, 15 animations.',
  st('ok', ev('H2') + ' ' + ev('H3')), 'What the app\'s "Generate" produces. The phone\'s file of 2026-11-15 was 83,430,028 B from the older generator ' + ev('P1') + '.'],
 ['<b>mylove_rig_prepared.glb</b><br><small>owner\'s prepared avatar</small>', 'private Dropbox: <code>/3d-mega-hub-diagnostics/3d-mega-hub/mylove_rig_prepared.glb</code><br><small>not in Git, not linked: it is a private file</small>',
  '13,221,520 B<br><small class="mono">sha256 656cf244…576d14</small>', '1 mesh, 163,756 vertices, 144,073 triangles, 1 material with two 2048² WebP maps, 1 skin with 20 joints, <b>0 animations</b>, 0 morph targets.',
  st('ok', 'retrieved and inspected byte for byte ' + ev('H6')), 'Full report: <a href="#prepared">prepared avatar</a>. The original on Dropbox was only read.'],
 ['<b>mylove.glb</b><br><small>earlier static version</small>', 'owner\'s device / earlier upload', '9,287,272 B<br><small class="mono">sha256 8ef94a00…426a26</small>', 'Static mesh.', st('unv', 'size and hash as recorded in ' + src('docs/AVATAR_GEOMETRY_EVIDENCE.md', r'8ef94a00', 'AVATAR_GEOMETRY_EVIDENCE.md') + '; the file was not re-read for this document'), 'Superseded by the prepared version as the reference.'],
]
section('glb', 'GLB files', '<p>Every model file that exists for this project, in the repository or outside it. There are no glTF, FBX, OBJ, BVH, VRM or USD files anywhere in the repository; '
        'the only image files in it are the evidence renders under <code>docs/evidence/</code> ' + ev('G2') + '. Model files in the owner\'s other repositories are listed under <a href="#donors">donor projects</a>.</p>' + table(['File', 'Where', 'Size', 'Contents', 'Status', 'Note'], rows, 'filter'))

# ----------------------------------------------------------------------------------------------- prepared avatar
PJ = [('Root', '—', 'root', 'at the feet'), ('Hips', 'Root', 'hips', ''), ('Spine', 'Hips', 'spine_01 … spine_02', 'one joint where the engine has several'), ('Chest', 'Spine', 'spine_03 … chest', ''),
      ('Neck', 'Chest', 'neck_01, neck_02', ''), ('Head', 'Neck', 'head', 'no jaw, no eyes'), ('LeftShoulder', 'Chest', 'clavicle', 'see the left/right note'), ('LeftUpperArm', 'LeftShoulder', 'upperarm_01, upperarm_02', 'no twist joints'),
      ('LeftLowerArm', 'LeftUpperArm', 'forearm_01, forearm_02', ''), ('LeftHand', 'LeftLowerArm', 'hand', 'no fingers'), ('RightShoulder', 'Chest', 'clavicle', ''), ('RightUpperArm', 'RightShoulder', 'upperarm_01, upperarm_02', ''),
      ('RightLowerArm', 'RightUpperArm', 'forearm_01, forearm_02', ''), ('RightHand', 'RightLowerArm', 'hand', 'no fingers'), ('LeftUpperLeg', 'Hips', 'thigh', ''), ('LeftLowerLeg', 'LeftUpperLeg', 'shin', ''),
      ('LeftFoot', 'LeftLowerLeg', 'foot', 'no toe'), ('RightUpperLeg', 'Hips', 'thigh', ''), ('RightLowerLeg', 'RightUpperLeg', 'shin', ''), ('RightFoot', 'RightLowerLeg', 'foot', 'no toe')]
facts = [
 ['Container', 'glTF 2.0 binary; declared length equals file length; JSON chunk 4,640 B, binary chunk 13,216,852 B', st('ok')],
 ['Written by', '<code>trimesh</code> (asset.generator)', st('ok')],
 ['What the file says about itself', '<code>asset.extras.animationReadiness</code>: status <b>"approximate_unverified"</b>; method "local estimated biped landmarks and smooth nearest-segment weights"; <code>animationClips: 0</code>; note <i>"Starting-point skin only. Rig deformation and retargeting have not been tested."</i>', st('ok')],
 ['Meshes', '1 mesh, 1 primitive: 163,756 vertices, 144,073 triangles; position, normal, one UV set, joints (16-bit), weights (float). No tangents, no vertex colours.', st('ok')],
 ['Size and placement', 'Fits a box 0.80 × 1.00 × 0.36: <b>one unit tall, centred on the origin</b>, not metres and not standing on the ground. T-pose. Faces +Z.', st('ok')],
 ['Materials', '1 opaque PBR material: base-colour map and metallic-roughness map, factors 1.0. No normal map.', st('ok')],
 ['Textures', '2 images, both <b>WebP</b>, 2048 × 2048 (1,876,952 B and 439,408 B). The file <b>requires</b> the extension <code>EXT_texture_webp</code>.', st('ok')],
 ['Skeleton', '1 skin, 20 joints (table below). Joints have translations only; the 20 inverse bind matrices agree with the joint positions exactly.', st('ok')],
 ['Skin weights', 'Every vertex weighted, weights sum to 1, four non-zero weights on every vertex, of which 3.86 on average are above 0.0001. Very soft: for 43.6 % of vertices the strongest joint has less than 0.6 of the weight, for 92 % less than 0.9.', st('ok')],
 ['Which joints own the mesh', 'Chest 31,664 vertices · Neck 22,253 · left shoulder 15,591 · lower legs 14,787 and 13,714 · … · upper arms 1,172 and 1,088 · lower arms 405 and 464 · <b>hands 38 and 36</b>. The sleeves and the hair hang from chest, neck and shoulders; the arms own almost nothing.', st('ok')],
 ['Animation clips', '<b>0</b>', st('ok')],
 ['Morph targets', '0: no facial animation is possible with this file', st('ok')],
]
compat = [
 ['Opens and is drawn as a static model by the native renderer', st('ok', 'on the phone: 144,073 triangles, frames presented ' + ev('P1') + '; on the host: imported and drawn textured ' + ev('H6'))],
 ['Its WebP textures are decoded by the native importer', st('ok', '2048² base colour present after import ' + ev('H6') + '; an Android test covers a required-WebP file')],
 ['Its WebP textures are decoded by the Filament preview', st('unv', 'the extension is <i>required</i> by the file; whether the bundled Filament build supports it was not checked')],
 ['Its skeleton is adopted by the AvatarForge engine', st('ok', '<b>No</b>, and correctly so: after import the engine holds 0 joints and 0 animations ' + ev('H6') + '. The engine adopts only its own 71-joint skeleton.')],
 ['AvatarForge clips (walk, wave, …) can be played on it', st('miss', 'needs retargeting, which does not exist: 20 joints with other names, another rest pose, another scale')],
 ['It can test the Filament animation controls', st('ok', '<b>No</b>: it has no clips, so the controls never appear. It opens in the native preview ' + src(f'{KT_APP}/GlbPreviewRouting.kt', r'fun opensInAnimationPreview', 'GlbPreviewRouting.kt'))],
 ['Its skin would deform well if animated', st('unv', 'its own metadata says untested; the weight distribution above suggests the arms, sleeves and hair would not follow')],
]
prepared = ('<p>The file was fetched from the owner\'s Dropbox with the connected account, saved outside the repository, made read-only and inspected. '
            'Nothing was written back to Dropbox. SHA-256 <code class="mono">656cf2446eefbffc92cf569ce1a2f52b6d0ad449c282fc5a4d911abfef576d14</code>, 13,221,520 bytes: '
            'the same file the phone imported on 2026-11-15 ' + ev('P1') + '.</p>'
            '<h3>What is in it</h3>' + table(['', 'Finding', 'Status'], facts, 'filter')
            + '<h3>Compatibility with the app and the AvatarForge animation engine</h3>' + table(['Question', 'Answer'], compat, 'filter')
            + '<h3>Its 20 joints and the nearest AvatarForge joints</h3>'
            + table(['Joint in the file', 'Parent', 'Nearest AvatarForge joint(s)', 'Note'], [[f'<code>{a}</code>', f'<code>{b}</code>' if b != '—' else '—', f'<code>{c}</code>', d] for a, b, c, d in PJ], 'filter compact')
            + '<p class="callout"><b>Left and right are swapped in the names.</b> The figure faces +Z, so its own left side is at +X. The joints named <code>Left…</code> are at −X: '
              'they are on the figure\'s right. They were named from the viewer\'s side. Any mapping onto another rig has to swap them, or a walk will swing the wrong arm with each leg.</p>'
            + '<h3>What it would take to animate this avatar</h3><ol>'
              '<li><b>Decide the route.</b> Either the native renderer learns to skin a foreign skeleton, or clips are baked into a copy of the file so the Filament preview can play them. The first serves every imported rig; the second is quicker for this one file.</li>'
              '<li><b>A retarget map</b> from the engine\'s 71 joints to these 20 (the table above is its starting point), with the left/right swap and the rest poses reconciled: this file is in a T-pose, the engine rests in an A-pose.</li>'
              '<li><b>Better weights.</b> Hands that own 36 vertices cannot move a hand. The sleeves, skirt and hair need weights that follow the limbs, or simulation, or to be left rigid on purpose.</li>'
              '<li><b>Then look at it moving</b>, on the phone, before anything is called working.</li></ol>'
            + details('How the inspection was done', '<p>The JSON chunk was parsed and every accessor read with NumPy: joint hierarchy and translations from the nodes; inverse bind matrices compared with the accumulated joint positions; '
                      'weights summed per vertex and per joint; image headers decoded for format and size. The file was then imported through the engine\'s own importer with <code>af_host_render --report</code>, '
                      'which printed vertices=163756 triangles=144073 submeshes=1, engine joints=0, skin joints=0, animations=0, base-colour texture 2048x2048, and drew the model. '
                      'The render shows a figure with long dark red hair in a black dress with wide sleeves, arms out; it is not included here because the asset is private.</p>'))
section('prepared', 'Prepared avatar: mylove_rig_prepared.glb', prepared)

# ----------------------------------------------------------------------------------------------- donors
GH = 'https://github.com/davealone69-gif'
drows = []
for d in DONORS.get('entries', []):
    link = f'{GH}/{d["repo"]}/blob/{d["head"]}/{d["path"]}'
    raw = f'{GH}/{d["repo"]}/raw/{d["head"]}/{d["path"]}'
    also = ''.join(f'<br><small>also: {E(a)}</small>' for a in d.get('also', []))
    drows.append([f'<b>{E(d["path"].split("/")[-1])}</b><br><small><a href="{GH}/{d["repo"]}" target="_blank">{E(d["repo"])}</a> @ <code>{E(d["head"][:7])}</code></small>',
                  f'<a class="src" href="{link}" target="_blank">{E(d["path"])}</a><br><a href="{raw}">download</a>{also}',
                  f'{d["bytes"]:,} B<br><small class="mono">sha256 {E(d["sha256"][:8])}…{E(d["sha256"][-6:])}</small>',
                  E(d['what']), st(d['lic_st'], E(d['licence'])), E(d['use'])])
donors = ('<p>The owner\'s other repositories were searched for anything this project could reuse ' + ev('D1') + '. <b>None of them holds a wardrobe:</b> no hair, clothing, skin or texture library exists anywhere on the account, and no animation files. '
          'What does exist is a handful of GLB files, and four of them matter here because they carry what this repository lacks: a skeleton with a clip that can be played. They are listed first.</p>'
          + table(['File', 'Where', 'Size', 'What is in it (inspected)', 'Licence and provenance', 'What it is good for'], drows, 'filter')
          + '<p class="callout"><b>Status of all of these in this project:</b> ' + st('ok', 'inspected') + ' ' + st('unv', 'none has been brought into this repository or opened in the app') + '. '
          'Download links go to the owner\'s public repositories at the commit that was inspected. Nothing here was copied, and nothing should be without checking its licence line above.</p>'
          + '<h3>Repositories with avatar-related names and no model files</h3><p>' + ', '.join(f'<a href="{GH}/{E(n)}" target="_blank">{E(n)}</a>' for n in DONORS.get('no_models', []))
          + '. Their default branches contain no model, animation or environment files ' + ev('D1') + '. Their source code was not read for this document; the README section carried over from the integration branch describes <code>3d4me</code> as a procedural body without a rig and <code>Aura-Studio-Avatar</code>\'s animation interface as a logging stub ' + st('unv', 'owner\'s findings, not re-checked here') + '.</p>'
          + '<h3>Not visible to the connected account</h3><p>' + ', '.join(E(n) for n in DONORS.get('not_visible', [])) + ' ' + st('blk', 'not in the list of repositories this GitHub connection can see; nothing can be said about their contents') + '.</p>')
section('donors', 'Donor projects and reusable assets', donors)

# ----------------------------------------------------------------------------------------------- UI
rows = [
 ['Import a GLB from the device', src(f'{KT_APP}/AvatarStudioScreen.kt', r'onClick = onImport', 'AvatarStudioScreen.kt'), st('ok', 'used on the phone ' + ev('P1'))],
 ['Download a GLB from an HTTPS or Dropbox link', src(f'{KT_APP}/AvatarStudioScreen.kt', r'HTTPS or Dropbox \.glb link', 'AvatarStudioScreen.kt'), st('src', 'not exercised for this document')],
 ['"Use sample model"', src(f'{KT_APP}/AvatarStudioScreen.kt', r'Use sample model', 'AvatarStudioScreen.kt'), st('ok', 'emulator smoke test in CI ' + ev('C1'))],
 ['<b>The creator</b>: sections Look, Body, Hair, Face, Skin, Outfit, Shoes and Motion on a rail beside the avatar; a picture of every preset with its name written over it, colour tiles and dots, sliders; Randomize, Save, Create avatar; a dark look (the default) and a light one, switched in the header', src(f'{KT_APP}/AvatarStudioScreen.kt', r'^fun AvatarStudioScreen', 'AvatarStudioScreen.kt') + ' · ' + src(f'{KT_APP}/AvatarCreatorPanels.kt', r'^enum class CreatorCategory', 'AvatarCreatorPanels.kt') + ' · ' + src(f'{KT_APP}/AppearanceControls.kt', r'^data class AppearanceSelection', 'AppearanceSelection'), st('src', 'laid out after ' + GUIDE + ' in 210ae9a; the choices of the owner\'s second reference (ethnicity; body type, breast size, butt size; hair colour and style) and its dark look added in 2920265; compiled and linted, the choice-to-spec mapping unit-tested, and the emulator smoke test opens the screen and presses its diagnostic button ' + ev('C3') + '; <b>never seen drawn, on a phone or anywhere else</b>. The first guide\'s Accessories and Background sections are not there: the engine has no options for them')],
 ['Prompt box and "Generate avatar"', src(f'{KT_APP}/AvatarStudioScreen.kt', r'Describe your avatar', 'AvatarStudioScreen.kt'), st('ok', 'generation ran on the phone with the older build ' + ev('P1'))],
 ['Cancel generation', src(f'{KT_APP}/AvatarStudioScreen.kt', r'generationJob\?\.cancel\(\)', 'AvatarStudioScreen.kt'), st('src')],
 ['Orbit, pinch, double-tap on the preview; turn, zoom and reset buttons over the stage', src(f'{KT_ENGINE}/AvatarForgeSurfaceView.kt', r'fun zoomBy', 'AvatarForgeSurfaceView.kt') + ' · ' + src(f'{KT_APP}/AvatarStudioScreen.kt', r'StageButton\(', 'AvatarStudioScreen.kt'), st('part', 'gestures used on the phone ' + ev('P1') + '; the buttons are new and unrun')],
 ['Switch between the engine\'s renderer and the glTF viewer (the Filament preview)', src(f'{KT_APP}/AvatarStudioScreen.kt', r'Open in the glTF viewer', 'AvatarStudioScreen.kt') + ' · ' + src(f'{KT_APP}/GlbPreviewRouting.kt', r'fun mayOfferAnimationPreview', 'GlbPreviewRouting.kt'), st('src', 'offered only for a file that viewer can hold, never for an avatar made here; the rule is unit-tested ' + ev('C3') + ' and was widened in <code>7e4f0cb</code>, next row')],
 ['Imported model with several colour textures or transparency: opens in the glTF viewer by itself; textures reduced at import to a 448 MiB budget; orbit, pinch and double-tap in that viewer; ambient light', src(f'{KT_APP}/GlbPreviewRouting.kt', r'fun needsGltfViewer', 'GlbPreviewRouting.kt') + ' · ' + src(f'{KT_APP}/GlbTextureBudget.kt', r'^    fun rewrite', 'GlbTextureBudget.kt') + ' · ' + src(f'{KT_APP}/AndroidGlbImageScaler.kt', r'override fun scale', 'AndroidGlbImageScaler.kt') + ' · ' + src(f'{KT_APP}/ImportedGlbPreparation.kt', r'fun fitTextures', 'ImportedGlbPreparation.kt') + ' · ' + src(f'{KT_APP}/FilamentPreviewView.kt', r'private fun applyOrbitCamera', 'FilamentPreviewView.kt'), st('src', 'new in <code>7e4f0cb</code>, after three reference models from the owner showed 2 of 14 and 4 of 45 parts textured in the engine\'s renderer (<a href="OWNER_MODELS_2026-11-15.md">record</a>). The routing rule, the budget and the GLB rewrite have JVM unit tests (GlbPreviewRoutingTest, GlbTextureBudgetTest), passed in <a href="https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43719099722">Android CI run 43719099722</a>; the bitmap scaler, the gestures and the light have none. The build that has it: <a href="https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-7e4f0cb">phone-test-7e4f0cb</a>. Not run on a phone')],
 ['glTF viewer picture: 4x MSAA and FXAA, ambient occlusion, a shadow-casting key light, the PBR Neutral tone curve, the stage colour as background. Imported textures at their authored brightness. JPEG texture test', src(f'{KT_APP}/FilamentPreviewView.kt', r'private fun configureViewQuality', 'FilamentPreviewView.kt: configureViewQuality') + ' · ' + src(f'{KT_APP}/FilamentPreviewView.kt', r'private fun applyToneCurveAndBackground', 'applyToneCurveAndBackground') + ' · ' + src(f'{CPP}/src/af_glb_read.cpp', r'glTF 2\.0: an absent baseColorFactor', 'af_glb_read.cpp: baseColorFactor') + ' · ' + src('app/src/androidTest/java/com/davemega/hub/AvatarForgeRendererSmokeTest.kt', r'fun jpegTextureIsDecodedAndRenderedByAvatarForge', 'JPEG texture test'), st('src', 'new in <code>50bbe25</code> and <code>96152cd</code>, after a second folder of reference models (<a href="OWNER_MODELS_2026-11-15.md">record</a>, section 6). The JPEG test passed on the CI emulator in <a href="https://github.com/davealone69-gif/3d-mega-hub/actions/runs/43725536116">Android CI run 43725536116</a>; the brightness fix is covered by the host test imported_glb_pbr_test; the viewer picture has no test and was judged by eye from emulator screenshots of the bundled figure. The build that has it: <a href="https://github.com/davealone69-gif/3d-mega-hub/releases/tag/phone-test-96152cd">phone-test-96152cd</a>. Not run on a phone')],
 ['Animation controls (Filament preview only): previous, play / pause, stop / reset, next, seek, loop, speed', src(f'{KT_APP}/AvatarStudioScreen.kt', r'Animations in this file ·', 'AvatarStudioScreen.kt'), st('src', 'appear only for a file with clips; never seen')],
 ['Measured renderer diagnostics under the preview', src(f'{KT_APP}/AvatarStudioScreen.kt', r'Measured renderer diagnostics', 'AvatarStudioScreen.kt'), st('ok', 'phone ' + ev('P1'))],
 ['Debug build: "generated avatar diagnostic" button', src(f'{KT_APP}/AvatarStudioScreen.kt', r'diagnostic-seed-1337', 'AvatarStudioScreen.kt'), st('ok', 'phone ' + ev('P1'))],
 ['Animation controls for a generated avatar: a strip of its animations under the stage; Motion section with play / pause, stop, loop, speed', src(f'{KT_APP}/AvatarStudioScreen.kt', r'animation strip', 'AvatarStudioScreen.kt') + ' · ' + src(f'{KT_ENGINE}/AvatarForgeSurfaceView.kt', r'fun playAnimation', 'AvatarForgeSurfaceView.kt'), (st('part', 'starting an animation is proved through the production view on the CI emulator ' + ev('E2') + '; no test presses the buttons; never run on a phone') if ANIM_OK else st('src'))], ['Blendshape / expression controls', '—', st('miss')], ['Save: hand the avatar\'s GLB to the share sheet', src(f'{KT_APP}/AvatarStudioScreen.kt', r'model/gltf-binary', 'AvatarStudioScreen.kt') + ' · ' + src('app/src/main/res/xml/file_provider_paths.xml', r'files-path', 'file_provider_paths.xml') + ' · ' + src(f'{KT_ENGINE}/AvatarForgeEngine.kt', r'File\(appContext.filesDir, "avatars"\)', 'AvatarForgeEngine.kt'), st('src', 'since 210ae9a; never pressed on a device')],
]
section('ui', 'Controls in the app, and what they are connected to', table(['Control', 'Source', 'Status'], rows, 'filter'))

# ----------------------------------------------------------------------------------------------- verification matrix
rows = [
 ['Generate an avatar from a prompt', st('ok', ev('H2')), st('ok', 'current generator ' + ev('E1')), st('ok', 'older build ' + ev('P1'))],
 ['Create an avatar from the creator\'s choices', st('ok', 'the same spec values, by command line ' + ev('H7')), st('src', 'compiled; the mapping from picker to spec is unit-tested ' + ev('C3') + '; no instrumented test taps the cards'), st('unv')],
 ['Each hair style produces an avatar', st('ok' if MX else 'unv', ev('H7')), st('unv'), st('unv')],
 ['Each outfit and footwear produces an avatar', st('ok' if MX else 'unv', ev('H7')), st('part', 'one outfit: long hair and dress ' + ev('E1')), st('unv')],
 ['Each preset looks like what it is named', st('ok', f'all {len(MX)} (14 hair, 10 outfits, 5 footwear), judged from the host renders; the limits of each are written beside it ' + ev('H7')), st('unv'), st('unv')],
 ['The avatar looks right (face, eyes, hairline, garment edges)', st('part', 'default avatar drawn and inspected; known defects listed under outstanding work ' + ev('H4')), st('unv', 'the run saves a screenshot as an artifact; it was not viewed for this document'), st('unv', 'the phone has only seen the older generator')],
 ['Export to GLB (app-private file)', st('ok', ev('H2')), st('ok', ev('E1')), st('ok', 'older build ' + ev('P1'))],
 ['Save the avatar where the user can reach it', '<small>n/a</small>', st('src', 'Save opens the share sheet; no test presses it'), st('unv')],
 ['Import the exported GLB with skeleton and clips intact', st('ok', ev('H3') + ' ' + ev('H5')), (st('ok', '71 joints ' + ev('E1') + ' and its animations listed by the renderer ' + ev('E2')) if ANIM_OK else st('part', 'the renderer reports 71 joints after import ' + ev('E1'))), st('unv', 'the old build reported 0 joints ' + ev('P1') + '; no newer result sent')],
 ['Draw a generated avatar (static)', st('ok', 'software render ' + ev('H4')), st('ok', 'current generator, pixels read back ' + ev('E1')), st('ok', 'older build: 1,100,168 triangles, 53.8 FPS ' + ev('P1'))],
 ['Draw an imported GLB (static)', st('ok', ev('H6')), st('ok', 'imported-GLB render smoke test ' + ev('C3')), st('ok', 'prepared avatar ' + ev('P1'))],
 ['Play an animation on a generated avatar', st('part', 'computed and tested, one frame drawn ' + ev('H5') + ' ' + ev('H4')), (st('ok', 'one held pose through the production view: the picture changed ' + ev('E2')) if ANIM_OK else st('src', 'in source; no passing run')), st('unv', 'the build that can do it has not been run on the phone')],
 ['Play an imported GLB\'s own clips', st('unv'), st('unv'), st('unv')],
 ['Play, pause, stop, seek, loop, speed', '<small>n/a: no screen on the host</small>', (st('part', 'native: starting an animation is tested ' + ev('E2') + '; pause, loop, speed and stop are in source and untested. Filament: controls in source, never run') if ANIM_OK else st('unv')), st('unv')],
 ['Facial blendshapes move the face', st('unv'), st('miss'), st('miss')],
 ['Frame rate holds on the SM-S921B with the current generator', '<small>n/a</small>', '<small>n/a</small>', st('unv', '53.8 FPS was measured with the older generator at a similar triangle count ' + ev('P1'))],
]
section('verify', 'Runtime verification matrix', '<p>What has been seen to work, and where. "Host" is a Linux x86-64 machine running the engine\'s C++ directly; '
        '"CI" is GitHub Actions building the APK and running it on an Android emulator; the phone is the owner\'s Samsung SM-S921B.</p>'
        + table(['Capability', 'Host', 'CI (emulator)', 'Samsung SM-S921B'], rows, 'filter'))

# ----------------------------------------------------------------------------------------------- missing register
rows = [
 ['Hair assets', 'No hair meshes, hair cards, strand grooms or hair textures. Thirteen procedural volumes only.', st('miss')],
 ['Clothing assets', 'No garment meshes or patterns. Ten procedural outfits bound to the body they were generated on; none can be moved to another avatar or to an imported model.', st('miss')],
 ['Skin and face textures', 'No photographic or painted skin, no face textures, no eye textures. Everything is computed.', st('miss')],
 ['Face variety', 'One face per sex, moved by seven shape offsets since 2920265, with six starting presets. Still no face from a photo, no likeness of anyone, and no change of age in the face.', st('part')],
 ['Animation files', 'No BVH, FBX or glTF animation files. 15 clips authored in code.', st('miss')],
 ['An animated GLB to test with', 'Not in this repository, and the prepared avatar has 0 clips. Suitable files do exist in the owner\'s other repositories: <code>CesiumMan.glb</code>, <code>test-rig.glb</code>, <code>studio_avatar.glb</code> and a complete AvatarForge sample; see <a href="#donors">donor projects</a>. None has been brought in or played yet.', st('part')],
 ['Preset pictures', '51 pictures: one per hair style (14), outfit (10), footwear (5), starting look (6), body type (6), breast size (5) and butt size (5), rendered by the host tool from the engine\'s own output and bundled in the app (<code>res/drawable-nodpi/preset_*.jpg</code>). They show the choice on the default female body, not on the avatar being built, and there are no male pictures.', st('src', 'in the APK since 210ae9a (24) and 2920265 (the rest; the hair pictures re-rendered); not seen on a device')],
 ['Retargeting data', 'No joint-name maps to Mixamo, VRM, Unity Humanoid or to the prepared avatar\'s 20 joints.', st('miss')],
 ['Environment lighting', 'No HDR environment files in the repository.', st('miss')],
 ['Licence for the project itself', 'Neither this repository nor AvatarForge has a LICENSE file (GitHub reports none). Third-party notices do exist: ' + src('THIRD_PARTY_NOTICES.md', None, 'THIRD_PARTY_NOTICES.md') + ' covers Filament (Apache-2.0) and <code>female.glb</code> (see <a href="#glb">GLB files</a>).', st('miss')],
 ['Accessories', 'No glasses, hats, jewellery, bags. Armour hardware is the only "gear".', st('miss')],
 ['Accessory and background options', GUIDE.capitalize() + ' have sections for them. The engine has no parameters for accessories or the stage background, so the creator does not list those sections. (Face and eye colour were in the same position until 2920265.)', st('miss')],
 ['The owner\'s second reference (2026-11-15)', 'Three screenshots of a dark picture creator (steps Ethnicity; Body type, Breast size, Butt size; Hair color, Hair style) and eight rendered body references in swimwear, in the owner\'s Dropbox. Looked at, not copied: nothing from them is in the repository or the app. The creator now offers the same choices with the same names (its &quot;BBW&quot; is &quot;Plus size&quot; here), drawn with this engine\'s own renders. What it does not have: photographs on the cards, the step-by-step flow with a progress bar, and bodies or faces of the quality of those references.', st('part', 'the choices exist and are wired ' + ev('H9') + '; the picture quality of the reference is not reached')],
 ['Reference art in the owner\'s Dropbox folder <code>/avatar</code>', 'Looked at on 2026-11-15, not copied: intro art (male, female, cyborg on red smoke), an icon, realistic body references and style references (armour, cyborgs, gothic). They are the quality target the owner named. Nothing from the folder is in the repository or the APK: who made each picture and under what licence is not known.', st('unv')],
 ['Teeth and tongue', 'Modelled only when the engine\'s internal <code>openMouth</code> flag is set ' + src(f'{CPP}/src/af_field.h', r'bool openMouth', 'af_field.h') + '. No spec field or control sets it, so every avatar has a closed mouth with nothing behind the lips.', st('part')],
]
section('missing', 'Missing-assets register', '<p>Things a wardrobe-and-animation application is usually expected to have, that this project does not have. Listed so nobody goes looking for them.</p>'
        + table(['What', 'Detail', 'Status'], rows, 'filter'))

# ----------------------------------------------------------------------------------------------- tasks
rows = [
 ['1', 'Connect the existing appearance presets to controls', st('src', 'done in source: pickers in f90f326, the creator with pictures in 210ae9a, the choices of the picture reference in 2920265; compiled, linted and unit-tested by CI ' + ev('C3') + '; every hair, outfit and footwear preset but box braids also reachable from the prompt ' + ev('H8')), 'Install the phone-test APK of this branch and tap through each section once.'],
 ['2', 'Make skin, hair and clothing changes visible', st('ok', 'on the host: every preset generates, is drawn and looks like what it is named ' + ev('H7')), 'On the phone: one other hair style, one other outfit, a darker and a lighter skin tone, one hair colour, one clothing colour. Known limits are listed under known visual defects.'],
 ['3', 'Connect the animation library to the avatar preview', (st('ok', 'done and proved on the CI emulator: GPU skinning, JNI, controls in the creator, an instrumented test ' + ev('E2')) if ANIM_OK else st('src', 'GPU skinning, JNI, controls and an instrumented test are in source; no passing run')),
  'Watch it on the phone and measure the frame rate with an animation playing. Not done: a seek slider, blending between clips, tests for pause, loop and speed, a test of a walking or jumping clip rather than a held pose.'],
 ['4', 'Verify the prepared rig', st('ok', 'inspected; verdict in <a href="#prepared">its report</a>'), 'It has no clips and an approximate skin. Decide whether to retarget onto it (new work) or to replace it with an asset that carries clips.'],
 ['5', 'Imported GLB animation through the right renderer', st('src', 'Filament controls; routing by file content, unit-tested ' + ev('C3')), 'Needs no new code: import <code>CesiumMan.glb</code> (see <a href="#donors">donor projects</a>) with the phone-test APK of this branch, watch it walk, try every control. Then keep <code>test-rig.glb</code> as a fixture and make that an instrumented test.'],
 ['6', 'Keep GLES performance on the SM-S921B', st('unv', 'not re-measured since the generator changed'), 'Repeat the 2026-11-15 measurement (53.8 FPS at 1.10 M triangles) with the current generator: 1.04 to 1.96 M triangles depending on the outfit on the host, 2.12 M for the gown with long hair in CI. Measure again with an animation playing: nothing is culled then.'],
 ['7', 'Generation, save, export, import, playback end to end', st('part', 'generate → export → import → evaluate proved on the host ' + ev('H5') + '; generate → export → import → draw' + (' → start an animation' if ANIM_OK else '') + ' proved on the CI emulator ' + ev('E1') + ((' ' + ev('E2')) if ANIM_OK else '') + '; Save is in source and unrun'), 'The same chain on the phone, ending with an animation visibly playing and a GLB saved from the share sheet.'],
]
blockers = [
 ['No Android toolchain where this work is done', 'Kotlin and the APK are compiled and tested only by GitHub Actions. Anything in Kotlin is unproved until a run finishes.', st('blk')],
 ['A phone test needs the owner', 'The APK of the code this document describes ' + (('is published: <a href="' + REPO + '/releases/tag/' + APK_TAG + '" target="_blank">' + APK_TAG + '</a>, file <a href="' + REPO + '/releases/download/' + APK_TAG + '/' + APK_FILE + '">' + APK_FILE + '</a>, ' + f'{APK_BYTES:,}' + ' B, sha256 <span class="mono">' + APK_SHA + '</span>, published by run <a href="' + REPO + '/actions/runs/' + str(APK_RUN) + '" target="_blank">' + str(APK_RUN) + '</a> after its readiness gate passed. ') if NEWAPK else 'is published by a manual workflow once CI is green on it; PR #12 has the link. ') + 'It contains the reworked presets, built-in animation and the creator with the choices of the owner\'s picture reference (looks, body type, breast and butt size, hair colours and styles, face, swimwear). Uninstall the earlier build first: each CI debug build is signed with its own one-job key. Nothing in it has been tried on the phone. Earlier builds: <code>phone-test-041e31d</code> is the one the owner reported on; <code>phone-test-9519eae</code> has the reworked presets and still the old screen; <code>phone-test-a5e93a8</code> has built-in animation and the first creator, without those choices.', st('blk')],
 ['Production signing', 'Needs the owner\'s keystore. Preview builds are signed with throw-away keys; uninstall before installing another build.', st('blk')],
 ['Generation time and memory on the phone at the finer lattice', 'The base lattice went from 8 mm to 4 mm. Host time is about the same as before (28–88 s over 27 generations of the preset matrix at <code>2920265</code>, two running at once on a 2-core machine); phone time and peak memory are not measured.', st('unv')],
]
section('tasks', 'Outstanding work and blockers', '<h3>The seven integration priorities</h3>' + table(['#', 'Priority', 'State', 'Next step'], rows, 'filter')
        + '<h3>Blockers</h3>' + table(['Blocker', 'Detail', 'Status'], blockers, 'filter')
        + '<h3>Known defects in the generated avatar</h3><ul>'
          '<li>The T-shirt still bulges at the sides below the armpits: the web of skin between arm and trunk is too large and the cloth follows it.</li>'
          '<li>Garment and hair edges are drawn by the texture; close up they show fine ticks about a millimetre long.</li>'
          '<li>Ears are flat patches with almost no relief. Hidden by the default hair, visible with short styles.</li>'
          '<li>Feet and hands are plain; fingers and toes are simple capsules.</li>'
          '<li>The face is one stylised face, not a likeness of anyone.</li>'
          + ''.join(f'<li>{E(d)}</li>' for d in EXTRA.get('defects', [])) + '</ul>')

# ----------------------------------------------------------------------------------------------- gallery
def img(name, cap):
    return f'<figure><a href="evidence/{name}" target="_blank"><img loading="lazy" src="evidence/{name}" alt="{E(cap)}"></a><figcaption>{cap}</figcaption></figure>'
gal = ('<p class="callout">These are <b>host software renders</b>: the engine\'s own output, imported back through the engine\'s own GLB reader and drawn by a small rasteriser '
       '(' + src(f'{TESTCPP}/host_avatar_render.cpp', None, 'host_avatar_render.cpp') + '). They show geometry, material regions and the baked colour texture. '
       'They are <b>not</b> phone captures and not the app\'s renderer: no shadows, no image lighting.</p>'
       + img('avatar_host_render_face_2026-11-15.jpg', 'Default avatar, face, front and three-quarter. Prompt: "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans".')
       + img('avatar_host_render_body_2026-11-15.jpg', 'The same avatar: front, three-quarter, side, back. Rendered at 041e31d: 1,144,840 triangles then, 1,148,034 with the generator of 2920265 (the hairline and the back of the hair have changed since the picture was made).')
       + img('avatar_host_render_walk_clip_2026-11-15.jpg', 'The same avatar after its GLB was imported and the walk clip applied at 0.30 s. One still frame: evidence that the clip deforms the mesh, not that playback works.')
       + (img('preset_matrix_hair_2026-11-15.jpg', 'Each hair style, generated at the app\'s quality: front, back three-quarter and head. Generator of 2920265.') if EXTRA.get('sheets') else '')
       + (img('preset_matrix_outfits_2026-11-15.jpg', 'Each outfit with a footwear preset: front, back three-quarter and head. Each in its own default colours. Generator of 2920265.') if EXTRA.get('sheets') else '')
       + (img('preset_matrix_footwear_2026-11-15.jpg', 'Each footwear preset with the athletic outfit, feet close up from three sides. Generator of 2920265.') if EXTRA.get('sheets') else '')
       + (img('creator_cards_2026-11-15.jpg', 'Every card of the creator\'s Look, Body and Hair sections, as bundled in the app: six starting looks, six body types, five breast sizes, five butt sizes (in swimwear, a colour chosen to show the form) and fourteen hair styles. Each is one avatar generated at the app\'s quality with exactly the values the card sends. Generator of 2920265.') if os.path.exists(os.path.join(ROOT, 'docs/evidence/creator_cards_2026-11-15.jpg')) else ''))
section('gallery', 'Evidence gallery', gal)

# ----------------------------------------------------------------------------------------------- evidence register
def evrow(code, title, body):
    return f'<div class="evi" id="ev-{code}"><h4><span class="code">{code}</span> {title}</h4>{body}</div>'
mrows = [[E(k), str(v.get('rc')), f'{v.get("secs")} s', f'{v.get("triangles", 0):,}', E(v.get('note', ''))] for k, v in MX.items()]
ci_rows = [[E(c['workflow']) + ''.join(f'<br><small>{E(j[0])}: {E(j[1] or "running")}</small>' for j in c.get('jobs', [])), f'<a href="{REPO}/actions/runs/{c["id"]}" target="_blank">{c["id"]}</a>', E(c['status']), E(c.get('conclusion') or '—')] for c in CI]
PREV = EXTRA.get('ci_prev', {})
prev_html = ('<p>The commit before it that changed code, <code>' + E(PREV.get('sha', '')[:7]) + '</code>: ' + ', '.join(f'{E(r["workflow"])} <a href="{REPO}/actions/runs/{r["id"]}" target="_blank">{r["id"]}</a> {E(r["conclusion"])}' for r in PREV.get('runs', [])) + '.</p>') if PREV else ''
evid = (
 evrow('H1', 'Host test suite, ' + CT + ' passed', '<p>' + E(CTEST.get('when', '')) + ', Linux x86-64, 2 cores, Release build of the engine at commit <code>' + E(CTEST.get('sha', '')) + '</code>. '
       '<code>ctest</code>: native_sampling_regression, glb_extension_regression, camera_frustum_regression, avatar_lod_regression, scene_visibility_regression, imported_glb_pbr_regression, glb_import_robustness, animation_roundtrip, prompt_preset_words. ' + str(CTEST.get('secs', '')) + ' s. '
       'Sources: ' + src(f'{TESTCPP}/native_sampling_test.cpp', None, 'native_sampling_test.cpp') + ', ' + src(f'{TESTCPP}/imported_glb_pbr_test.cpp', None, 'imported_glb_pbr_test.cpp') + ', ' + src(f'{TESTCPP}/animation_roundtrip_test.cpp', None, 'animation_roundtrip_test.cpp') + ', ' + src(f'{TESTCPP}/prompt_preset_words_test.cpp', None, 'prompt_preset_words_test.cpp') + '. '
       'Three expectations in the older tests were changed in this session to match corrected behaviour; the commit message of b636f23 lists them. The same tests run in CI as the job "Native C++ sampling regression" ' + ev('C3') + '.</p>')
 + evrow('H2', 'Host generation of the diagnostic avatar', '<p><code>af_host_probe --sex female --build 2 --hair 3 --outfit 1 --height 1.75 --prompt "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans"</code>: '
       '623,493 vertices, 1,148,034 triangles, 71 joints, 52 blendshapes, 9 submeshes, height 1.769 m; deformed-mesh validation valid=1; GLB 66,730,796 B, sha256 <span class="mono">02986fb84c5cd1d43502b32985437516bcfd00bc4c0fcc7094cb9352071c7909</span>; 42 s wall clock including export and analysis. Run with the generator of <code>2920265</code>; with the generator of <code>9519eae</code> the same command gave 621,802 vertices, 1,144,836 triangles and 66,583,092 B, and at <code>041e31d</code> 621,809 vertices, 1,144,840 triangles and 66,579,684 B.</p>')
 + evrow('H3', 'That GLB imported back through the engine', '<p><code>af_host_render --glb avatar.glb --report --joints</code>: import OK, no warning; 623,493 vertices, 1,148,034 triangles, 9 submeshes, 13 materials; '
       'engine joints 71, skin joints 71, animations 15 (idle 4.0 s, breathe 5.0, walk 1.1, jog 0.72, run 0.62, squat 2.4, jump 1.6, wave 2.2, punch 0.9, kick 1.1, pushup 2.0, t_pose, a_pose, sit 0.1, turn_360 2.0); every vertex carries weights; base-colour texture present. '
       '<code>apply_clip 3 (walk) t=0.300 -> rc=0</code>.</p>')
 + evrow('H4', 'Host software renders', '<p>The images in <a href="#gallery">the gallery</a>, files under <code>docs/evidence/</code>. Looked at by the author of this document; the defects seen are listed under outstanding work.</p>')
 + evrow('H5', 'Animation round-trip test', '<p>' + src(f'{TESTCPP}/animation_roundtrip_test.cpp', None, 'animation_roundtrip_test.cpp') + ' (draft-quality avatar, 13 s): the exported file carries one skin, every joint and inverse bind matrix, one animation per library clip and normalised weights; '
       'the import keeps joints, names, parents, rest translations and vertices with no warning; rest-pose skin matrices are the identity; for every clip at 0 %, 37 % and 80 % of its length the imported vertices skinned with the evaluated matrices match the generating engine\'s own skinning within 0.2 mm; '
       '708 rotation keys read from the file equal engine playback; idle differs from walk; seeking changes the pose; the walk loops (boundary difference 5e-07); bad arguments are refused; a file whose joint names were altered imports as static with a warning.</p>')
 + evrow('H6', 'Inspection of mylove_rig_prepared.glb', '<p>Fetched 2026-11-15 from Dropbox (file modified 2026-11-15T03:26:21Z, 13,221,520 B), sha256 656cf244…576d14. Parsed with Python and NumPy, then imported with <code>af_host_render --report</code>. Findings in <a href="#prepared">the report</a>.</p>')
 + evrow('H7', 'Preset matrix', ('<p>One avatar generated per preset at the app\'s quality with <code>af_host_render --generate --hair N --outfit N --footwear N</code> and drawn. "rc 0" means the generator finished and produced a mesh; the note is what was seen in the render. Hair runs use the T-shirt outfit and bare feet; outfit runs use medium hair and a shoe; footwear runs use short hair and the athletic outfit, with the feet drawn close up. Colours were left at their defaults, which since <code>9519eae</code> means each outfit and each kind of footwear in its own colours: the colour controls are not exercised by this matrix. The runs were made again with the generator of <code>2920265</code>, two at a time on a 2-core machine, so the times are longer than a run alone; the notes say what was seen, and the grade is one person\'s judgement of renders, not a measurement. Scripts: <code>matrix.sh</code> and <code>footwear.sh</code>, each a loop over the command above; the sheets are in <a href="#gallery">the gallery</a>.</p>'
       + table(['Preset run', 'rc', 'Time', 'Triangles', 'Seen'], mrows, 'compact')) if MX else '<p>Not run.</p>')
 + evrow('H8', 'Every preset is reachable from the prompt box', '<p>' + src(f'{TESTCPP}/prompt_preset_words_test.cpp', None, 'prompt_preset_words_test.cpp') + ', ctest <code>prompt_preset_words</code>, part of ' + ev('H1') + ': 30 plain prompts, for hair (14: every style but box braids, which has no prompt word, with bald and two words for bangs), outfits (12, with "t-shirt and jeans" and two words for swimwear) and footwear (4), analysed through <code>af_engine_analyze_prompt</code>, the entry point the app uses. 30 of 30 resolved to the expected preset. '
       'It tests the words, not the look: what each preset looks like is ' + ev('H7') + '.</p>')
 + evrow('H9', 'The creator\'s cards: every choice of the picture reference generated and drawn', '<p>For each card of the creator\'s Look, Body and Hair sections one avatar was generated at the app\'s quality with exactly the values the card sends, using <code>af_host_render --generate</code> and its options for the rest of the spec (<code>--bust --glute --muscle --fat --melanin --face --hairtone --eyetone --clothtone</code>, added in <code>2920265</code>: ' + src(f'{TESTCPP}/host_avatar_render.cpp', r'"--glute"', 'host_avatar_render.cpp') + '). 6 starting looks, 6 body types, 5 breast sizes, 5 butt sizes, 14 hair styles and the swimwear outfit: 37 generations, each with exit code 0; the body, breast and butt cards wear the swimwear outfit in crimson, a colour that shows the form. The pictures are the ones bundled in the app; the sheet is in <a href="#gallery">the gallery</a>.</p>'
       '<p><b>Seen in the renders.</b> Looks: six different skin tones, hair and eye colours; noses, lips, eyes and cheekbones differ visibly but modestly; it is the same stylised face underneath. Body types: slim is narrow, curvy has the smallest waist against the hips, plus size has a broad abdomen, muscular is the widest across the shoulders; athletic and average are close. Breast sizes: five clearly different sizes; the two largest are smooth rounded masses. Butt sizes: five clearly different sizes; the largest is a plain rounded mass. Hair: all fourteen read as their names; box braids are ropes with painted bands, curls a lumpy mass with a curl texture, the fringe a solid band.</p>'
       '<p><b>Measured, not only looked at</b> (host test ' + src(f'{TESTCPP}/native_sampling_test.cpp', r'The shape controls appended to af_spec', 'native_sampling_test.cpp') + ', part of ' + ev('H1') + '): on a 1.70 m female body the furthest-forward point of the bust moves 64 mm from the smallest size to the largest and each size stands at least 4 mm in front of the one before; the furthest-back point of the seat moves 65 mm from the smallest to the largest; under 18 years both controls are ignored; a measured bust girth still decides when given; face offsets are clamped to -1..+1 and both ends of all seven together still give a head; the widest nose is wider than the narrowest; the curvy build\'s waist-to-hip ratio is smaller than the average build\'s; swimwear covers the bust and the pelvis, not the waist, and a male body gets trunks and no top.</p>'
       '<p>What this does not show: any of it on a phone or in the app\'s renderer; a male body with these choices; hair colours other than brown, dark brown and black; a face slider moved alone; the creator screen itself, which nobody has seen drawn.</p>')
 + evrow('G2', 'Git history: which asset files have ever existed here', '<p>On the complete history of the repository (' + HIST + ', fetched without a depth limit): <code>git log --all --diff-filter=A --name-only</code> for the extensions glb, gltf, fbx, obj, bin, ktx, ktx2, png, jpg, jpeg, webp, bvh, anim, vrm, blend, dae, hdr, exr, usd and zip. '
       'The only files ever added are <code>app/src/main/assets/models/female.glb</code>, one Gradle cache file, and the evidence renders under <code>docs/evidence/</code>. No hair, clothing, skin, texture or animation asset file has ever been committed on any branch. <code>main</code> does not contain <code>female.glb</code>.</p>')
 + evrow('D1', 'Scan of the owner\'s other repositories', '<p>' + str(DONORS.get('scanned', '?')) + ' repositories of <code>davealone69-gif</code> visible to the connected GitHub account were listed, and the file tree of each default branch read through the GitHub API (no tree was truncated). Model, animation and environment formats searched for: glb, gltf, fbx, obj, vrm, dae, blend, usd, usdz, stl, ply, bvh, anim, mtl, ktx, ktx2, hdr, exr. '
       'Found: ' + str(DONORS.get('total_model_files', '?')) + ' files, all GLB, in five repositories; no FBX, BVH or other animation file anywhere. Each distinct GLB was downloaded to a scratch folder outside this repository, hashed, its JSON chunk parsed (meshes, skins, animations, morph targets, materials, images) and animation lengths read from the keyframe times. '
       'Four of them were also imported through this engine with <code>af_host_render --report</code>. The three Khronos samples were compared by Git blob with <code>KhronosGroup/glTF-Sample-Assets</code>, and the licence lines quoted are from that repository\'s model READMEs. Nothing was copied into this repository.</p>')
 + (evrow('E1', 'Generated avatar through the production renderer on the CI emulator', (
      '<p>GitHub Actions run <a href="' + REPO + '/actions/runs/' + str(EMU['run']) + '" target="_blank">' + str(EMU['run']) + '</a> (workflow "AvatarForge generated-avatar runtime verification") on commit <code>' + E(EMU['sha'][:7]) + '</code>. '
      'Android emulator ' + E(EMU['device']) + ', ' + E(EMU['gl']) + ', which is a software GPU.</p>'
      '<p>The instrumented test <code>generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView</code> ' + src('app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt', r'fun generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView', 'source') +
      ' generated an avatar with the app\'s engine (' + E(EMU['spec']) + '), exported it (' + f"{EMU['glbBytes']:,}" + ' B, sha256 <span class="mono">' + E(EMU['glbSha']) + '</span>), opened that file in the production <code>AvatarForgeSurfaceView</code> and read the pixels back: '
      'status ' + E(EMU['status']) + ', ' + f"{EMU['visiblePixels']:,}" + ' geometry pixels, ' + str(EMU['presented']) + ' frames presented, ' + str(EMU['drawCalls']) + ' draw calls, ' + f"{EMU['triangles']:,}" + ' triangles, ' + f"{EMU['vertices']:,}" + ' vertices, GL and EGL errors 0.</p>'
      '<p>The renderer\'s telemetry reported <b>joints ' + str(EMU['joints']) + '</b>; the phone, running the build from before the import fix, reported 0 ' + ev('P1') + '. Timings: GLB import ' + f"{EMU['glbImportMs']:.0f}" + ' ms, one pose of the whole mesh ' + f"{EMU['initialPoseMs']:.1f}" + ' ms, first frame ' + f"{EMU['firstFrameMs']:.0f}" + ' ms, ' + f"{EMU['fps']:.2f}" + ' frames per second. '
      'The frame rate says nothing about a phone: this emulator draws on the CPU.</p>'
      '<p>Tests in that run: ' + E(EMU['tests']) + ' APK under test: ' + f"{EMU['apkBytes']:,}" + ' B, sha256 <span class="mono">' + E(EMU['apkSha']) + '</span>.</p>'
      + ('<p>The same workflow afterwards: ' + ', '.join(f'<code>{E(x[0])}</code> run <a href="{REPO}/actions/runs/{x[1]}" target="_blank">{x[1]}</a> {E(x[2])}' for x in EMU.get('later', [])) + '.</p>' if EMU.get('later') else '') +
      '<p>What this does not show: any animation. The test draws the avatar in its rest pose.</p>')) if EMU else evrow('E1', 'Generated avatar through the production renderer on the CI emulator', '<p>Not recorded when this document was generated.</p>'))
 + (evrow('E2', 'Built-in animation through the production renderer on the CI emulator', (
      '<p>GitHub Actions run <a href="' + REPO + '/actions/runs/' + str(ANIM.get('run')) + '" target="_blank">' + str(ANIM.get('run')) + '</a> (workflow "' + E(str(ANIM.get('workflow', ''))) + '") on commit <code>' + E(str(ANIM.get('sha', ''))[:7]) + '</code>, on the same software-GPU emulator as ' + ev('E1') + '.</p>'
      '<p>The instrumented test <code>generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView</code> ' + src('app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt', r'animatedFramesPresented', 'source') +
      ' goes on after the rest pose is verified: it asks the production <code>AvatarForgeSurfaceView</code> for the avatar\'s animations, reads the picture back, plays one, waits for three presented frames and reads the picture back again. '
      'Result: <b>' + str(ANIM.get('clips')) + ' animations listed</b>, <code>' + E(str(ANIM.get('played'))) + '</code> played, ' + str(ANIM.get('frames')) + ' frames presented while it played, <b>' + f"{int(ANIM.get('changedPixels', 0)):,}" + ' pixels of the picture changed</b> (a pixel counts when its three colour channels differ by more than 36 in total), no GL error and no failed draw call in the renderer\'s telemetry. '
      + E(str(ANIM.get('model', ''))) + '</p>'
      '<p>What this shows: the app\'s own view, renderer, JNI and shaders change what is on screen when an animation is started, on an OpenGL ES 3.1 implementation. '
      'What it does not show: motion over time (the pose is a held one, chosen so the picture does not depend on when the frame is taken), pause, loop, speed, any other clip, that the pose is the right one, or anything about a phone. '
      'The run before it, on <code>210ae9a</code>, failed this test with "The production renderer listed no animations for a generated avatar" (runs <a href="' + REPO + '/actions/runs/43695827227" target="_blank">43695827227</a> and <a href="' + REPO + '/actions/runs/43695825489" target="_blank">43695825489</a>); that failure is what exposed the unforwarded calls in the renderer wrapper.</p>')) if ANIM_OK else evrow('E2', 'Built-in animation through the production renderer on the CI emulator', '<p>No passing run was recorded when this document was generated. The test exists: ' + src('app/src/androidTest/java/com/davemega/hub/avatarforge/GeneratedAvatarRuntimeTest.kt', r'animatedFramesPresented', 'GeneratedAvatarRuntimeTest') + '.</p>'))
 + evrow('P2', 'Phone report of 2026-11-15 on the 041e31d build', '<p>The owner\'s words, in full: <i>"Major fault is animation crashes and no built in controls."</i> No diagnostics file, screenshot or crash log came with it, and it does not say what was on screen when the app closed. '
      'In that build an avatar made in the app had no animation control of its own; the one button that promised animation, "Open animation-capable Filament preview", sent the avatar\'s GLB to gltfio. '
      'So the report is taken to mean: no controls for the avatar, and a crash on that button. That reading fits the code (' + src(f'{KT_APP}/GlbPreviewRouting.kt', r'fun animationPreviewRefusal', 'GlbPreviewRouting') + ') and is not confirmed. '
      'If the crash was something else, for example an imported file with clips, this session has not fixed it.</p>')
 + evrow('P1', 'Phone test of 2026-11-15', '<p>Owner\'s Samsung SM-S921B, Android 17, OpenGL ES 3.2 through ANGLE on Vulkan. APK built from <code>797d156</code> (sha256 b35ed11d…896fa1): <b>the generator before this session\'s changes, and before the import fix</b>. '
       'Generated avatar: GLB 83,430,028 B, 1,100,168 triangles, pixels verified (194,401), first frame 2,162 ms, 53.8 FPS over the last 240 frames, renderer reported 0 joints. Imported prepared avatar: 13,221,520 B, 144,073 triangles, frames presented. '
       'Write-up: ' + src('docs/PHONE_TEST_2026-11-15.md', None, 'docs/PHONE_TEST_2026-11-15.md') + '.</p>')
 + evrow('C1', 'CI on 797d156', '<p>All four workflows green on that commit: runs <a href="' + REPO + '/actions/runs/43656813485">43656813485</a>, <a href="' + REPO + '/actions/runs/43656816458">43656816458</a>, <a href="' + REPO + '/actions/runs/43656806774">43656806774</a>, <a href="' + REPO + '/actions/runs/43656818994">43656818994</a>. They say nothing about later commits.</p>')
 + evrow('C2', 'CI on the integration branch at d66eb22', '<p>Failed, as recorded in the README section carried over from that branch: the generated-GLB production render test presented 2 frames in 15 s, and the imported-GLB smoke test ran out of memory. That branch does not contain this branch\'s renderer fixes.</p><p>The owner\'s later README commit <code>5ba0e00</code> records the same for the integration head <code>7b19ca7</code>: diagnostic run <a href="' + REPO + '/actions/runs/43680996825" target="_blank">43680996825</a> and runtime run <a href="' + REPO + '/actions/runs/43680996884" target="_blank">43680996884</a> failed at the production SurfaceView test (2 frames in 15 s), Android CI <a href="' + REPO + '/actions/runs/43680996926" target="_blank">43680996926</a> failed in its imported-GLB renderer job, release preview <a href="' + REPO + '/actions/runs/43680996849" target="_blank">43680996849</a> passed; its verdict is NOT READY. On this branch the same production test passes ' + ev('E1') + '.</p>')
 + evrow('C3', 'CI on this branch', ('<p>Runs for commit <code>' + E(EXTRA.get('ci_sha', '')[:7]) + '</code> when this document was generated:</p>' + table(['Workflow', 'Run', 'Status', 'Conclusion'], ci_rows, 'compact')
       + prev_html + '<p>"Publish phone-test APK" on a pull request only runs the self-test of its publishing rules; publishing is a separate manual run: run <a href="' + REPO + '/actions/runs/' + str(APK_RUN) + '" target="_blank">' + str(APK_RUN) + '</a> checked the readiness gate for this commit and published its APK as pre-release <a href="' + REPO + '/releases/tag/' + APK_TAG + '" target="_blank">' + APK_TAG + '</a>. Adding this document creates a newer commit with its own runs; see <a href="' + REPO + '/pull/12/checks">the checks of PR #12</a> and the README for the current state.</p>') if CI else '<p>See <a href="' + REPO + '/pull/12/checks">the checks of PR #12</a>.</p>')
 + evrow('G1', 'female.glb', '<p>Read from the repository: 418,312 B, sha256 <span class="mono">f2002ff4201e1845097a1647108dbbaf5ad722521d067732f1765e1153bae779</span>; generator "3DoubleD mesh forge"; 1 node, 1 mesh, 3,014 vertices, 16,560 indices; attributes NORMAL, POSITION, TEXCOORD_0; 7 morph targets named muscle, weight, height, shoulder_width, hip_width, bust, limb_length; 2 materials; no skins, animations, images or textures.</p>'
       '<p><b>Compared with its recorded source.</b> ' + src('THIRD_PARTY_NOTICES.md', r'Git blob ID', 'THIRD_PARTY_NOTICES.md') + ' says the file is unchanged from <code>davealone69-gif/3DDD</code> at <code>be5bf17</code>, with matching Git blob <code>a1c00c6b</code>. Measured: the file here is Git blob <code>ab9d7bcb</code>, 418,312 B; the source blob <code>a1c00c6b</code> is 418,556 B. '
       'Their binary chunks are identical (415,864 B, sha256 <span class="mono">68ba800af749efeb…</span>), so the geometry and morph data are the same; the JSON chunks differ (2,420 B here, 2,664 B at the source): the source declares a twelfth, empty accessor and buffer view, which the file here does not have, and writes some numbers differently. '
       'So the model is the same, the file is a re-written header, and the notice\'s "matching blob" line is not accurate. History explains it: the file was added in <code>c6ef5c0</code> as blob <code>a1c00c6b</code>, identical to the source, and re-written by <code>98dd9ec</code> ("fix: repair GLB morph compatibility before Filament native loading", 2026-11-06). The notice was written on a line of history that still had the original. The notice is corrected in the commit that adds this document; the link above shows it as it stood. The CC0 statement was read in the 3DDD README at that revision: "The generated base rigs are original work shipped under CC0."</p>')
)
section('evidence', 'Evidence register', evid)

# ----------------------------------------------------------------------------------------------- links
links = (f'<ul><li><a href="{REPO}/blob/{BRANCH}/README.md">README of this branch</a></li>'
         f'<li><a href="{REPO}/pull/12">PR #12</a>: this branch into <code>integration/avatarforge-renderer-port</code></li>'
         f'<li><a href="{REPO}/pull/9">PR #9</a>: <code>integration/avatarforge-renderer-port</code> into <code>main</code> (not merged; <code>main</code> untouched)</li>'
         f'<li>Commits this document describes: <a href="{REPO}/commit/b636f23">b636f23</a> generator and import fix · <a href="{REPO}/commit/22f80a7">22f80a7</a> merge of the integration branch · <a href="{REPO}/commit/f90f326">f90f326</a> preset controls · <a href="{REPO}/commit/0dd5bc8">0dd5bc8</a> test expectation for the T-shirt outfit · <a href="{REPO}/commit/39b0be9">39b0be9</a> merge of the integration branch at 5ba0e00 (README only) · <a href="{REPO}/commit/041e31d">041e31d</a> import adopts a skeleton only when the whole clip library binds; one prompt test per preset · <a href="{REPO}/commit/9519eae">9519eae</a> the 14 weak presets rebuilt · <a href="{REPO}/commit/210ae9a">210ae9a</a> GPU-skinned animation, its controls, the creator, the Filament guard · <a href="{REPO}/commit/a5e93a8">a5e93a8</a> playback passed through the renderer wrapper; stage buttons. The source links point at <code>{CODE_SHA[:7]}</code></li>'
         f'<li>Other documents: ' + src('docs/PHONE_TEST_2026-11-15.md', None, 'PHONE_TEST_2026-11-15.md') + ' · ' + src('docs/AVATAR_QUALITY_PLAN.md', None, 'AVATAR_QUALITY_PLAN.md') + ' · ' + src('docs/BRAVO_REVIEW.md', None, 'BRAVO_REVIEW.md') + ' · ' + src('BUILD_AND_VERIFY.md', None, 'BUILD_AND_VERIFY.md') + '</li>'
         f'<li>Engine origin: <a href="https://github.com/davealone69-gif/AvatarForge">davealone69-gif/AvatarForge</a> · source of the bundled sample: <a href="https://github.com/davealone69-gif/3DDD">davealone69-gif/3DDD</a> · Khronos samples: <a href="https://github.com/KhronosGroup/glTF-Sample-Assets">KhronosGroup/glTF-Sample-Assets</a></li></ul>')
section('links', 'README, pull requests and related documents', links)

# ----------------------------------------------------------------------------------------------- page
toc = ''.join(f'<a href="#{i}">{E(t)}</a>' for i, t, _ in sections)
body = ''.join(f'<section id="{i}"><details class="sec" open><summary><h2>{E(t)} <a class="anchor" href="#{i}" title="Link to this section">#</a></h2></summary><div class="sb">{b}</div></details></section>' for i, t, b in sections)
chips = ''.join(f'<button class="chip st {k}" data-filter="{k}">{v}</button>' for k, v in LABELS.items())
CSS = '''
:root{--bg:#0d1117;--panel:#151b23;--panel2:#1b222c;--line:#2a3340;--text:#d7dde5;--dim:#8b96a5;--link:#6cb6ff;--acc:#e3b341}
*{box-sizing:border-box}html{scroll-behavior:smooth;scroll-padding-top:118px}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.55 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
a{color:var(--link);text-decoration:none}a:hover{text-decoration:underline}
header{position:sticky;top:0;z-index:5;background:#0d1117f2;backdrop-filter:blur(6px);border-bottom:1px solid var(--line);padding:10px 18px}
header h1{font-size:18px;margin:0 0 6px}header .meta{color:var(--dim);font-size:12.5px}
.bar{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin-top:8px}
#q{flex:1 1 220px;min-width:160px;background:var(--panel);border:1px solid var(--line);color:var(--text);border-radius:6px;padding:7px 10px;font-size:14px}
.btn{background:var(--panel);border:1px solid var(--line);color:var(--text);border-radius:6px;padding:6px 10px;cursor:pointer;font-size:12.5px}
.layout{display:grid;grid-template-columns:250px minmax(0,1fr);gap:0}
nav{position:sticky;top:118px;align-self:start;max-height:calc(100vh - 126px);overflow:auto;padding:14px 10px 30px 18px;border-right:1px solid var(--line)}
nav a{display:block;padding:5px 8px;border-radius:6px;color:var(--dim);font-size:13.5px}nav a:hover,nav a.on{background:var(--panel);color:var(--text);text-decoration:none}
main{padding:6px 26px 80px;min-width:0}
section{padding-top:8px;margin-bottom:26px}h2{font-size:21px;border-bottom:1px solid var(--line);padding-bottom:6px;margin:26px 0 12px}
h3{font-size:16px;margin:20px 0 8px;color:#e6edf3}h4{margin:0 0 4px;font-size:14.5px}
.anchor{color:var(--line);margin-right:4px}.lead{font-size:16px}.small,small{color:var(--dim);font-size:12.5px}
.mono,code{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:12.5px}code{background:var(--panel2);padding:1px 5px;border-radius:4px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px;margin:14px 0}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:12px 14px}.card b{display:block;font-size:24px;color:#fff}.card span{color:var(--dim);font-size:13px}.card.warn{border-color:#8a3b3b}.card.warn b{color:#ff938a}
.tw{overflow-x:auto;border:1px solid var(--line);border-radius:8px;margin:10px 0}
table{border-collapse:collapse;width:100%;font-size:13.5px}th,td{text-align:left;vertical-align:top;padding:8px 10px;border-bottom:1px solid var(--line)}
th{background:var(--panel2);position:sticky;top:0;font-weight:600;color:#e6edf3;white-space:nowrap}tr:last-child td{border-bottom:0}tbody tr:hover{background:#ffffff06}
table.compact td,table.compact th{padding:4px 8px;font-size:12.5px}
.st{display:inline-block;font-size:10.5px;font-weight:700;letter-spacing:.03em;padding:2px 7px;border-radius:999px;border:1px solid;white-space:nowrap;margin:1px 0}
.st.ok{color:#56d364;border-color:#2ea04366;background:#2ea04318}.st.src{color:#6cb6ff;border-color:#388bfd66;background:#388bfd18}
.st.part{color:#e3b341;border-color:#bb800966;background:#bb800918}.st.unv{color:#b1bac4;border-color:#6e768166;background:#6e768118}
.st.miss{color:#ff938a;border-color:#f8514966;background:#f8514918}.st.blk{color:#d2a8ff;border-color:#a371f766;background:#a371f718}
.note{color:var(--dim);font-size:12.5px}.chip{cursor:pointer;opacity:.9}.chip.off{opacity:.35}.chip.sel{outline:2px solid #fff5}
a.src{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:12px}a.ev{font-family:ui-monospace,Menlo,monospace;font-size:11.5px;background:var(--panel2);border:1px solid var(--line);border-radius:4px;padding:0 4px;color:var(--acc)}
.bad{color:#ff938a}details{background:var(--panel);border:1px solid var(--line);border-radius:8px;margin:10px 0}summary{cursor:pointer;padding:9px 12px;font-weight:600}
.db{padding:0 12px 10px}.callout{background:#bb800914;border-left:3px solid var(--acc);padding:9px 12px;border-radius:0 8px 8px 0}
figure{margin:14px 0;background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px}figure img{max-width:100%;height:auto;border-radius:6px;display:block}figcaption{color:var(--dim);font-size:13px;margin-top:6px}
.evi{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:10px 12px;margin:8px 0}.evi .code{color:var(--acc);font-family:ui-monospace,monospace;margin-right:6px}.evi:target{outline:2px solid var(--acc)}
details.sec{background:none;border:0;border-radius:0;margin:0}details.sec>summary{padding:0;list-style:none;font-weight:inherit}details.sec>summary::-webkit-details-marker{display:none}
details.sec>summary h2::before{content:"\\25BE";display:inline-block;width:1.1em;color:var(--dim);font-size:.85em}details.sec:not([open])>summary h2::before{content:"\\25B8"}details.sec:not([open])>summary h2{margin-bottom:6px;color:var(--dim)}
.hide{display:none!important}#count{color:var(--dim);font-size:12.5px}
footer{color:var(--dim);font-size:12.5px;border-top:1px solid var(--line);margin-top:30px;padding-top:12px}
@media(max-width:900px){.layout{grid-template-columns:1fr}nav{position:static;max-height:none;border-right:0;border-bottom:1px solid var(--line);display:flex;flex-wrap:wrap;gap:2px;padding:8px 12px}nav a{padding:4px 8px}main{padding:6px 14px 60px}html{scroll-padding-top:170px}}
'''
JS = '''
const q=document.getElementById('q'),count=document.getElementById('count');let active=null;
function apply(){const t=q.value.trim().toLowerCase();let shown=0,total=0;
 document.querySelectorAll('table.filter tbody tr').forEach(r=>{total++;const okT=!t||r.textContent.toLowerCase().includes(t);const okS=!active||r.querySelector('.st.'+active);const ok=okT&&okS;r.classList.toggle('hide',!ok);if(ok)shown++;});
 document.querySelectorAll('section').forEach(s=>{if(!t&&!active){s.classList.remove('hide');return;}const rows=s.querySelectorAll('table.filter tbody tr');const any=[...rows].some(r=>!r.classList.contains('hide'));const textHit=t&&!active&&s.textContent.toLowerCase().includes(t);s.classList.toggle('hide',!(any||textHit));if(any||textHit){const d=s.querySelector('details.sec');if(d)d.open=true;}});
 count.textContent=(t||active)?shown+' of '+total+' rows':total+' rows';
 if(t||active)document.querySelectorAll('details').forEach(d=>{if([...d.querySelectorAll('tr')].some(r=>!r.classList.contains('hide')))d.open=true;});}
q.addEventListener('input',apply);
document.querySelectorAll('.chip').forEach(c=>c.addEventListener('click',()=>{const f=c.dataset.filter;active=(active===f)?null:f;document.querySelectorAll('.chip').forEach(x=>{x.classList.toggle('sel',x.dataset.filter===active);x.classList.toggle('off',active&&x.dataset.filter!==active);});apply();}));
document.getElementById('exp').onclick=()=>document.querySelectorAll('details').forEach(d=>d.open=true);
document.getElementById('col').onclick=()=>document.querySelectorAll('details').forEach(d=>d.open=false);
document.getElementById('clr').onclick=()=>{q.value='';active=null;document.querySelectorAll('.chip').forEach(x=>x.classList.remove('sel','off'));apply();};
const links=[...document.querySelectorAll('nav a')];
if('IntersectionObserver' in window){const io=new IntersectionObserver(es=>{es.forEach(e=>{if(e.isIntersecting){links.forEach(l=>l.classList.toggle('on',l.getAttribute('href')==='#'+e.target.id));}});},{rootMargin:'-120px 0px -70% 0px'});document.querySelectorAll('main section').forEach(s=>io.observe(s));}
function reveal(id){let el=null;try{el=document.getElementById(decodeURIComponent(id));}catch(e){}if(!el)return false;let p=el;while(p){if(p.tagName==='DETAILS')p.open=true;if(p.classList)p.classList.remove('hide');p=p.parentElement;}if(el.tagName==='SECTION'){const d=el.querySelector('details.sec');if(d)d.open=true;}return true;}
document.addEventListener('click',e=>{const a=e.target.closest?e.target.closest('a[href^="#"]'):null;if(a&&a.getAttribute('href').length>1)reveal(a.getAttribute('href').slice(1));});
window.addEventListener('hashchange',()=>{if(location.hash.length>1&&reveal(location.hash.slice(1))){const t=document.getElementById(decodeURIComponent(location.hash.slice(1)));if(t)t.scrollIntoView();}});
apply();if(location.hash.length>1&&reveal(location.hash.slice(1))){const t=document.getElementById(decodeURIComponent(location.hash.slice(1)));if(t)t.scrollIntoView();}
'''
page = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>3D Mega Hub + AvatarForge: avatar assets and animation master document</title><style>{CSS}</style></head>
<body><header><h1>3D Mega Hub + AvatarForge — avatar assets and animation</h1>
<div class="meta">Master inventory · evidence recorded {EXTRA.get("generated", "")}{(" at <code>" + EVIDENCE_SHA[:7] + "</code>") if STALE else ""} · page built {BUILT} · source links point at commit <a href="{REPO}/commit/{CODE_SHA}"><code>{CODE_SHA[:12]}</code></a> on <code>{BRANCH}</code> (<a href="{REPO}/pull/12">PR #12</a> → <a href="{REPO}/pull/9">PR #9</a>, not merged) · one engine, one application, one inventory</div>
<div class="bar"><input id="q" type="search" placeholder="Search every table: ponytail, WebP, skinning, blendshape, MISSING…" autocomplete="off">{chips}
<button class="btn" id="clr">Clear</button><button class="btn" id="exp">Expand all</button><button class="btn" id="col">Collapse all</button><span id="count"></span></div></header>
<div class="layout"><nav>{toc}</nav><main>{body}
<footer>Assembled from the repository at <code>{CODE_SHA}</code> and from the evidence listed in the register. Source links are permanent links to that commit.
A status here is a statement about that commit and that evidence; it does not carry over to later commits by itself.
{('<b>The evidence was recorded at <code>' + EVIDENCE_SHA[:7] + '</code>; the source links point at a later commit, <code>' + CODE_SHA[:7] + '</code>.</b> ') if STALE else ''}Regenerate with <code>python3 tools/avatar_master_doc/build_doc.py</code>.</footer></main></div>
<script>{JS}</script></body></html>'''
out = os.path.join(ROOT, 'docs/AVATAR_ASSETS_ANIMATION_MASTER.html')
open(out, 'w').write(page)
bad = page.count('class="bad"')
print('wrote', out, len(page), 'bytes;', len(sections), 'sections; unresolved source links:', bad)
print('patterns that matched no line:', len(MISSES))
if STALE: print('NOTE: evidence in doc_extra.json was recorded at', EVIDENCE_SHA[:7], 'and the tree is at', CODE_SHA[:7])
for m in MISSES: print('   ', m)
