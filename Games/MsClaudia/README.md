

# Requirements

C/C++, no external libraries, no sprites, pure code

# Nerd Stats

 - ~1hr to reach a functional prototype.
 - Iterations: ~20 prompts to reach this stage.
 - Output: 1,000 lines of maintainable (ok-ish) code.
 - Tech Stack: Claude arbitrarily chose GDI rendering—it’s modular enough to swap easily.
 - The Catch: The level of detail required in prompting is extreme. You still need to know how to build to guide the AI.
 - Basic SFX, VFX included!
 - Ironically. really bad ghost AI!

# Compilation
Any version for Windows x64 as it used the GDI32

`cl game.cpp -EHa -O2 user32.lib gdi32.lib winmm.lib`


# Controls

 ## Keyboard


 - UP, DOWN, LEFT, RIGHT ARROW: Ms Claude movement

 - M: Mute

 - N: cheat and go to next level

<img width="1338" height="921" alt="image" src="https://github.com/user-attachments/assets/61108770-7c31-46b6-b581-e19a04c15738" />


<img width="667" height="799" alt="image" src="https://github.com/user-attachments/assets/88a9981e-3566-44b5-b860-ca03d4868a2c" />






