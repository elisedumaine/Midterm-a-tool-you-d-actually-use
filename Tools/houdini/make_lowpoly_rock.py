"""Build the IWB low-poly rock HDA in the open Houdini (run with Tools/houdini.py -f).

A faceted rock: a coarse sphere, squashed, pushed around by noise, then
given hard (cusped) normals so every face reads as a flat low-poly facet.
Parameters: Seed, Size, Roughness, Detail.  Saved to <project>/Houdini.
"""
import os

import hou

HDA_PATH = os.path.join(os.path.expanduser("~"), "Documents", "Unreal Projects", "InteractiveWorldBuilder",
                        "Houdini", "iwb_lowpoly_rock.hda").replace("\\", "/")
os.makedirs(os.path.dirname(HDA_PATH), exist_ok=True)

work = hou.node("/obj").createNode("geo", "iwb_rock_build")
subnet = work.createNode("subnet", "iwb_lowpoly_rock")
inner = subnet
sphere = inner.createNode("sphere")
sphere.parm("type").set("polymesh")
sphere.parm("rows").setExpression('ch("../detail") + 2')
sphere.parm("cols").setExpression('ch("../detail") * 2 + 2')
squash = inner.createNode("xform", "squash")
squash.setInput(0, sphere)
squash.parm("sy").set(0.62)
noise = inner.createNode("mountain::2.0", "shape")
noise.setInput(0, squash)
noise.parm("height").setExpression('ch("../roughness")')
noise.parm("elementsize").set(0.9)
noise.parm("offsetx").setExpression('ch("../seed") * 13.37')
noise.parm("offsety").setExpression('ch("../seed") * 7.11')
size = inner.createNode("xform", "size")
size.setInput(0, noise)
size.parm("scale").setExpression('ch("../size")')
# Sit on the ground: the lowest point at y = 0.
ground = inner.createNode("xform", "ground")
ground.setInput(0, size)
ground.parm("ty").setExpression('-$YMIN')
facets = inner.createNode("normal", "facets")
facets.setInput(0, ground)
facets.parm("cuspangle").set(1.0)
output = inner.createNode("output", "out")
output.setInput(0, facets)
output.setDisplayFlag(True)
inner.layoutChildren()

asset = subnet.createDigitalAsset(name="iwb_lowpoly_rock", hda_file_name=HDA_PATH, description="IWB Low Poly Rock",
                                  min_num_inputs=0, max_num_inputs=0)
definition = asset.type().definition()
group = definition.parmTemplateGroup()
for template in (
    hou.IntParmTemplate("seed", "Seed", 1, default_value=(1,), min=0, max=1000),
    hou.FloatParmTemplate("size", "Size", 1, default_value=(1.0,), min=0.1, max=10.0),
    hou.FloatParmTemplate("roughness", "Roughness", 1, default_value=(0.35,), min=0.0, max=1.0),
    hou.IntParmTemplate("detail", "Detail", 1, default_value=(5,), min=3, max=12),
):
    group.append(template)
definition.setParmTemplateGroup(group)
definition.save(HDA_PATH)
geometry = asset.displayNode().geometry()
print("HDA saved:", HDA_PATH)
print("rock polygons:", len(geometry.prims()), "| bounds:", geometry.boundingBox())
work.destroy()
