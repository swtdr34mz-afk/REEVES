#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 swtdr34mz (REEVES)
"""The MPC skin (Plugin Skins/: TUI.json, Q-Links.json and the pictures) from the renderer's output.

    python3 mpc/skin/make_skin.py <render folder> mpc/plugin/skins

The renderer (render_skin.cpp) draws each MPC page with the VST3's own art and cuts every control out at every value;
this places each picture where it was cut from, bound to the MPC plugin's parameter:

  knobs      film strips (a frame a value), dragged; a double tap opens the MPC's editor (its list for a choice)
  buttons    a picture for each value; a tap switches it (or steps to the next value: SHAPE)
  LED lists  each row a radio button

The pages (the function keys): OSC, FILTER, ENV, LFO, MOD (with its display), PLAY, each twice: pressed once layer A,
again layer B.
On each the 16 Q-Links turn up to four groups, the top row of Q-Links the first group and so on down. Standard library
only (Pillow, if there, makes the pictures smaller).
"""
import json
import os
import shutil
import sys

# The MPC plugin's parameters (mpc/plugin/src/ReevesMpc.cpp): 0 VOLUME, then layer A's 136, layer B's 136, then
# the program's own. A page shows a layer's controls by their VST ids without the layer ("cutoff"): the sub-tab says
# which layer (the function key pressed once: A, twice: B).
LAYER_IDS = """o1_freq o1_fine o1_shape o1_shmod o2_freq o2_fine o2_shape o2_shmod sync sub osc_mix noise slop o1_key o2_key
o1_reset o2_reset glide o1_glide o2_glide glide_mode cutoff reso fenv_amt fvel keyamt audiomod pole4 fdel fatt fdec
fsus frel pan_spread aenv_amt avel adel aatt adec asus arel vca_level e3_dest e3_amt e3_vel e3del e3att e3dec e3sus
e3rel e3_repeat lfo1_freq lfo1_shape lfo1_amt lfo1_dest lfo1_clk lfo1_key lfo2_freq lfo2_shape lfo2_amt lfo2_dest
lfo2_clk lfo2_key lfo3_freq lfo3_shape lfo3_amt lfo3_dest lfo3_clk lfo3_key lfo4_freq lfo4_shape lfo4_amt lfo4_dest
lfo4_clk lfo4_key mod1_src mod1_dst mod1_amt mod2_src mod2_dst mod2_amt mod3_src mod3_dst mod3_amt mod4_src mod4_dst
mod4_amt mod5_src mod5_dst mod5_amt mod6_src mod6_dst mod6_amt mod7_src mod7_dst mod7_amt mod8_src mod8_dst mod8_amt
mw_amt mw_dst at_amt at_dst br_amt br_dst vel_amt vel_dst ft_amt ft_dst fx_on fx_type fx_mix fx_p1 fx_p2 fx_clk bpm
divide arp_on arp_mode arp_range arp_repeat arp_relatch seq_on seq_type seq_mode seq_d1 seq_d2 seq_d3 seq_d4 pgm_vol
unison uni_mode uni_detune key_mode pb_range pan_mode""".split()
assert len(LAYER_IDS) == 136
NL = len(LAYER_IDS)
GLOBAL = {"volume": 0, "layer_mode": 1 + 2 * NL, "split_point": 2 + 2 * NL, "voices": 3 + 2 * NL, "quality": 4 + 2 * NL,
          "hold": 5 + 2 * NL}
NPARAMS = 6 + 2 * NL


def mpc(pid, layer):
    """the MPC index of a page's id on a layer's sub-tab (0: A, 1: B); "a_cutoff" / "b_cutoff" name the layer"""
    if pid in GLOBAL:
        return GLOBAL[pid]
    if pid[:2] in ("a_", "b_"):
        return mpc(pid[2:], pid[0] == "b")
    return 1 + layer * NL + LAYER_IDS.index(pid)


