# Script properties

A Lua script can offer variables to whoever places it. The editor shows a control
for each one in **Properties > Lua Script > Properties**, the scene keeps what was
set, and the script reads it as a field of the instance.

```lua
local Door = class('Door')

Door.properties = {
    { name = "speed",  value = 2.5, min = 0, max = 10 },             -- number: a slider (no range: a drag)
    { name = "count",  value = 3, type = "int" },                    -- whole number
    { name = "open",   value = false },                              -- checkbox
    { name = "label",  value = "Door" },                             -- text
    { name = "offset", value = Vec3.new(0, 1, 0) },                  -- Vec2, Vec3, Vec4: that many numbers
    { name = "tint",   value = Vec4.new(1, 1, 1, 1), type = "color" }, -- colour picker (Vec3 or Vec4)
    { name = "mode",   value = "auto", options = { "auto", "manual" } }, -- drop-down
    { name = "sound",  value = "", type = "asset" },                 -- a project file (drop it from Assets)
}

function Door:init(owner)
    print(self.speed, self.open, self.offset.y)   -- what was set in the editor
end

return Door
```

- The kind of `value` decides the control. `type` narrows it (`"int"`, `"color"`, `"asset"`).
- Optional per entry: `label` (what the control is called) and `tip` (shown on hover).
- Short forms `speed = 2.5` and `speed = { value = 2.5, min = 0 }` also work; those are listed alphabetically.
- Values are written into the instance before `init()`, and again each time Play starts.
- Saving the script updates the panel within a second: new properties appear with the
  script's value, removed ones go, and a property whose name and kind are unchanged
  keeps what was set.
- In the scene file they are under the component: `"properties": { "speed": 7.0, ... }`.
- Editor commands: `script_properties {name, script}` and
  `set_script_property {name, property, value, script}` (undoable, like the control).

Not covered yet: properties on a scene's main script, and references to other
objects in the scene.
