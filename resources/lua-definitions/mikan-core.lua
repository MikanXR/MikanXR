---@meta
-- Mikan scripting API — core types, globals, and coroutine scheduler.
-- This file is a LuaLS definition file; it contains no executable code.
-- Keep in sync with: src/Editor/Scripting/CommonScriptContext.cpp (bindCommonScriptFunctions,
-- addLuaCoroutineScheduler) and src/Editor/Scripting/LuaMath.cpp (LuaVec3f::bindFunctions,
-- LuaQuatf::bindFunctions).

------------------------------------------------------------------------
-- Coroutine scheduler globals (injected by addLuaCoroutineScheduler)
------------------------------------------------------------------------

--- Wait for `frame_duration` frames before resuming the current coroutine.
---@param frame_duration number
function wait_frames(frame_duration) end

--- Wait until the next frame before resuming the current coroutine.
function wait_next_frame() end

--- Wait for `seconds_duration` seconds before resuming the current coroutine.
---@param seconds_duration number
function wait_seconds(seconds_duration) end

--- Returns the duration of one frame in seconds (1/fps).
---@return number
function get_frame_delta_seconds() end

--- Start a new coroutine. The task function runs asynchronously across frames.
---@param task function
function start_coroutine(task) end

------------------------------------------------------------------------
-- Debugger
------------------------------------------------------------------------

--- Trigger a pause in the LRDB Lua debugger on the next line event.
--- Use this as a programmatic breakpoint when gutter breakpoints cannot
--- be set through the VS Code extension.
function lrdb_break() end

------------------------------------------------------------------------
-- Vec3f — 3-component float vector
------------------------------------------------------------------------

---@class Vec3f
---@field x number X component
---@field y number Y component
---@field z number Z component
---@overload fun(x: number, y: number, z: number): Vec3f
Vec3f = {}

--- Add two vectors.
---@param v Vec3f
---@return Vec3f
function Vec3f:__add(v) end

--- Subtract two vectors.
---@param v Vec3f
---@return Vec3f
function Vec3f:__sub(v) end

--- Scale uniformly by a scalar.
---@param s number
---@return Vec3f
function Vec3f:scaleUniform(s) end

--- Scale non-uniformly by a per-component vector.
---@param s Vec3f
---@return Vec3f
function Vec3f:scaleNonUniform(s) end

--- Return the Euclidean length of the vector.
---@return number
function Vec3f:length() end

--- Return a unit-length copy of the vector.
---@return Vec3f
function Vec3f:normalize() end

--- Compute the dot product of two vectors.
---@param a Vec3f
---@param b Vec3f
---@return number
function Vec3f.dot(a, b) end

--- Compute the cross product of two vectors.
---@param a Vec3f
---@param b Vec3f
---@return Vec3f
function Vec3f.cross(a, b) end

------------------------------------------------------------------------
-- Quatf — unit quaternion rotation
------------------------------------------------------------------------

---@class Quatf
---@field w number W component
---@field x number X component
---@field y number Y component
---@field z number Z component
---@overload fun(w: number, x: number, y: number, z: number): Quatf
Quatf = {}

--- Compose (multiply) two quaternions.
---@param q Quatf
---@return Quatf
function Quatf:__mul(q) end

--- Rotate a vector by this quaternion.
---@param v Vec3f
---@return Vec3f
function Quatf:rotateVec3f(v) end

--- Return the multiplicative inverse of this quaternion.
---@return Quatf
function Quatf:inverse() end

--- Return the norm (length) of this quaternion.
---@return number
function Quatf:length() end

--- Return a unit-length copy of this quaternion.
---@return Quatf
function Quatf:normalize() end

------------------------------------------------------------------------
-- ScriptBehavior : the class every project script defines
------------------------------------------------------------------------

--- The base class of the one class each project script file defines. All
--- project scripts share one Lua state, but a script component holds its own
--- instance of its file's class, so two components can use the same file with
--- different parameters. The file's class is the value the chunk returns, or
--- else the last class created while the chunk ran.
---
--- Parameters: every field `init` assigns whose name does not start with `_`
--- and whose value is a boolean, integer (`4`), number (`4.0`), string, Vec3f,
--- or ComponentRef becomes an editor-editable, persisted parameter, shown in
--- declaration order. A value stored in the project for that component wins
--- over the default; otherwise the default is adopted and stored. Every edit in
--- the script panel rewrites the field on the instance, so read parameters off
--- `self` inside method bodies rather than caching them in `init`. Fields of
--- other types stay private to the script.
---
--- Methods by naming convention:
--- - `Trigger_<Name>(self, args)`: a trigger called `<Name>`, shown as a
---   button in the script panel and reachable through the client API and the
---   automation server. `args` is a table of string arguments, empty when the
---   caller supplied none.
--- - `HttpTrigger_<Name>(self, args)`: bound to an HTTP route in the HTTP
---   Triggers panel; `args` carries the request's query string, so
---   `?user=bob&tier=3` arrives as `{ user = "bob", tier = "3" }`.
--- - `OnMessage(self, message)`: receives a client script message; return
---   true when handled to stop it reaching later scripts.
--- - `SequenceStart(self, sequence)`, `SequenceUpdate(self, sequence,
---   timeSinceStart, deltaSeconds)`, `SequenceStop(self, sequence)`: drive a
---   DMXSequenceComponent that picked this script component. SequenceUpdate
---   is what makes the component pickable; the other two are optional.
---
--- An error inside a method is reported and leaves the project scripts
--- running. An error while the file loads or inside `init` unloads them all.
---@class ScriptBehavior
---@field super ScriptBehavior The parent class
ScriptBehavior = {}

--- Create a subclass. `ScriptBehavior()` is shorthand for
--- `ScriptBehavior:extend()`; an unnamed class takes its file's name.
---@param name string|nil
---@return ScriptBehavior
function ScriptBehavior:extend(name) end

--- Override to declare parameters as `self.<name> = <default>`. Runs once per
--- script component when the scripts load. Call `MyClass.super.init(self)` to
--- run a parent class's init.
function ScriptBehavior:init() end

--- True when this instance's class is cls or derives from it.
---@param cls ScriptBehavior
---@return boolean
function ScriptBehavior:is(cls) end

--- A parameter default that names a scene component of the given class (a
--- `k_componentClassName` string such as "StageComponent"). The script panel
--- shows it as a dropdown of that class's live components plus none. After
--- init the field holds the component handle typed by its class, or nil when
--- nothing is selected or the selected object no longer exists. Mikan rewrites
--- the field on every panel edit and on object creation and destruction, so
--- nil-check it inside method bodies.
---@param componentClassName string
---@return ComponentRef
function ComponentRef(componentClassName) end

---@class ComponentRef

------------------------------------------------------------------------
-- ScriptContext : messaging helpers and enums
------------------------------------------------------------------------

---@class ScriptContext
ScriptContext = {}

--- Broadcast a string message to all registered message handlers in all contexts.
---@param message string
function ScriptContext.broadcastMessage(message) end

------------------------------------------------------------------------
-- ScriptContext.CullMode enum
------------------------------------------------------------------------

--- Stencil cull-mode values. Use with StencilComponent.cullMode.
---@alias ScriptContext.CullMode
---| 0 # none — no culling
---| 1 # zAxis — cull along Z axis
---| 2 # yAxis — cull along Y axis
---| 3 # xAxis — cull along X axis

ScriptContext.CullMode = { none = 0, zAxis = 1, yAxis = 2, xAxis = 3 }
