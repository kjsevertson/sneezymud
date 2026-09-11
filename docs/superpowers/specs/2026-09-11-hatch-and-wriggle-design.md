# Hatch and Wriggle — Design

## The problem

A closed door stops a thief exactly as hard as it stops a warrior, and a
warrior can at least break it down. Picking answers a *locked* door, but a
door that is merely shut and heavy has no thief-flavoured answer at all —
the options are open it, which anyone can do, or go around.

Hatch and Wriggle give the thief a way past a door that leaves the door
alone: cut a way through beside it, and squeeze.

## Shape

Two skills, deliberately split.

**HATCH `<direction>`** cuts an escape hatch in the wall beside a closed
door. It creates a window object in the room, targeted at whatever room
that exit leads to.

**WRIGGLE `<hatch>`** is how a thief gets through one. Anyone may *look*
through a hatch — that falls out of what a window already is — but only a
thief can fit.

The split is the point. The hatch is a hole in the world that everyone can
see through and one class can use, and it stays behind after its maker has
gone.

## Why a window and not a portal

`TWindow` and `TPortal` are both `TSeeThru`, which carries a single
`target_room` and `getTarget()`/`setTarget()`. They differ in exactly one
way that matters: `TPortal` overrides `enterMe()` and `TWindow` does not,
so `TObj`'s base refusal stands — "You can't seem to find a way to enter
that."

That refusal is a feature. A portal is a door for everybody; a window is a
view. Leaving `enterMe()` alone means the hatch cannot be walked through by
whoever wanders in, and Wriggle carries the movement instead, where the
class gate belongs.

Looking already works and needs nothing: `TWindow::lookObj` and `showMe`
both reach `TBeing::windowLook()`, which sends the target room's name,
description and contents to anyone who asks.

## Hatch

Takes a direction. Resolves it with `findDoor()`, the same call `doOpen`,
`doClose` and `doPick` use, then requires `exitp->condition & EXIT_CLOSED`
— there is no hatching an open doorway, though see below for what happens
when the door opens afterward.

The target is the exit's `to_room`, written onto the window with
`setTarget()` when the task finishes rather than when it starts, so an
interrupted cut leaves nothing behind.

The exit carries more than the destination — `door_type`, `weight`,
`lock_difficulty` — and none of it matters. A hatch is cut in the wall
beside the door, not through the door, so what the door is made of has no
say in the work.

**Cutting needs tools:** a hammer and a chisel. The augmentation crafts set
the precedent of borrowing a kit the world already stocks, and stonework is
what this is.

## Wriggle

An agility check, modified by the thief's size: a hobbit goes through what
a half-ogre does not.

On the way through there is a chance to damage the hatch. Objects already
carry structure points, so this is the same wear any worked item takes, and
a hatch cut to nothing closes for good. That gives it two clocks at once —
it decays on its own, and it wears out under traffic — which means a hatch
is a route with a limited number of uses rather than a permanent second
door.

Wriggling costs movement. Whether it is instant or a short task is not yet
settled — a task makes it interruptible and gives the room something to
watch, an instant squeeze makes it a genuine escape.

Movement follows the tail of `TPortal::enterMe()` rather than reinventing
it: the destination's mob limit, the flying-sector transition, then
`--(*ch)`, `thing_to_room(ch, getTarget())`, and a look at the far side.

## A hatch is two objects

A hole in a wall is visible from both sides, so Hatch cuts two windows —
one in each room, each targeted at the other. Either can be looked
through, either can be wriggled through, and they live and die together.

**Pairing.** `TSeeThru` uses only value1: the target room packed into 23
bits, with bit 23 reserved for the window's random/fuzzy variant.
`getFourValues()` writes the other three as zero and ignores them on load,
so a window has three unused slots. Hatch generates a pair id, stamps it on
both halves in value2, and destroying one searches the target room for a
window carrying the same id.

This needs an override of `assignFourValues`/`getFourValues` on `TWindow`,
which currently just delegates to `TSeeThru`. Builder-placed windows load
with value2 of zero, which reads as unpaired, so nothing already in the
world is affected.

The alternative — inferring the partner as "the window in my target room
that points back at me" — needs no new field but breaks the moment two
hatches join the same pair of rooms, and makes correctness depend on a rule
enforced somewhere else. The id is cheaper to be sure about.

Collisions only matter inside one room, since that is the only place the
lookup ever searches.

**Both halves are ITEM_NORENT.** A hatch is scenery and belongs to the
room, not to whoever cut it; the flag makes certain it can never be carried
off or stored.

## Decisions taken

**The hatch outlives the door's state.** If someone opens the door the
hatch was cut beside, the hatch stays and keeps working. It is slightly
silly — nobody needs a hatch through an open doorway — but it avoids a set
of checks that would have to ask what the door is doing every time, and it
leaves a player free to make a worse choice than the obvious one, which is
a thing worth protecting.

**The hatch is not the thief's property.** It is scenery once cut. Anyone
may look through it; any thief may wriggle through it.

**A hatch is shared, not a peephole.** It is a hole in a wall, so it is
visible and usable from both sides. That is what makes it two objects
rather than one.

**One hatch to a pair of rooms.** Hatch refuses to cut a second one where
a hatch already joins those two rooms -- a wall has room for the hole it
already has. This also means the pair id never has to disambiguate two
hatches in the same place, though it is kept anyway so that correctness
does not rest on the refusal being remembered.

## A hatch to nowhere

If a hatch's target room stops existing, the hatch breaks. `windowLook()`
already notices the case — it sends "You see only an empty void" and logs
the dangling vnum — so that is the condition to hang it on: a window whose
target does not resolve is a hole into nothing, and should close rather
than sit there.

That covers a destroyed room from both sides. The half standing *in* the
lost room goes with the room; the half in the surviving room finds its
target gone and breaks itself.

Exits being rewritten underneath a hatch is not a case worth handling —
nothing in the game changes an exit's destination at runtime.

## Open questions

- Is Wriggle instant, or a short task?
- What does a failed Wriggle cost beyond the movement it already spends?
