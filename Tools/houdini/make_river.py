"""Build the IWB low-poly river HDA in the open Houdini (run with Tools/houdini.py -f).

Input 0: one open curve along the river, its points already at the water
surface, with a "scale" point attribute whose x is the river width (metres,
as sent by the Houdini Engine curve input).  The HDA makes:
  * a faceted water ribbon (a few strips across, small seeded ripples);
  * cascades wherever the water drops more than "Cascade Drop" between two
    points: those steep strips get the white-water material;
  * a ring of foam and a few low-poly boulders at the foot of every cascade.
Materials come from the prim attribute unreal_material.  Saved to <project>/Houdini.
"""
import math
import os

import hou

HDA_PATH = os.path.join(os.path.expanduser("~"), "Documents", "Unreal Projects", "InteractiveWorldBuilder",
                        "Houdini", "iwb_river.hda").replace("\\", "/")
os.makedirs(os.path.dirname(HDA_PATH), exist_ok=True)

BUILD = r'''
import math
import random

node = hou.pwd()
geo = node.geometry()
source = node.inputs()[0].geometry() if node.inputs() and node.inputs()[0] else None
geo.clear()
WATER = "/Game/WorldBuilder/Materials/M_IWB_Water.M_IWB_Water"
CASCADE = "/Game/WorldBuilder/Materials/M_IWB_Cascade.M_IWB_Cascade"
FOAM = "/Game/WorldBuilder/Materials/M_IWB_Foam.M_IWB_Foam"
ROCK = "/Game/WorldBuilder/Materials/M_IWB_RiverRock.M_IWB_RiverRock"
material = geo.addAttrib(hou.attribType.Prim, "unreal_material", "")
rng = random.Random(node.evalParm("../seed"))
strips = max(2, node.evalParm("../strips"))
ripple = node.evalParm("../ripple")
drop_limit = node.evalParm("../cascade_drop")
foam_scale = node.evalParm("../foam_size")


def width_of(point):
    try:
        scale = point.attribValue("scale")
        return max(0.5, abs(scale[0]))
    except hou.OperationFailed:
        return node.evalParm("../default_width")


def quad(a, b, c, d, mat):
    poly = geo.createPolygon()
    for p in (a, b, c, d):
        poly.addVertex(p)
    poly.setAttribValue(material, mat)
    return poly


def disc(center, radius, height, sides, mat, wobble=0.0):
    middle = geo.createPoint()
    middle.setPosition(hou.Vector3(center[0], center[1] + height, center[2]))
    ring = []
    for i in range(sides):
        angle = math.tau * i / sides
        r = radius * (1.0 + rng.uniform(-wobble, wobble))
        p = geo.createPoint()
        p.setPosition(hou.Vector3(center[0] + math.cos(angle) * r, center[1] + height * 0.3, center[2] + math.sin(angle) * r))
        ring.append(p)
    for i in range(sides):
        tri = geo.createPolygon()
        for p in (middle, ring[(i + 1) % sides], ring[i]):
            tri.addVertex(p)
        tri.setAttribValue(material, mat)


def boulder(center, radius):
    """A squat low-poly rock: an irregular ring pyramid with a flat-ish top."""
    sides = rng.choice((5, 6, 7))
    top = geo.createPoint()
    top.setPosition(hou.Vector3(center[0], center[1] + radius * rng.uniform(0.7, 1.0), center[2]))
    ring = []
    for i in range(sides):
        angle = math.tau * i / sides + rng.uniform(-0.3, 0.3)
        r = radius * rng.uniform(0.75, 1.15)
        p = geo.createPoint()
        p.setPosition(hou.Vector3(center[0] + math.cos(angle) * r, center[1] + radius * rng.uniform(0.15, 0.45), center[2] + math.sin(angle) * r))
        ring.append(p)
    base = []
    for p in ring:
        q = geo.createPoint()
        pos = p.position()
        q.setPosition(hou.Vector3(center[0] + (pos[0] - center[0]) * 1.1, center[1] - radius * 0.4, center[2] + (pos[2] - center[2]) * 1.1))
        base.append(q)
    for i in range(sides):
        j = (i + 1) % sides
        tri = geo.createPolygon()
        for p in (top, ring[j], ring[i]):
            tri.addVertex(p)
        tri.setAttribValue(material, ROCK)
        quad(base[i], ring[i], ring[j], base[j], ROCK)


if source is not None:
    curve = next((prim for prim in source.prims() if prim.type() in (hou.primType.Polygon, hou.primType.NURBSCurve, hou.primType.BezierCurve)), None)
    points = [v.point() for v in curve.vertices()] if curve else list(source.points())
    centres = [p.position() for p in points]
    widths = [width_of(p) for p in points]
    rows = []
    for i, c in enumerate(centres):
        a, b = centres[max(0, i - 1)], centres[min(len(centres) - 1, i + 1)]
        tx, tz = b[0] - a[0], b[2] - a[2]
        length = math.hypot(tx, tz) or 1.0
        sx, sz = -tz / length, tx / length
        row = []
        for k in range(strips + 1):
            u = k / float(strips) - 0.5
            # Banks a touch higher than the middle, and small seeded ripples.
            lift = (abs(u) * 2.0) ** 2 * ripple * 0.6 + (rng.uniform(-ripple, ripple) if 0 < k < strips else 0.0)
            p = geo.createPoint()
            p.setPosition(hou.Vector3(c[0] + sx * u * widths[i], c[1] + lift, c[2] + sz * u * widths[i]))
            row.append(p)
        rows.append(row)
    falls = []
    for i in range(len(rows) - 1):
        drop = centres[i][1] - centres[i + 1][1]
        mat = CASCADE if drop > drop_limit else WATER
        if drop > drop_limit:
            falls.append((i + 1, drop))
        for k in range(strips):
            quad(rows[i][k], rows[i][k + 1], rows[i + 1][k + 1], rows[i + 1][k], mat)
    # Foam and boulders at the foot of each fall (one set per run of steps).
    last = -10
    for index, drop in falls:
        if index - last <= 2:
            last = index
            continue
        last = index
        foot = centres[min(len(centres) - 1, index + 1)]
        w = widths[min(len(widths) - 1, index + 1)]
        disc(foot, w * 0.55 * foam_scale, 0.06, 9, FOAM, wobble=0.25)
        top = centres[index - 1]
        for side in (-1, 1):
            a, b = centres[max(0, index - 2)], centres[index]
            tx, tz = b[0] - a[0], b[2] - a[2]
            length = math.hypot(tx, tz) or 1.0
            sx, sz = -tz / length, tx / length
            for spot, size in ((top, 0.11), (foot, 0.15)):
                offset = side * (w * 0.5 + size * w * 0.25)
                boulder((spot[0] + sx * offset, spot[1], spot[2] + sz * offset), w * size * rng.uniform(0.7, 1.1))
geo.addAttrib(hou.attribType.Global, "iwb_falls", 0)
geo.setGlobalAttribValue("iwb_falls", len(falls))
'''

