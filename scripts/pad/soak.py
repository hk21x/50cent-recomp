#!/usr/bin/env python3
# Soak pad script for 50 Cent: Bulletproof (scripts/game.env SOAK_PAD): soak.py SECONDS [SEED] (or SOAK_SEED=n)
# From a first boot the route of scripts/pad/boot.txt into the first mission (the alley), then play at random until
# SECONDS: walk and run about, turn the camera, fire (RT), crouch (LT held), jump (A), grab or execute (B), the action
# and reload (X), evade (Y), heal (White), the weapon inventory (Black), and pause and unpause. START may open the
# PDA on a new message (Popcorn's camera modes), where only B goes back: B four times reaches the pause menu's top
# (B does nothing there), and A takes its Continue. Times are vertical
# blanks at 60 a second.
import random
import sys

secs = int(sys.argv[1]) if len(sys.argv) > 1 else 600
import os
rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else int(os.environ.get("SOAK_SEED", "1")))
end = secs * 60
lines = ["# boot.txt's route: the logos and teaser, New Game, Thug, no profile, the intro and cutscene skipped",
         "1700 START 10", "2300 START 10", "2550 START 10", "2850 START 10",
         "3500 A 8", "4150 A 8", "4800 X 8", "5500 UP 8", "5600 A 8", "6150 START 10", "6850 START 10"]
t = 7800
moves = ["LS", "LS", "LS", "LSD", "LSL", "LSR", "LS+LSL", "LS+LSR"]
while t < end:
    r = rng.random()
    if r < 0.35:  # walk somewhere
        lines.append(f"{t} {rng.choice(moves)} {rng.randint(60, 300)}")
    elif r < 0.47:  # turn the camera
        lines.append(f"{t} {rng.choice(['RSL', 'RSR', 'RS', 'RSD'])} {rng.randint(20, 90)}")
    elif r < 0.62:  # fire, sometimes crouched
        lines.append(f"{t} {rng.choice(['RT', 'RT', 'LT+RT'])} {rng.randint(10, 90)}")
    elif r < 0.80:  # the face buttons: jump, grab or execute, action and reload, evade
        lines.append(f"{t} {rng.choice(['A', 'B', 'X', 'X', 'Y'])} {rng.randint(8, 20)}")
    elif r < 0.86:  # heal, the weapon inventory
        lines.append(f"{t} {rng.choice(['WHITE', 'BLACK'])} {rng.randint(30, 120)}")
    elif r < 0.90:  # pause (the PDA may open on a new message), back out with B to the top, Continue (A)
        lines.append(f"{t} START 10")
        t += 240
        for _ in range(4):
            lines.append(f"{t} B 8")
            t += 90
        lines.append(f"{t} A 8")
    else:  # crouch and creep
        held = rng.randint(60, 240)
        lines.append(f"{t} LT+LS {held}")
        t += held
    t += rng.randint(30, 150)
print("\n".join(lines))
