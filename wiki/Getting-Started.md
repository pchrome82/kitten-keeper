# Getting Started

## 1. Flash the firmware

The easiest way is to flash a ready-made release.

1. Go to the [Releases page](https://github.com/pchrome82/kitten-keeper/releases) and download the `.bin` file for your board:
   - AMOLED-1.8: `kitten-keeper-<version>-amoled18.bin`
   - AMOLED-2.16: `kitten-keeper-<version>-amoled216.bin`
2. Open the browser flasher at https://espressif.github.io/esptool-js/ in Chrome or Edge.
3. Connect your board, set the speed to 921600 baud, and flash the file at address `0x0`.
4. If the board is not found, hold **BOOT**, tap **RESET**, release **BOOT**, and try again.

Flashing keeps your save. Your cat, hairballs, outfits and homes are stored on the board, so an update does not wipe them.

## 2. Set the clock

Day and night, birthdays and the seasons all follow the clock on your board. Go to **MENU → SETTINGS → SET CLOCK** and set the date and time. Each cat gets a birthday picked for it, about ten days after it moves in, and you can change it under **SETTINGS → BIRTHDAY**.

## 3. Adopt your first kitten

When you have no cat yet, you start on the adoption screen:

1. **Pick a breed.** Swipe or tap through the breeds. The screen shows each one's Hair, Speed and Strength. All seven regular breeds are free to adopt. See [[Breeds]].
2. **Name your kitten.** Tap **TYPE NAME** to type a name, **RANDOM NAME** for a random one, or **MINE** to bring back the name you typed last time.
3. Tap **ADOPT!**

Your kitten arrives with a random personality (such as Playful or Foodie), and the game tells you what it is when they move in. A new kitten arrives hungry and bored, and it will need the litter box soon.

## 4. The home screen

Your cat lives in its home. Tap the cat to pet it, and hold to make it purr.

The bar along the top shows three meters, and the number on the right is your cat's Health:

| Icon | Meter | Meaning |
|---|---|---|
| Fish | Fullness | How fed your cat is |
| Ball | Fun | How entertained your cat is |
| Moon | Energy | How rested your cat is |

A meter turns red when it drops below 25.

The toolbar along the bottom has five buttons:

| Button | What it does |
|---|---|
| **FEED** | Opens the feeding game. Tilt the device to pour kibble into the bowl. |
| **PLAY** | Opens the games menu, which also has a CAT NAP button. See [[Mini-Games]]. |
| **POO** | Opens the litter box. Scoop up poops and drop them in the can. |
| **SHOP** | Opens the shop. See [[Shop]]. |
| **MENU** | Opens the menu: Stats, Change Breed, Alarm Clock, Journal, Settings and Help. |

A red dot on a button means that need wants you now. The FEED dot shows when your cat is getting hungry, the PLAY dot when it is getting bored, and the POO dot when there is mess to clean.

## 5. The menu

| Option | What it does |
|---|---|
| **STATS** | Your cat's name, breed, age, mood, personality, all its meters, weight, and how many things it has caught. See [[Care and Needs]]. |
| **CHANGE BREED** | Switch your cat to another breed. It costs 10 hairballs each time. See [[Breeds]]. |
| **ALARM CLOCK** | Set a time. Your cat meows you awake at that time each morning. |
| **JOURNAL** | Milestones you have reached, and a photo album of every cat you have raised. |
| **SETTINGS** | Volume, brightness, screen-off time, the clock, screen rotation, birthdays and reset. |
| **HELP** | Four pages that explain the bars, the weight and mood, and the game scores. |

### Settings in brief

- **Volume** and **Bright** sliders, and **Screen off** to set how long the screen stays on without a touch.
- **SET CLOCK** sets the date and time.
- **ROTATE** switches between ON (the tilt sensor turns the screen), OFF (always the normal way up) and FLIP (always upside down).
- **TOUCH FIX** (on boards with a touch screen) calibrates touch from five taps. Use it if your taps land in the wrong place. The fix stays after a game reset.
- **BIRTHDAY** changes your cat's birthday.
- **RESET GAME** erases everything and starts from scratch. It asks you to confirm first.

## 6. Shake or lift to wake

Your cat sleeps at night. A touch wakes it. If you shake or lift the device while your cat is asleep, it wakes up grumpy. See [[Care and Needs]] for more on sleep.