old = hou.node("/obj/iwb_river_build")
if old:
    old.destroy()
work = hou.node("/obj").createNode("geo", "iwb_river_build")
subnet = work.createNode("subnet", "iwb_river")
inputs = subnet.indirectInputs()
python_sop = subnet.createNode("python", "build_river")
python_sop.setInput(0, inputs[0])
python_sop.parm("python").set(BUILD)
facets = subnet.createNode("normal", "facets")
facets.setInput(0, python_sop)
facets.parm("cuspangle").set(1.0)
output = subnet.createNode("output", "out")
output.setInput(0, facets)
output.setDisplayFlag(True)
subnet.layoutChildren()

asset = subnet.createDigitalAsset(name="iwb_river", hda_file_name=HDA_PATH, description="IWB Low Poly River",
                                  min_num_inputs=1, max_num_inputs=1)
definition = asset.type().definition()
group = definition.parmTemplateGroup()
for template in (
    hou.IntParmTemplate("seed", "Seed", 1, default_value=(1,), min=0, max=1000),
    hou.IntParmTemplate("strips", "Strips Across", 1, default_value=(3,), min=2, max=8),
    hou.FloatParmTemplate("ripple", "Ripple", 1, default_value=(0.06,), min=0.0, max=0.5),
    hou.FloatParmTemplate("cascade_drop", "Cascade Drop (m)", 1, default_value=(0.9,), min=0.1, max=10.0),
    hou.FloatParmTemplate("foam_size", "Foam Size", 1, default_value=(1.0,), min=0.0, max=3.0),
    hou.FloatParmTemplate("default_width", "Default Width (m)", 1, default_value=(4.0,), min=0.5, max=40.0),
):
    group.append(template)
definition.setParmTemplateGroup(group)
definition.save(HDA_PATH)

# Quick self-test: a stepped, winding curve with two falls.
test = work.createNode("python", "test_curve")
test.parm("python").set("""
import math
geo = hou.pwd().geometry()
scale = geo.addAttrib(hou.attribType.Point, "scale", (1.0, 1.0, 1.0))
poly = geo.createPolygon(is_closed=False)
height = 6.0
for i in range(30):
    if i in (10, 20):
        height -= 2.5
    height -= 0.05
    p = geo.createPoint()
    p.setPosition((i * 2.0, height, math.sin(i * 0.3) * 3.0))
    p.setAttribValue(scale, (4.0, 4.0, 4.0))
    poly.addVertex(p)
""")
asset.setInput(0, test)
asset.cook(force=True)
geometry = asset.geometry()
print("HDA saved:", HDA_PATH)
if asset.errors() or not geometry:
    print("cook errors:", asset.errors(), [n.errors() for n in asset.allSubChildren() if n.errors()])
print("test river: polygons", len(geometry.prims()), "| falls", geometry.attribValue("iwb_falls"),
      "| materials", sorted(set(p.attribValue("unreal_material").split("/")[-1] for p in geometry.prims())))
work.destroy()