# The pages (the renderer's names) and their Q-Links' groups top down (a group as (ids, False): not framed)
PAGES = [
    ("OSC", [(["o1_freq", "o1_fine", "o1_shmod", "sub"], False),
             (["o2_freq", "o2_fine", "o2_shmod", "osc_mix"], False),
             (["noise", "slop"], False)]),
    ("FILTER", [["cutoff", "reso", "fenv_amt", "fvel"],
                (["keyamt", "audiomod"], False),
                ["fatt", "fdec", "fsus", "frel"],
                (["fdel"], False)]),
    ("ENV", [(["aatt", "adec", "asus", "arel"], False),
             (["pan_spread", "aenv_amt", "avel", "adel"], False),
             (["e3att", "e3dec", "e3sus", "e3rel"], False),
             (["e3_dest", "e3_amt", "e3_vel", "e3del"], False)]),
    ("LFO", [(["lfo1_freq", "lfo1_amt", "lfo1_dest"], False),
             (["lfo2_freq", "lfo2_amt", "lfo2_dest"], False),
             (["lfo3_freq", "lfo3_amt", "lfo3_dest"], False),
             (["lfo4_freq", "lfo4_amt", "lfo4_dest"], False)]),
    ("MOD", [(["mod1_amt", "mod2_amt", "mod3_amt", "mod4_amt"], False),
             (["mod5_amt", "mod6_amt", "mod7_amt", "mod8_amt"], False),
             (["mod1_dst", "mod2_dst", "mod3_dst", "mod4_dst"], False),
             (["mod5_dst", "mod6_dst", "mod7_dst", "mod8_dst"], False)]),
    ("PLAY", [(["fx_type", "fx_mix", "fx_p1", "fx_p2"], False),
              (["arp_mode", "divide", "arp_range", "arp_repeat"], False),
              (["o1_glide", "o2_glide", "pgm_vol", "seq_mode"], False),
              (["uni_mode", "uni_detune", "key_mode", "split_point"], False)]),
]
MODH = "Local/8/modslot"                 # the slot MOD's knobs edit (the screen's own value)
# Program (Track) mode: the knobs as the MPC's own program knobs (layer A), bottom left Q-Link 1
PROGRAM_QLINKS = ["a_fx_mix", "a_lfo1_amt", "a_uni_detune", "volume",
                  "a_aatt", "a_adec", "a_asus", "a_arel",
                  "a_fatt", "a_fdec", "a_fsus", "a_frel",
                  "a_cutoff", "a_reso", "a_fenv_amt", "a_keyamt"]


class O(list):
    """A JSON object kept in order, with repeated keys (the overlays' two "version"s)."""
    def __init__(self, *pairs):
        super().__init__(pairs)


def dump(v, ind=0):
    sp = "  " * ind
    if isinstance(v, O):
        if not v:
            return "{}"
        return "{\n" + ",\n".join("%s  %s: %s" % (sp, json.dumps(k), dump(x, ind + 1)) for k, x in v) + "\n" + sp + "}"
    if isinstance(v, list):
        if not v:
            return "[]"
        if all(not isinstance(x, (O, list)) for x in v):
            return "[" + ", ".join(json.dumps(x) for x in v) + "]"
        return "[\n" + ",\n".join(sp + "  " + dump(x, ind + 1) for x in v) + "\n" + sp + "]"
    return json.dumps(v)


def box(r):
    return "%d %d %d %d" % tuple(r)


def image_data(img):
    return O(("version", 2), ("imageType", "Regular"), ("colour", "0"), ("image", img))


def bounds(r, focus="No", hide_handles=None, show="Show"):
    if hide_handles:
        return O(("version", 2), ("acceptsHWFocus", focus), ("showWhenDataModelInvalid", show), ("whenVisible", "Always"),
                 ("boundsType", "Absolute"), ("bounds", box(r)), ("additionalInvalidatingHandles", hide_handles))
    return O(("version", 1), ("acceptsHWFocus", focus), ("showWhenDataModelInvalid", show), ("whenVisible", "Always"),
             ("boundsType", "Absolute"), ("bounds", box(r)))


