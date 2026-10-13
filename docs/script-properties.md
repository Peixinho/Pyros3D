# Script properties

A field of a script's class is a variable the editor can set. The editor shows a
control for each one in **Properties > Lua Script > Properties**, the scene keeps
what was set, and the script reads it as its own field.

```lua
local Door = class('Door')

Door.speed  = 2.5                    -- @range 0 10  Turns a second
Door.count  = 3                      -- @int
Door.open   = false
Door.label  = "Door"
Door.offset = Vec3.new(0, 1, 0)
Door.tint   = Vec4.new(1, 1, 1, 1)   -- @color
Door.mode   = "auto"                 -- @options auto manual
Door.sound  = ""                     -- @asset
Door.target = ""                     -- @object

Door._timer = 0                      -- not shown: the name begins with _

function Door:init(owner)
    print(self.speed, self.open, self.offset.y)   -- what was set in the editor
end

return Door
```

The kind of the value is the kind of the control:

| Value | Control | Comment on the same line |
|---|---|---|
| number | drag | `@range min max` makes it a slider; `@int` a whole number |
| `true` / `false` | checkbox | |
| string | text | `@options a b c` makes it a drop-down; `@asset` a project file; `@object` an object of the scene (the script gets the object itself, or `nil`) |
| `Vec2` / `Vec3` / `Vec4` | that many numbers | `@color` makes a `Vec3`/`Vec4` a colour picker |

- Any other text in the comment is the control's tip; `@label "Shown name"` renames it.
- Controls are listed in the order the script gives the fields.
- Functions, tables and names beginning with `_` are never shown.
- Values are written into the instance before `init()`, and again each time Play starts.
- Saving the script updates the panel within a second: new fields appear with the
  script's value, removed ones go, and a field whose name and kind are unchanged
  keeps what was set.
- Only values that were set are kept by the scene, under the component:
  `"properties": { "speed": 7.0 }`. A variable that was never set follows the script: change
  its value in the `.lua` file and every object that left it alone changes with it. A set
  value shows a `*` beside its control; right-click the control to go back to the script's value.
- Editor commands: `script_properties {name, script}` and
  `set_script_property {name, property, value, script}` (undoable, like the control).

A table says the same entry by entry, for when a comment is not enough:
`Door.properties = { { name = "speed", value = 2.5, min = 0, max = 10, tip = "..." }, ... }`.
Where both name a variable, the table wins.

A scene's main script offers variables the same way. They are shown at the top of
the scene's own properties (nothing selected), saved in the scene as
`"mainScriptProperties"`, and reached by the editor commands with an empty `name`.

An object's own properties list its scripts' variables under each attached script,
so the script component does not have to be selected to set them.
