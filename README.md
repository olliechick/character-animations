# Animated models

There are three animated models.

1. An army pilot running
2. A mannequin running
3. A dwarf, who either kneels down then gets up, or walks

A quick video of the three animations is at https://youtu.be/7TaeUtlKNcg.

## Build instructions

```bash
g++ -Wall -g -o animationX animationX.cpp -lGL -lGLU -lglut -lGLEW -lassimp -lIL -ILU
```

Then run it using `./animationX`.

Replace `X` with either `1`, `2`, or `3`, depending on which animation you want to run.

## Instructions for use

The up and down arrow keys move the camera forwards and backwards, respectively.
Page Up and Page Down move the camera forwards and backwards ten times as fast.

The left and right arrow keys move the camera left and right, respectively, in a circle around the model.
Home and End move the camera left and right ten times as fast.

The R key resets the camera and animation to their initial state.

For the dwarf model (animation 3), the 1 and 2 keys switch between the two animations.
