# Kitten Keeper: design decisions (running list)

## Art direction (decided 2026-10-31)
- Cats: hand-pixel look (the "pixel3" concept). Baked sprite sheets in flash, not runtime-drawn.
- Each body type has its own frames: skinny ~0.85x normal, normal 1.0x, chonk ~1.5x and visibly huge. No sharing between body types.
- Breeds: markings/palettes on top of the shapes. Keep all 7 unless art effort forces a cut (Sphynx is cheapest to drop).
- Default house: **Grimy Cartoon** (static baked room). Only the window is live (sky/time of day, clouds, trees, butterflies, birds, weather).
- Room lighting: two baked variants (day, night+lamp) blended at dusk/dawn. Sunbeam napping removed.
- Litter box and food bowl sit on opposite sides of the room. Cup of water on the table is a separate sprite (knock-it-off game).

## Sleeping spots
1. Start: cat sleeps on the bare floor.
2. Buy **floor cat bed** (cushioned): replaces box-sleeping.
3. Later buy **cat tree with bed** (expensive): cat sleeps up top.
4. **Cat tree (no bed)** also sold separately.

## Boxes
| stage | box | cost | lifetime |
|---|---|---|---|
| kitten | tissue box | free; replacement = very small fee | kitten destroys it after 1 hour (ASSUMED 1 hour, confirm) |
| adult (on growing up) | shoebox | free first one; cheaper than cardboard | 8 h (enough to sleep and play in) |
| any | cardboard box | more than shoebox | 24 h |
| any | permanent box | expensive vs other boxes | forever |
- Tissue box is no longer offered once the cat is an adult.
- A cushioned floor bed replaces sleeping in the box. The box stays (unless sold).

## Magic Cat (new mini-game, uses the box)
- Box is shown upside down. Player guesses: cat inside or not.
- Reveal: box flips. Cat inside = win; empty box = disappointing meow.
- Needs a box owned; hairball reward on correct guess (amount TBD, keep stingy).

## Pipeline plan
1. Sprite pipeline + baked cat (sit, walk, sleep, eat x 3 body types), compared side by side with current cat in the web build.
2. Static room + live window.
3. Decoration shop + cat tree + sleeping/box systems above.
4. Sleep/power work (screen off fast, deep sleep, timed wake).

## Litter box minigame (new, 2026-10-31)
- Slide a finger across the box to scoop the droppings into the scoop.
- Then **tilt the device** to carry/tip the load over a trash can and drop it in.
- Miss the can: the mess lands on the floor. Player must scoop it off the floor and try again (reuses the floor-mess flow).
- Replaces the current tap-to-scoop. Auto-cleaning boxes (shop upgrades) skip the game.

## Room implementation (step 2, in progress)
- Static baked room image per lighting (day, night); window glass is a cut-out filled live each few frames.
- Default objects: ONLY the window, the table with a cup of water, the litter box and the food bowl (opposite sides). Everything else is bought.
