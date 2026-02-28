# Bare Metal to AI-driven

A time stamp about the quality and performance within the game development driven by LLMs.
It was 2025 when we started experimenting and embracing multiple AI-driven tools in my formal work for multiple game-related projects.



## Failed experiments (2025)

On-device LLMs, privacy, speech to speech, restricted content... there were some of the
requirements and assumptions that were on the table for an educational mobile app.

A few of the models we tested with n

 * Llama 3.2
 * gemma2.2
 * whisper
 * gpt-40
 * Maxrosoft Azure AI Speech
 * Keen ASR
 * Unity Sentis
 * Vost

Moving forward a year later, these specific requirements now read more like the rule than the exception
for any of the LLMs that wants become future proof.


## Complete games

2026 I started experimented with Claude 3.5 Sonnet (free version) with real challenges:  building complete and classic game clones. 
Upfront I want to be clear, we have a complete functional game prototype, still far from a complete and enjoyable product.

The types of classic game clones ranging from Pong, Ms. PacMan, Mario Bros and increasing difficulty. 

### MS. Claudia (Ms. Pac-Man Clone)

Requirements
C/C++, no external libraries, no sprites, pure code

The Nerd Stats:
 - ~1hr to reach a functional prototype.
 - Iterations: ~20 prompts to reach this stage.
 - Output: 1,000 lines of maintainable (ok-ish) code.
 - Tech Stack: Claude arbitrarily chose GDI rendering—it’s modular enough to swap easily.
 - The Catch: The level of detail required in prompting is extreme. You still need to know how to build to guide the AI.
 - Basic SFX, VFX included!
 - Ironically. really bad ghost AI!

You can try the game your self here.

Now the real problem, 100% sure what comes next is the norm out there is social media: polishing.

I spent considerable amount of hours trying to improve the in-game AI, no success.
 - Kept repeating same mistakes over and over
 - Reverting existant code with new solutions
 - Redoing previous work
 - Breaking unrelated parts of the game
 - Not understanding instructions

Want to stress how ironic is that the in-game AI was still the biggest challenge for the LLM to solve.
It doesn't end here, we will be performing the same and more challenging projects with a Claude premium.




