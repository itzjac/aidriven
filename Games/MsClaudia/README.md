# Requirements

C/C++, no external libraries, no sprites, pure code

# Nerd Stats

 - ~1hr prompt with Claude to reach a functional prototype\
 - Iterations: ~20 prompts to reach this stage
 - Output: 1,000 lines of maintainable (ok-ish) code
 - Tech Stack: Claude arbitrarily chose GDI rendering—it’s modular enough to swap easily
 - The Catch: The level of detail required in prompting is extreme. You still need to know how to build to guide the AI
 - Basic SFX, VFX included!
 - Ironically. really bad ghost AI!

# Compilation

Platform: Windows x64 (as it used the GDI32)

Compiler: Visual Studio 2022 (possibly any previous version)


`cl game.cpp -EHa -O2 user32.lib gdi32.lib winmm.lib`


# Controls

 ## Keyboard


 - UP, DOWN, LEFT, RIGHT ARROW: Ms Claude movement

 - M: Mute

 - N: cheat and go to next level

<img width="1338" height="921" alt="image" src="https://github.com/user-attachments/assets/61108770-7c31-46b6-b581-e19a04c15738" />


<img width="667" height="799" alt="image" src="https://github.com/user-attachments/assets/88a9981e-3566-44b5-b860-ca03d4868a2c" />

# AI-Driven code

As mentioned there are two big issues to get the MS. Claudia to a polish phase: in-game AI, maze data.


I spent considerable amount of hours trying to improve the in-game AI and trying to 
extract the hardcoded maze data into a mazes.dat file with no success.

The model:

 - Kept repeating same mistakes over and over
 - Reverting existant code with new solutions
 - Redoing previous work
 - Breaking unrelated parts of the game
 - Not understanding instructions

Want to stress how ironic is that the in-game AI was still the biggest challenge for the LLM to solve. 

It doesn't end here, we will be performing the same and more challenging game clones with portability in mind: Linux, iOS and Windows.