def entry(name, typ, data, b, remap=None):
    return O(("version", 2),
             ("componentData", O(("version", 1), ("name", name), ("type", typ), ("data", data))),
             ("handle remapping", O(("version", 1), ("map", [O(("key", k), ("value", v)) for k, v in (remap or [])]))),
             ("bounds", b))


def action(on, handler, handle, extra):
    return O(("version", 1), ("onAction", on), ("handler", handler), ("handleName", handle), ("additionalData", extra))


def component(actions, children, ignore_mouse=False):
    clear = O(("version", 1), ("colour", "0"), ("image", ""))
    return O(("version", 2), ("actions", actions),
             ("backgroundData", O(("version", 1), ("focussed", clear), ("unfocussed", clear))),
             ("ignoreMousePresses", ignore_mouse), ("disableCoarseDataWheel", False), ("componentsData", children))


FOCUS_LOST = O(("version", 1), ("onAction", "Focus Lost"), ("handler", "Focus Lost"), ("handleName", "Focus Lost"),
               ("additionalData", ""))


def overlay(typ, data):
    return component([], [entry("overlay", typ, data, bounds((0, 0, 1280, 800), "Yes"))], ignore_mouse=True)


def range_handle(a, b, steps, h):
    return ("IndexedEnabling/%d/%d/%s" % (a, steps, h)) if a == b else ("RangedEnabling/%d/%d/%d/%s" % (a, b, steps, h))


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    src, out = argv[1], argv[2]
    m = json.load(open(os.path.join(src, "manifest.json")))
    pages = {p["name"]: p for p in m["pages"]}
    for p in m["pages"]:
        for c in p["controls"]:
            if c["id"] not in GLOBAL and c["id"] not in LAYER_IDS:
                sys.exit("%s: the page's %s is not one of the MPC plugin's parameters" % (p["name"], c["id"]))

    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    used = set()

    def use(f):
        used.add(f)
        return f

    def knob_overlay(bipolar):
        return overlay("KnobOverlay", O(("version", 2), ("version", 1), ("actions", [FOCUS_LOST]), ("overlayType", "Knob With Header"),
                                        ("scaleType", "Linear"), ("polarityType", "Bipolar" if bipolar else "Unipolar"),
                                        ("showMinusSign", True), ("handleName", "Data")))
    defs = [("knob overlay", knob_overlay(False)), ("knob overlay bipolar", knob_overlay(True)),
            ("menu overlay", overlay("MenuOverlay", O(("version", 1), ("version", 1), ("actions", [FOCUS_LOST]),
                                                      ("withNumbers", False), ("handleName", "Data"))))]
    defined = set()
    focus = O(("version", 1), ("backgroundColour", "19ffffff"), ("outlineColour", "ffe81f40"), ("backgroundInset", 1.0),
              ("outlineThickness", 1.0))
    names = {}                           # the discrete parameters' names by MPC index (replacementStrings)

    def tab_children(page, layer):
        kids = [entry("panel", "Image", image_data(use(page["background"])), bounds((0, 0, 1280, 628))),
                entry("layer", "Image", image_data(use(page["labelB" if layer else "labelA"])), bounds(page["labelRect"]))]
        boxes, legends = [], []

        for i, c in enumerate(page["controls"]):
            idx = mpc(c["id"], layer)
            if c["names"]:
                names[idx] = c["names"]
            h = "Parameter %d" % idx
            if c["kind"] == "knob":
                fr = c["frame"]
                kx, ky, w, hh = c["rect"]
                ox, oy = kx - fr[0], ky - fr[1]
                ov = "menu overlay" if c["names"] else "knob overlay bipolar" if c["bipolar"] else "knob overlay"
                name = "%s knob %d %s" % (page["name"].lower(), i, c["id"])
                if name not in defined:  # (one definition for both layers: the handle is remapped)
                    defined.add(name)
                    knob = O(("version", 3), ("knobType", "FilmStrip"), ("filmStrip", use(c["strip"])), ("numFrames", c["frames"]),
                             ("invert", False), ("dragOrientation", "Vertical"), ("handleName", "Data"))
                    defs.append((name, component(
                        [action("Mouse Down", "Q-Link", "Data", ""), action("Double Click", "Show Overlay", "Data", ov),
                         action("Enter Pressed", "Show Overlay", "Data", ov)],
                        [entry("knob", "Knob", knob, bounds((ox, oy, w, hh))),
                         entry("focus", "Focus", focus, O(("version", 1), ("acceptsHWFocus", "No"), ("showWhenDataModelInvalid", "Show"),
                                                          ("whenVisible", "WhenFocussed"), ("boundsType", "Absolute"),
                                                          ("bounds", box((0, 0, fr[2], fr[3])))))])))
                hide = ["IndexedEnabling/%d/8/%s" % (c["slot"], MODH)] if c.get("slot", -1) >= 0 else None
                if c.get("value"):           # its value under its legend
                    vs = O(("version", 2), ("imageType", "FilmStrip"), ("colour", "0"), ("image", use(c["value"])),
                           ("numFrames", c["steps"]), ("handleName", h))
                    kids.append(entry(c["id"] + " value", "Image", vs, bounds(c["valueRect"], "No", hide)))
                kids.append(entry(c["id"], name, O(("version", 1), ("handleName", "Data")), bounds(fr, "Yes", hide, "Hide"),
                                  [("Data", h)]))
                boxes.append((c["id"], fr))
                legends.append((c["id"], fr))
            elif c["kind"] == "button":
                rs = c["ranges"]
                for r in rs:
                    hide = [range_handle(r["a"], r["b"], c["steps"], h)] if len(rs) > 1 else None
                    data = O(("version", 2), ("onImage", use(r["image"])), ("offImage", r["image"]), ("buttonId", r["next"]),
                             ("numButtonsInGroup", c["steps"]), ("handleName", h), ("gestureBehaviour", "Instant"))
                    kids.append(entry(c["id"], "Button", data, bounds(c["rect"], "No", hide, "Hide")))
                boxes.append((c["id"], c["rect"]))
            else:
                if len(c["rows"]) > c["steps"]:
                    sys.exit("%s: %d LEDs for %d values" % (c["id"], len(c["rows"]), c["steps"]))
                for k, row in enumerate(c["rows"]):
                    data = O(("version", 2), ("onImage", use(row["on"])), ("offImage", use(row["off"])),
                             ("buttonId", row.get("value", k)),
                             ("numButtonsInGroup", c["steps"]), ("handleName", h), ("gestureBehaviour", "Instant"))
                    kids.append(entry(c["id"], "Button", data, bounds(row["rect"])))
                    boxes.append((c["id"], row["rect"]))
        if page.get("cells"):            # MOD's display: the eight slots' values, the slot numbers, SLOT n
            for cell in page["cells"]:
                st = O(("version", 2), ("imageType", "FilmStrip"), ("colour", "0"), ("image", use(cell["strip"])),
                       ("numFrames", cell["steps"]), ("handleName", "Parameter %d" % mpc(cell["id"], layer)))
                kids.append(entry(cell["id"] + " cell", "Image", st, bounds(cell["rect"])))
            for k, row in enumerate(page["select"]):
                data = O(("version", 2), ("onImage", use(row["on"])), ("offImage", use(row["off"])), ("buttonId", k),
                         ("numButtonsInGroup", 8), ("handleName", MODH), ("gestureBehaviour", "Instant"))
                kids.append(entry("mod slot", "Button", data, bounds(row["rect"])))
                boxes.append(("", row["rect"]))
            st = O(("version", 2), ("imageType", "FilmStrip"), ("colour", "0"), ("image", use(page["slotStrip"])),
                   ("numFrames", 8), ("handleName", MODH))
            kids.append(entry("mod slot shown", "Image", st, bounds(page["slotShow"])))
        return kids, boxes, legends

    def frame(group, boxes, legends):
        """The Q-Link group's frame: around its controls with their legends, with an even gap that cuts no other."""
        mine = [r for p, r in boxes + legends if p in group]
        if not mine:
            return None
        x0, y0 = min(r[0] for r in mine), min(r[1] for r in mine)
        x1, y1 = max(r[0] + r[2] for r in mine), max(r[1] + r[3] for r in mine)

        def cuts(a, b, c, d):
            for _pid, r in boxes:
                ix = min(c, r[0] + r[2]) - max(a, r[0])
                iy = min(d, r[1] + r[3]) - max(b, r[1])
                inside = r[0] >= a and r[1] >= b and r[0] + r[2] <= c and r[1] + r[3] <= d
                if ix > 0 and iy > 0 and not inside:
                    return r
            return None
        while True:
            r = cuts(x0, y0, x1, y1)
            if r is None:
                break
            x0, y0, x1, y1 = min(x0, r[0]), min(y0, r[1]), max(x1, r[0] + r[2]), max(y1, r[1] + r[3])
        for pad in (6, 5, 4, 3, 2, 1, 0):
            a, b, c, d = max(0, x0 - pad), max(0, y0 - pad), min(1280, x1 + pad), min(628, y1 + pad)
            if cuts(a, b, c, d) is None:
                return (a, b, c - a, d - b)
        return (x0, y0, x1 - x0, y1 - y0)

    tabs, qmaps = [], []
    for key, (name, groups) in enumerate(PAGES):
        page = pages[name]
        for layer in (0, 1):
            kids, boxes, legends = tab_children(page, layer)
            rects, q = [], {}
            for row, g in enumerate(groups):
                ids, framed = (g, True) if isinstance(g, list) else g
                if framed:
                    r = frame(ids, boxes, legends)
                    if r:
                        rects.append(r)
                for col, pid in enumerate(ids):
                    q[13 + col - 4 * row] = mpc(pid, layer)
            tabs.append(O(("version", 1), ("tabName", name + (" B" if layer else " A")), ("fnKeyIndex", key),
                          ("fnKeySubIndex", layer), ("componentDefinition", component([], kids)),
                          ("qlinkBoundsData", [box(r) for r in rects])))
            qmaps.append(O(("Tab", key + 1), ("SubTab", layer + 1), ("Bank Direction", "RowTopDown"),
                           ("Q-Links", O(*[("Q-Link %d" % n, q.get(n, -1)) for n in range(1, 17)]))))

    repl = [O(("key", "Parameter %d" % k), ("value", v)) for k, v in sorted(names.items())]
    tui = O(("pageData", O(("version", 1),
                           ("componentDefinitions", O(("version", 2), ("importFiles", []),
                                                      ("localComponentDefinitions", [O(("key", k), ("value", v)) for k, v in defs]))),
                           ("replacementStrings", repl),
                           ("info", O(("version", 1), ("type", "CompleteDescription"))),
                           ("tabs", tabs))))
    ql = O(("version", 4), ("info", O(("version", 1), ("type", "CompleteDescription"))),
           ("Screen Mode Q-Links", O(("version", 4), ("map", qmaps))),
           ("Program Mode Q-Links", O(*[("Q-Link %d" % (n + 1), mpc(pid, 0)) for n, pid in enumerate(PROGRAM_QLINKS)])))
    with open(os.path.join(out, "TUI.json"), "w") as f:
        f.write(dump(tui) + "\n")
    with open(os.path.join(out, "Q-Links.json"), "w") as f:
        f.write(dump(ql) + "\n")
    try:                                 # (256 colours: the pictures a fraction of the size)
        from PIL import Image
    except ImportError:
        Image = None
        print("(no Pillow: the pictures copied as they are, many times larger)")
    for f in sorted(used):
        if Image:
            im = Image.open(os.path.join(src, f)).convert("RGB")
            im.quantize(256, method=Image.MEDIANCUT, dither=Image.NONE).save(os.path.join(out, f), optimize=True)
        else:
            shutil.copy2(os.path.join(src, f), os.path.join(out, f))
    print("TUI.json: %d tabs, %d components; Q-Links.json; %d pictures in %s" % (len(tabs), len(defs), len(used), out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
