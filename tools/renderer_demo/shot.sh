#!/bin/sh
# shot.sh <name> "<x y z yaw pitch>" : run the demo at a view, capture it once loaded, write <name>.png/.jpg in the scratchpad
S=/private/tmp/claude-501/-Users-peixinho/96df5880-9dba-4a52-a772-24c4a69f8df9/scratchpad
cd ~/dev/cpp/Pyros3D_master/tools/renderer_demo
pkill -f renderer_demo 2>/dev/null; sleep 0.5
(PYROS_NO_BOUGHS=$NOB PYROS_CAM="$2" PYROS_SECONDS=60 PYROS_PROFILE_LOG=/tmp/claude-501/r2_$1.log ../../build_r2/renderer_demo demo_scene.json > /tmp/claude-501/r2_$1.txt 2>&1 &)
for i in $(seq 1 200); do grep -q 'loaded in' /tmp/claude-501/r2_$1.txt && break; sleep 0.5; done
W=$(yabai -m query --windows 2>/dev/null | jq -r '.[] | select(.app=="renderer_demo") | .id' 2>/dev/null | head -1)
sleep ${3:-5}
screencapture -x -o -l $W $S/$1.png
pkill -f renderer_demo
/usr/bin/python3 -c "
from PIL import Image; im=Image.open('$S/$1.png').convert('RGB'); im=im.crop((0,32,im.width,im.height)); im.resize((1400,int(1400*im.height/im.width))).save('$S/$1.jpg',quality=88)"
