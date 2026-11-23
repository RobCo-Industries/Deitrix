# Culvert

Culvert is a pipe-turning puzzle. It ships on the Deitrix card as the holotape `/PIPBOY/HOLO/CULVERT.HTP`.

## Start it

In Deitrix, go to DATA, then TAPES. Pick "Culvert" and press SELECT. Deitrix stops the radio and writes its saves first. When you quit, Deitrix shows TAPES again with "Tape ended, status 0." under the list.

The title screen offers "New game" and "Quit". After you solve a level, it also offers "Continue (level N)", where N is the next level you have not solved.

## The goal

The grid holds square tiles. Each tile holds a pipe piece: an end cap, a straight, an elbow, a tee or a cross. The tile with the ring is the source. Water flows from the source through the pipes that meet.

Pipes the water reaches are bright. The others are dim. You solve the level when every tile is bright and no pipe opens into a wall or into a side with no pipe. Every level has a solution with no loops.

Levels are not stored. The level number alone sets the puzzle, so level 12 is the same on every device. The grid starts at 5 by 4 tiles and grows every three levels, up to 10 by 7 from level 25 on. There is no last level.

## Keys

| Key | Play | Menus |
|---|---|---|
| UP, DOWN, LEFT, RIGHT | Move the cursor. Hold to repeat. The cursor stops at the edges. | UP and DOWN move the bar |
| SELECT | Turn the tile a quarter turn clockwise. Holding it turns once. | Choose |
| Dial push | Same as SELECT. Holding it turns once. | Choose |
| Dial | Turn the tile one quarter per step, in the dial's direction | Move the bar |
| BACK | Open the pause menu | Title: quit. Pause menu: resume. |

Each quarter turn counts as one move. A dial event of several steps turns the tile one quarter at a time. If a quarter solves the level, the turn stops there and the other steps are dropped. Four quarters bring a tile back, so a fast spin applies only its step count modulo 4, in the dial's direction. A spin of 5 steps turns one quarter and counts one move. A spin of 8 steps does nothing. The status row shows the level, the moves and the time as `m:ss`. The clock stops while the pause menu is open.

When the level is solved, the network flashes and a short tune plays. Then the screen shows "Level N done in M moves". SELECT or the dial push starts the next level. The on-screen hints name SELECT only.

## The pause menu

- Resume: back to the level.
- Restart level: the same level from the start, with no moves and the clock at 0:00.
- Quit: leave the tape. You go back to TAPES in Deitrix.

## What is saved

Culvert keeps one save, `CULVERT`, in `/PIPBOY/SAVE`. It holds the highest level you reached and, for each grid size, the fewest moves you solved a level of that size in. It is written each time you solve a level. A missing or damaged save starts fresh, with no message.

## Sound

A turn plays a short tick. A turn that brings water to new tiles plays two rising notes instead. A solved level plays a short tune. The game plays the same with the volume at 0.
