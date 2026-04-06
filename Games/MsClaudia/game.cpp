// $(C) Copyright by TheByteCave Inc., 2026 All Rights Reserved. $
// $Creator: Dagon Meister $
// $File: C:\projects\aidriven\games\MSClaudia\game.cpp $

#include <windows.h>
#include <cmath>
#include <ctime>
#include <vector>
#include <mmsystem.h>
#include "resource.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

#include "loader.h"

const int TILE_SIZE = 48;
const int MAP_WIDTH = 28;
const int MAP_HEIGHT = 31;
const int SCREEN_WIDTH = MAP_WIDTH * TILE_SIZE;
const int SCREEN_HEIGHT = MAP_HEIGHT * TILE_SIZE + 100;
const int VIEWPORT_HEIGHT = 900; // Visible viewport height
const int NUM_MAZES = 6;

// Fruit types for each maze
enum FruitType {
    FRUIT_CHERRY = 0,
    FRUIT_STRAWBERRY = 1,
    FRUIT_PEAR = 2,
    FRUIT_APPLE = 3,
    FRUIT_ORANGE = 4,
    FRUIT_BANANA = 5
};


struct Vec2 {
    float x, y;
};


struct Fruit {
    Vec2 pos;
    int type;
    bool active;
    int direction; // 0=left to right, 1=right to left
    float speed;
    int lifetime;
    int startY; // Y position where fruit enters
};


// Game map
int gameMap[MAP_HEIGHT][MAP_WIDTH];

// All 6 mazes data - loaded from mazes.dat
int allMazes[NUM_MAZES][MAP_HEIGHT][MAP_WIDTH];

struct Ghost {
    Vec2 pos;
    Vec2 target;
    int dir;
    int mode;       // 0=scatter, 1=chase, 2=frightened, 3=returning, 4=penned
    int color;
    Vec2 scatterTarget;
    int frightenedTimer;
    int animFrame;
    float subPixel;
    int penTimer;   // Frames until released from ghost house
};

// Improved Sound system - NO Beep, only wave generation with proper completion
class SoundSystem {
    private:
    bool soundEnabled;
    CRITICAL_SECTION cs;
    
    void generateWave(BYTE* buffer, int samples, int frequency, int volume) {
        const int sampleRate = 22050;
        for (int i = 0; i < samples; i++) {
            double t = (double)i / sampleRate;
            double value = sin(2.0 * 3.14159 * frequency * t) * volume;
            buffer[i] = (BYTE)(128 + value);
        }
    }
    
    void playToneAsync(int frequency, int durationMs, int volume = 100) {
        if (!soundEnabled) return;
        
        const int sampleRate = 22050;
        int samples = (sampleRate * durationMs) / 1000;
        
        // Ensure sample count is even to avoid click/pop at end
        if (samples % 2 != 0) samples++;
        
        struct WavHeader {
            char riff[4] = {'R','I','F','F'};
            int fileSize;
            char wave[4] = {'W','A','V','E'};
            char fmt[4] = {'f','m','t',' '};
            int fmtSize = 16;
            short audioFormat = 1;
            short numChannels = 1;
            int sampleRateVal = sampleRate;
            int byteRate = sampleRate;
            short blockAlign = 1;
            short bitsPerSample = 8;
            char data[4] = {'d','a','t','a'};
            int dataSize;
        } header;
        
        header.dataSize = samples;
        header.fileSize = 36 + samples;
        
        BYTE* buffer = new BYTE[sizeof(header) + samples];
        memcpy(buffer, &header, sizeof(header));
        generateWave(buffer + sizeof(header), samples, frequency, volume);
        
        // Apply fade out to last 10% of samples to avoid clicks
        int fadeStart = samples - (samples / 10);
        for (int i = fadeStart; i < samples; i++) {
            float fadeAmount = 1.0f - ((float)(i - fadeStart) / (samples - fadeStart));
            int idx = sizeof(header) + i;
            int sample = buffer[idx];
            sample = 128 + (int)((sample - 128) * fadeAmount);
            buffer[idx] = (BYTE)sample;
        }
        
        // Use SND_ASYNC | SND_NOSTOP to prevent cutting off previous sounds
        PlaySound((LPCSTR)buffer, NULL, SND_MEMORY | SND_ASYNC | SND_NOSTOP);
        
        // Clean up after a delay (in a real application, you'd want a better cleanup strategy)
        delete[] buffer;
    }
    
    public:
    SoundSystem() : soundEnabled(true) {
        InitializeCriticalSection(&cs);
    }
    
    ~SoundSystem() {
        DeleteCriticalSection(&cs);
    }
    
    void toggleSound() {
        soundEnabled = !soundEnabled;
        printf("Sound %s\n", soundEnabled ? "ENABLED" : "MUTED");
        fflush(stdout);
    }
    
    bool isSoundEnabled() {
        return soundEnabled;
    }
    
    void playWakaWaka(int frame) {
        EnterCriticalSection(&cs);
        int freq = (frame % 16 < 8) ? 440 : 494;
        playToneAsync(freq, 50, 80);
        LeaveCriticalSection(&cs);
    }
    
    void playEatGhost() {
        EnterCriticalSection(&cs);
        playToneAsync(1047, 150, 100);
        LeaveCriticalSection(&cs);
    }
    
    void playPowerPellet() {
        EnterCriticalSection(&cs);
        playToneAsync(262, 120, 100);
        LeaveCriticalSection(&cs);
    }
    
    void playDeath() {
        EnterCriticalSection(&cs);
        playToneAsync(400, 500, 100);
        LeaveCriticalSection(&cs);
    }
    
    void playLevelComplete() {
        EnterCriticalSection(&cs);
        playToneAsync(1047, 300, 100);
        LeaveCriticalSection(&cs);
    }
    
    void playSiren(int intensity) {
        EnterCriticalSection(&cs);
        int baseFreq = 200 + intensity * 50;
        playToneAsync(baseFreq, 40, 60);
        LeaveCriticalSection(&cs);
    }
    
    void playFrightened() {
        EnterCriticalSection(&cs);
        playToneAsync(200, 50, 70);
        LeaveCriticalSection(&cs);
    }
};

class Game {
    public:
    Vec2 claudia;
    int pacDir;
    int nextDir;
    Ghost ghosts[4];
    int score;
    int lives;
    int dots;
    int powerTimer;
    int level;
    bool gameOver;
    int frameCount;
    int modeTimer;
    int currentMode;
    int pacAnimFrame;
    int pacMouthOpen;
    float pacSubPixel;
    SoundSystem sound;
    bool isMoving;
    int sirenCounter;
    
    // FPS tracking
    int fpsFrameCount;
    DWORD fpsLastTime;
    float currentFPS;
    
    // Ghost eaten effects
    int ghostEatenFlash;
    int slowMotionTimer;
    Vec2 ghostEatenPos;
    
    // Camera position
    float cameraY;
    float targetCameraY;
    float cameraSmoothVelocity; // For smooth damping
    
    // Fruit system
    Fruit fruit;
    int fruitSpawnTimer;
    int currentMazeIndex;
    int fruitsSpawnedThisLevel;
    int fruitBonusScore;
    
    // Game Over
    bool waitingForRestart;
    
    // Cheat codes
    void completeLevel() {
        printf("\n*** CHEAT: Level Skip Activated! ***\n");
        printf("Clearing all dots from maze...\n");
        fflush(stdout);
        
        // Clear all dots
        int dotsCleared = 0;
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (gameMap[y][x] == 2 || gameMap[y][x] == 3) {
                    gameMap[y][x] = 0;
                    dotsCleared++;
                }
            }
        }
        
        printf("Cleared %d dots/pellets\n", dotsCleared);
        
        dots = 0;
        
        printf("Dots set to 0, triggering level complete...\n");
        fflush(stdout);
        
        // Manually trigger level completion
        sound.playLevelComplete();
        Sleep(500);
        level++;
        
        printf("Level incremented to %d\n", level);
        printf("Loading next maze...\n");
        fflush(stdout);
        
        loadMaze(level - 1);
        resetLevel();
        
        printf("Level skip complete!\n\n");
        fflush(stdout);
    }
    
    Game() {
        currentMazeIndex = 0;
        if (!LoadMazes("mazes.dat", (int*)allMazes, NUM_MAZES * MAP_HEIGHT * MAP_WIDTH)) {
            printf("ERROR: Could not load mazes.dat!\n");
            fflush(stdout);
        }
        reset();
        loadMaze(0);
        resetLevel();
        fpsFrameCount = 0;
        fpsLastTime = GetTickCount();
        currentFPS = 0.0f;
        cameraY = 0.0f;
        targetCameraY = 0.0f;
        cameraSmoothVelocity = 0.0f;
    }
    
    void loadMaze(int index) {
        currentMazeIndex = index % NUM_MAZES;
        
        printf("Loading maze index: %d\n", currentMazeIndex);
        fflush(stdout);
        
        // Copy maze data
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                gameMap[y][x] = allMazes[currentMazeIndex][y][x];
            }
        }
        
        printf("Maze loaded successfully!\n");
        fflush(stdout);
    }
    
    void reset() {
        claudia = {13.0f, 23.0f};
        pacDir = 0;
        nextDir = 0;
        score = 0;
        lives = 3;
        powerTimer = 0;
        level = 1;
        gameOver = false;
        frameCount = 0;
        modeTimer = 0;
        currentMode = 1;
        pacAnimFrame = 0;
        pacMouthOpen = 0;
        pacSubPixel = 0;
        isMoving = false;
        sirenCounter = 0;
        ghostEatenFlash = 0;
        slowMotionTimer = 0;
        ghostEatenPos = {0, 0};
        cameraY = 0.0f;
        targetCameraY = 0.0f;
        cameraSmoothVelocity = 0.0f;
        
        ghosts[0] = {{13.0f, 11.0f}, {25.0f, 0.0f}, 2, 1, 0, {25.0f, 0.0f}, 0, 0, 0.0f, 0};
        ghosts[1] = {{11.0f, 14.0f}, {2.0f, 0.0f},  3, 4, 1, {2.0f, 0.0f},  0, 0, 0.0f, 0};
        ghosts[2] = {{13.0f, 14.0f}, {27.0f, 31.0f}, 3, 4, 2, {27.0f, 31.0f},0, 0, 0.0f, 0};
        ghosts[3] = {{15.0f, 14.0f}, {0.0f, 31.0f},  3, 4, 3, {0.0f, 31.0f}, 0, 0, 0.0f, 0};
        penAllGhosts();
        
        countDots();
    }
    
    // Put a single ghost back into the pen with a short release delay
    void penGhost(int i, int delay) {
        Vec2 homePos[] = {{13.0f, 11.0f}, {11.0f, 14.0f}, {13.0f, 14.0f}, {15.0f, 14.0f}};
        ghosts[i].pos = homePos[i];
        ghosts[i].subPixel = 0;
        if (i == 0) {
            // Blinky always starts outside the house
            ghosts[i].mode = 1;
            ghosts[i].dir = 2;
            ghosts[i].penTimer = 0;
        } else {
            ghosts[i].mode = 4;
            ghosts[i].dir = 3;
            ghosts[i].penTimer = delay;
        }
        ghosts[i].frightenedTimer = 0;
    }

    void penAllGhosts() {
        int penTimers[] = {0, 15, 90, 200};
        for (int i = 0; i < 4; i++) {
            penGhost(i, penTimers[i]);
        }
    }

    void countDots() {
        dots = 0;
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (gameMap[y][x] == 2) dots++;
            }
        }
        
        printf("Dots counted: %d\n", dots);
        fflush(stdout);
    }
    
    bool canMove(int x, int y) {
        if (y < 0 || y >= MAP_HEIGHT) return false;    // Block vertical escape
        if (x < 0 || x >= MAP_WIDTH) return true;      // Allow horizontal tunnel wrap
        int tile = gameMap[y][x];
        return tile != 1;
    }

    bool canMovePlayer(int x, int y) {
        if (y < 0 || y >= MAP_HEIGHT) return false;
        if (x < 0 || x >= MAP_WIDTH) return true;
        int tile = gameMap[y][x];
        return tile != 1 && tile != 4;
    }
    
    void update() {
        if (gameOver) {
            waitingForRestart = true;
            return;
        }
        
        frameCount++;
        
        // Update FPS counter
        fpsFrameCount++;
        DWORD currentTime = GetTickCount();
        if (currentTime - fpsLastTime >= 1000) { // Update every second
            currentFPS = (float)fpsFrameCount / ((currentTime - fpsLastTime) / 1000.0f);
            fpsFrameCount = 0;
            fpsLastTime = currentTime;
        }
        
        // Update visual effects
        if (ghostEatenFlash > 0) ghostEatenFlash--;
        if (slowMotionTimer > 0) slowMotionTimer--;
        
        // Update camera to follow player with critically damped spring smoothing
        float playerScreenY = claudia.y * TILE_SIZE + TILE_SIZE / 2.0f;
        targetCameraY = playerScreenY - VIEWPORT_HEIGHT / 2.0f;
        
        // Clamp target camera to valid range
        if (targetCameraY < 0) targetCameraY = 0;
        if (targetCameraY > SCREEN_HEIGHT - VIEWPORT_HEIGHT) {
            targetCameraY = SCREEN_HEIGHT - VIEWPORT_HEIGHT;
        }
        
        // Critically damped spring for ultra-smooth camera (no oscillation)
        float smoothTime = 0.15f; // Lower = snappier, higher = smoother
        float maxSpeed = 1000.0f;
        float deltaTime = 1.0f / 125.0f; // Target frame time
        
        // Smooth damp algorithm
        float omega = 2.0f / smoothTime;
        float x = omega * deltaTime;
        float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        
        float change = cameraY - targetCameraY;
        float temp = (cameraSmoothVelocity + omega * change) * deltaTime;
        cameraSmoothVelocity = (cameraSmoothVelocity - omega * temp) * exp;
        cameraY = targetCameraY + (change + temp) * exp;
        
        pacAnimFrame = (frameCount / 4) % 2;
        
        int currentTileX = (int)claudia.x;
        int currentTileY = (int)claudia.y;
        
        int dx[] = {1, 0, -1, 0};
        int dy[] = {0, 1, 0, -1};
        
        bool centered = (pacSubPixel == 0);
        isMoving = false;

        // Immediate reversal: always allowed mid-tile (you came from there)
        if (nextDir == (pacDir + 2) % 4) {
            pacDir = nextDir;
            pacSubPixel = 1.0f - pacSubPixel; // Flip progress to go back
            // Recalculate current tile after reversal
            currentTileX = (int)claudia.x;
            currentTileY = (int)claudia.y;
        }
        // Perpendicular or same-direction turn: try at tile center
        else if (centered) {
            int nextTileX = currentTileX + dx[nextDir];
            int nextTileY = currentTileY + dy[nextDir];
            if (nextTileX < 0) nextTileX = MAP_WIDTH - 1;
            if (nextTileX >= MAP_WIDTH) nextTileX = 0;
            if (canMovePlayer(nextTileX, nextTileY)) {
                pacDir = nextDir;
            }
        }

        if (centered) {
            if (gameMap[currentTileY][currentTileX] == 2) {
                gameMap[currentTileY][currentTileX] = 0;
                score += 10;
                dots--;
                if (dots == 0) {
                    sound.playLevelComplete();
                    level++;
                    Sleep(300);
                    loadMaze(level - 1); // Load next maze
                    resetLevel();
                    return;
                }
            } else if (gameMap[currentTileY][currentTileX] == 3) {
                gameMap[currentTileY][currentTileX] = 0;
                score += 50;
                sound.playPowerPellet();
                powerTimer = 300;
                for (int i = 0; i < 4; i++) {
                    if (ghosts[i].mode != 3 && ghosts[i].mode != 4) {
                        ghosts[i].mode = 2;
                        ghosts[i].frightenedTimer = 300;
                    }
                }
            }
        }
        
        int targetTileX = currentTileX + dx[pacDir];
        int targetTileY = currentTileY + dy[pacDir];
        
        if (targetTileX < 0) targetTileX = MAP_WIDTH - 1;
        if (targetTileX >= MAP_WIDTH) targetTileX = 0;
        
        if (canMovePlayer(targetTileX, targetTileY)) {
            // Two half-steps per frame for smoother movement (same overall speed)
            float speedMultiplier = (slowMotionTimer > 0) ? 0.3f : 1.0f;
            float step = 0.0625f * speedMultiplier;
            for (int s = 0; s < 2; s++) {
                pacSubPixel += step;
                if (pacSubPixel >= 1.0f) {
                    pacSubPixel = 0;
                    claudia.x = (float)targetTileX;
                    claudia.y = (float)targetTileY;
                    break;
                }
            }
            isMoving = true;
            pacMouthOpen = (frameCount / 4) % 2;

            if (frameCount % 8 == 0) {
                sound.playWakaWaka(frameCount);
            }
        } else {
            pacMouthOpen = 0;
        }
        
        if (powerTimer == 0) {
            sirenCounter++;
            if (sirenCounter % 30 == 0) {
                int chasingGhosts = 0;
                for (int i = 0; i < 4; i++) {
                    if (ghosts[i].mode == 0 || ghosts[i].mode == 1) chasingGhosts++;
                }
                sound.playSiren(chasingGhosts);
            }
        } else {
            if (frameCount % 20 == 0) {
                sound.playFrightened();
            }
        }
        
        modeTimer++;
        if (currentMode == 0 && modeTimer > 210) {
            currentMode = 1;
            modeTimer = 0;
            // Force grid alignment for all ghosts when mode switches
            for (int i = 0; i < 4; i++) {
                if (ghosts[i].mode == 0 || ghosts[i].mode == 1) {
                    ghosts[i].subPixel = 0;
                    ghosts[i].pos.x = (float)(int)(ghosts[i].pos.x + 0.5f);
                    ghosts[i].pos.y = (float)(int)(ghosts[i].pos.y + 0.5f);
                    ghosts[i].mode = currentMode;
                }
            }
        } else if (currentMode == 1 && modeTimer > 600) {
            currentMode = 0;
            modeTimer = 0;
            // Force grid alignment for all ghosts when mode switches
            for (int i = 0; i < 4; i++) {
                if (ghosts[i].mode == 0 || ghosts[i].mode == 1) {
                    ghosts[i].subPixel = 0;
                    ghosts[i].pos.x = (float)(int)(ghosts[i].pos.x + 0.5f);
                    ghosts[i].pos.y = (float)(int)(ghosts[i].pos.y + 0.5f);
                    ghosts[i].mode = currentMode;
                }
            }
        }
        
        if (powerTimer > 0) {
            powerTimer--;
            if (powerTimer == 0) {
                for (int i = 0; i < 4; i++) {
                    if (ghosts[i].mode == 2) {
                        ghosts[i].mode = currentMode;
                        // Force grid alignment when mode changes
                        ghosts[i].subPixel = 0;
                        ghosts[i].pos.x = (float)(int)(ghosts[i].pos.x + 0.5f);
                        ghosts[i].pos.y = (float)(int)(ghosts[i].pos.y + 0.5f);
                    }
                }
            }
        }
        
        for (int i = 0; i < 4; i++) {
            updateGhost(ghosts[i]);
        }
        
        // Update fruit
        fruitSpawnTimer--;
        
        // Spawn fruit if timer reached and less than 2 fruits spawned this level
        if (fruitSpawnTimer <= 0 && !fruit.active && fruitsSpawnedThisLevel < 2) {
            //spawnFruit(); // TODO(isaveg)
        }
        
        if (fruit.active) {
            // updateFruit(); // TODO(dagon)
        }
        
        // Check collisions
        for (int i = 0; i < 4; i++) {
            float renderX = claudia.x + dx[pacDir] * pacSubPixel;
            float renderY = claudia.y + dy[pacDir] * pacSubPixel;
            float dist = sqrt(pow(renderX - ghosts[i].pos.x, 2) + pow(renderY - ghosts[i].pos.y, 2));
            if (dist < 0.5f) {
                if (ghosts[i].mode == 2) {
                    score += 200;
                    sound.playEatGhost();
                    ghosts[i].mode = 3;
                    ghosts[i].target = {13.0f, 11.0f};

                    // Trigger visual effects
                    ghostEatenFlash = 15;
                    slowMotionTimer = 60;
                    ghostEatenPos = ghosts[i].pos;
                } else if (ghosts[i].mode != 3 && ghosts[i].mode != 4) {
                    lives--;
                    sound.playDeath();
                    if (lives <= 0) {
                        gameOver = true;
                    } else {
                        Sleep(500);
                        claudia = {13.0f, 23.0f};
                        pacSubPixel = 0;
                        penAllGhosts();
                    }
                }
            }
        }
    }
    
    // ENHANCED GHOST AI - Each ghost has unique targeting behavior
    void updateGhost(Ghost& g) {
        g.animFrame = (frameCount / 8) % 2;
        
        if (g.frightenedTimer > 0) g.frightenedTimer--;
        
        // Mode 4 = Penned in ghost house, waiting to be released
        if (g.mode == 4) {
            g.penTimer--;
            if (g.penTimer <= 0) {
                // Switch to chase/scatter — AI will navigate out through the door
                g.mode = currentMode;
                g.dir = 3; // Face up toward the exit
            }
            return;
        }

        // Apply slow motion when ghost is eaten (even slower)
        if (slowMotionTimer > 0 && (frameCount % 4 != 0)) {
            return; // Skip 3 out of 4 frames for stronger slow motion effect
        }

        // Mode 3 = Returning to ghost house after being eaten
        // Target tile just above the door — ghost gets placed inside on arrival
        if (g.mode == 3) {
            g.target = {13.0f, 11.0f};
        } 
        // Mode 2 = Frightened (preserve original frightened logic)
        else if (g.mode == 2) {
            // Random movement when frightened - no changes to this mode
        } 
        // Mode 0 = Scatter mode
        else if (currentMode == 0) {
            g.target = g.scatterTarget;
        } 
        // Mode 1 = Chase mode - ENHANCED AI FOR EACH GHOST
        else {
            int dx[] = {1, 0, -1, 0};
            int dy[] = {0, 1, 0, -1};
            
            switch (g.color) {
                case 0: // BLINKY (Red) - The Shadow/Aggressor
                // Directly targets Ms. Pac-Man's current tile
                g.target = claudia;
                break;
                
                case 1: // PINKY (Pink) - The Ambusher
                // Targets 2-4 tiles ahead of Ms. Pac-Man to cut her off
                {
                    int tilesAhead = 3; // Target 3 tiles ahead
                    g.target.x = claudia.x + dx[pacDir] * tilesAhead;
                    g.target.y = claudia.y + dy[pacDir] * tilesAhead;
                    
                    // Add slight randomization for Ms. Pac-Man style unpredictability
                    if (frameCount % 60 < 30) {
                        g.target.x += (rand() % 3) - 1; // -1, 0, or 1
                        g.target.y += (rand() % 3) - 1;
                    }
                }
                break;
                
                case 2: // INKY (Cyan) - The Fickle One
                // Complex targeting based on Blinky's position and Ms. Pac-Man's location
                {
                    // Get Blinky's position (ghost[0])
                    Vec2 blinkyPos = ghosts[0].pos;
                    
                    // Calculate point 2 tiles ahead of Ms. Pac-Man
                    float intermediateX = claudia.x + dx[pacDir] * 2;
                    float intermediateY = claudia.y + dy[pacDir] * 2;
                    
                    // Create vector from Blinky to intermediate point
                    float vectorX = intermediateX - blinkyPos.x;
                    float vectorY = intermediateY - blinkyPos.y;
                    
                    // Double the vector to get Inky's target (creates unpredictable patterns)
                    g.target.x = blinkyPos.x + vectorX * 2.0f;
                    g.target.y = blinkyPos.y + vectorY * 2.0f;
                    
                    // Add Ms. Pac-Man style randomization
                    if (frameCount % 45 < 15) {
                        g.target.x += (rand() % 5) - 2; // -2 to 2
                        g.target.y += (rand() % 5) - 2;
                    }
                }
                break;
                
                case 3: // SUE/CLYDE (Orange/Purple) - The Erratic One
                // Approaches when far, retreats to scatter corner when close
                {
                    float distToClaudia = sqrt(pow(g.pos.x - claudia.x, 2) + pow(g.pos.y - claudia.y, 2));
                    
                    // If more than 8 tiles away, chase Ms. Pac-Man
                    if (distToClaudia > 8.0f) {
                        g.target = claudia;
                        
                        // Add randomization when chasing
                        if (frameCount % 30 < 10) {
                            g.target.x += (rand() % 3) - 1;
                            g.target.y += (rand() % 3) - 1;
                        }
                    } 
                    // If 8 tiles or closer, retreat to scatter corner
                    else {
                        g.target = g.scatterTarget;
                        
                        // Add some erratic behavior when retreating
                        if (frameCount % 40 < 10) {
                            // Occasionally make random movements
                            g.target.x += (rand() % 4) - 2;
                            g.target.y += (rand() % 4) - 2;
                        }
                    }
                }
                break;
            }
        }
        
        int currentTileX = (int)g.pos.x;
        int currentTileY = (int)g.pos.y;

        // If ghost is inside the ghost house, override target to exit point
        // so it navigates up through the door instead of toward the player
        if (g.mode != 3 && currentTileX >= 0 && currentTileX < MAP_WIDTH
            && currentTileY >= 0 && currentTileY < MAP_HEIGHT
            && gameMap[currentTileY][currentTileX] == 4) {
            g.target = {13.0f, 11.0f};
        }

        int dx[] = {1, 0, -1, 0};
        int dy[] = {0, 1, 0, -1};
        
        bool centered = (g.subPixel == 0);
        
        if (centered) {
            int bestDir = g.dir;
            float bestDist = 999999;
            
            for (int d = 0; d < 4; d++) {
                // Can't reverse direction unless frightened
                if (d == (g.dir + 2) % 4 && g.mode != 2) continue;
                
                int nx = currentTileX + dx[d];
                int ny = currentTileY + dy[d];
                
                if (nx < 0) nx = MAP_WIDTH - 1;
                if (nx >= MAP_WIDTH) nx = 0;
                
                if (!canMove(nx, ny)) continue;
                
                float dist;
                if (g.mode == 2) {
                    // Frightened mode - random movement (preserve original)
                    dist = rand() % 1000;
                } else {
                    // Calculate Manhattan distance to target
                    dist = abs(nx - g.target.x) + abs(ny - g.target.y);
                    
                    // Add slight randomization for Ms. Pac-Man unpredictability
                    // This makes ghosts less perfectly predictable
                    int randomFactor = rand() % 100;
                    if (randomFactor < 15) { // 15% chance to add randomness
                        dist += (rand() % 3) - 1; // Add -1, 0, or 1 to distance
                    }
                }
                
                if (dist < bestDist) {
                    bestDist = dist;
                    bestDir = d;
                }
            }
            g.dir = bestDir;
        }
        
        float speed = (g.mode == 3) ? 0.25f : (g.mode == 2) ? 0.0625f : 0.125f;
        g.subPixel += speed;
        
        if (g.subPixel >= 1.0f) {
            g.subPixel = 0;
            int newX = currentTileX + dx[g.dir];
            int newY = currentTileY + dy[g.dir];
            
            if (newX < 0) newX = MAP_WIDTH - 1;
            if (newX >= MAP_WIDTH) newX = 0;
            
            g.pos.x = newX;
            g.pos.y = newY;
        }
        
        // Check if ghost has reached tile above the door
        if (g.mode == 3 && g.subPixel == 0) {
            if (currentTileX == 13 && currentTileY == 11) {
                penGhost(g.color, 30);
            }
        }
    }
    
    void resetLevel() {
        claudia = {13.0f, 23.0f};
        pacSubPixel = 0;
        pacDir = 0;
        nextDir = 0;
        penAllGhosts();
        
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (gameMap[y][x] == 0 && y != 14) {
                    gameMap[y][x] = 2;
                }
            }
        }
        gameMap[3][1] = gameMap[3][26] = 3;
        gameMap[23][1] = gameMap[23][26] = 3;
        countDots();
    }
};

Game game;
HWND hwnd;

// Triple buffering - now with proper synchronization
HDC backBufferDC = NULL;
HBITMAP backBufferBitmap = NULL;
HBITMAP oldBackBufferBitmap = NULL;
CRITICAL_SECTION bufferCS;
volatile bool bufferReady = false;

void DrawCircle(HDC hdc, int x, int y, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawClaudia(HDC hdc, int x, int y, int dir, int mouthOpen) {
    COLORREF yellow = RGB(255, 255, 0);
    COLORREF red = RGB(255, 0, 0);
    
    HBRUSH brush = CreateSolidBrush(yellow);
    HPEN pen = CreatePen(PS_SOLID, 2, yellow);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    int radius = 20; // Doubled from 10

    // Compute mouth corner points once - used for pie shape AND lipstick in both states
    POINT mouth1 = {0, 0}, mouth2 = {0, 0};
    switch(dir) {
        case 0: mouth1 = {x + 14, y - 10}; mouth2 = {x + 14, y + 10}; break;
        case 1: mouth1 = {x + 10, y + 14}; mouth2 = {x - 10, y + 14}; break;
        case 2: mouth1 = {x - 14, y + 10}; mouth2 = {x - 14, y - 10}; break;
        case 3: mouth1 = {x - 10, y - 14}; mouth2 = {x + 10, y - 14}; break;
    }

    // Extend mouth corners to circle circumference for lipstick endpoints
    float dx1 = (float)(mouth1.x - x), dy1 = (float)(mouth1.y - y);
    float len1 = sqrtf(dx1 * dx1 + dy1 * dy1);
    int edgeX1 = x + (int)(dx1 * radius / len1);
    int edgeY1 = y + (int)(dy1 * radius / len1);

    float dx2 = (float)(mouth2.x - x), dy2 = (float)(mouth2.y - y);
    float len2 = sqrtf(dx2 * dx2 + dy2 * dy2);
    int edgeX2 = x + (int)(dx2 * radius / len2);
    int edgeY2 = y + (int)(dy2 * radius / len2);

    if (!mouthOpen) {
        Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
    } else {
        Pie(hdc, x - radius, y - radius, x + radius, y + radius,
            mouth1.x, mouth1.y, mouth2.x, mouth2.y);
    }

    // Red lipstick - animates with mouth open/close
    {
        HPEN lipPen = CreatePen(PS_SOLID, 4, RGB(200, 0, 0));
        HPEN prevLipPen = (HPEN)SelectObject(hdc, lipPen);

        // Facing point on circumference (where lips converge when closed)
        int faceDx[] = {1, 0, -1, 0};
        int faceDy[] = {0, 1, 0, -1};
        int faceX = x + faceDx[dir] * radius;
        int faceY = y + faceDy[dir] * radius;

        if (mouthOpen) {
            // Mouth open: lips spread along pie edges from 40% to circumference
            float t = 0.4f;
            MoveToEx(hdc, x + (int)(dx1 * t), y + (int)(dy1 * t), NULL);
            LineTo(hdc, edgeX1, edgeY1);
            MoveToEx(hdc, x + (int)(dx2 * t), y + (int)(dy2 * t), NULL);
            LineTo(hdc, edgeX2, edgeY2);
        } else {
            // Mouth closed: lips converge toward facing point
            int lipX1 = faceX + (int)((edgeX1 - faceX) * 0.5f);
            int lipY1 = faceY + (int)((edgeY1 - faceY) * 0.5f);
            int lipX2 = faceX + (int)((edgeX2 - faceX) * 0.5f);
            int lipY2 = faceY + (int)((edgeY2 - faceY) * 0.5f);
            MoveToEx(hdc, lipX1, lipY1, NULL);
            LineTo(hdc, faceX, faceY);
            MoveToEx(hdc, lipX2, lipY2, NULL);
            LineTo(hdc, faceX, faceY);
        }

        SelectObject(hdc, prevLipPen);
        DeleteObject(lipPen);
    }
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    
    // Draw Ms. Pac-Man's bow on top (red ribbon bow)
    HBRUSH bowBrush = CreateSolidBrush(red);
    HPEN bowPen = CreatePen(PS_SOLID, 2, red);
    
    SelectObject(hdc, bowBrush);
    SelectObject(hdc, bowPen);
    
    // Bow position - on top/back of head, rotates with direction
    int bowX = 0, bowY = 0;
    switch(dir) {
        case 0: bowX = x - 5;  bowY = y - 16; break; // Right: bow top-left
        case 1: bowX = x - 5;  bowY = y - 16; break; // Down: bow on top (back of head)
        case 2: bowX = x + 5;  bowY = y - 16; break; // Left: bow top-right (mirrored)
        case 3: bowX = x - 5;  bowY = y + 16; break; // Up: bow on bottom (back of head)
    }

    // Bow loops are horizontal for left/right/down, vertical for up
    POINT loopA[4], loopB[4];
    if (dir == 3) {
        // Up: bow at bottom, loops spread horizontally, offset downward
        loopA[0] = {bowX - 8, bowY};     loopA[1] = {bowX - 3, bowY - 4};
        loopA[2] = {bowX - 3, bowY + 4}; loopA[3] = {bowX - 8, bowY};
        loopB[0] = {bowX + 8, bowY};     loopB[1] = {bowX + 3, bowY - 4};
        loopB[2] = {bowX + 3, bowY + 4}; loopB[3] = {bowX + 8, bowY};
    } else {
        // Right/Down/Left: bow on top, loops spread horizontally
        loopA[0] = {bowX - 8, bowY};     loopA[1] = {bowX - 3, bowY - 4};
        loopA[2] = {bowX - 3, bowY + 4}; loopA[3] = {bowX - 8, bowY};
        loopB[0] = {bowX + 8, bowY};     loopB[1] = {bowX + 3, bowY - 4};
        loopB[2] = {bowX + 3, bowY + 4}; loopB[3] = {bowX + 8, bowY};
    }
    Polygon(hdc, loopA, 4);
    Polygon(hdc, loopB, 4);

    // Center knot
    Ellipse(hdc, bowX - 2, bowY - 2, bowX + 2, bowY + 2);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(bowBrush);
    DeleteObject(bowPen);
}

void DrawGhost(HDC hdc, int x, int y, int color, int mode, int frame) {
    COLORREF colors[] = {RGB(255, 0, 0), RGB(255, 184, 255), RGB(0, 255, 255), RGB(255, 184, 82)};
    COLORREF ghostColor = (mode == 2) ? RGB(33, 33, 255) : (mode == 3) ? RGB(255, 255, 255) : colors[color];
    
    int radius = 20; // Doubled from 10
    
    HBRUSH brush = CreateSolidBrush(ghostColor);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 2, ghostColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // Top half circle
    Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
    
    // Bottom rectangle with waves
    Rectangle(hdc, x - radius, y, x + radius, y + 16);
    
    // Wave pattern at bottom (doubled size)
    POINT wave[7];
    wave[0] = {x - 20, y + 16};
    wave[1] = {x - 14, y + 8};
    wave[2] = {x - 6, y + 16};
    wave[3] = {x, y + 8};
    wave[4] = {x + 6, y + 16};
    wave[5] = {x + 14, y + 8};
    wave[6] = {x + 20, y + 16};
    Polyline(hdc, wave, 7);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    
    // Eyes (except when eaten)
    if (mode != 3) {
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        HBRUSH blueBrush = CreateSolidBrush(RGB(0, 0, 255));
        
        SelectObject(hdc, whiteBrush);
        // Doubled eye positions and sizes
        Ellipse(hdc, x - 14, y - 14, x - 4, y - 4);
        Ellipse(hdc, x + 4, y - 14, x + 14, y - 4);
        
        if (mode != 2) {
            SelectObject(hdc, blueBrush);
            Ellipse(hdc, x - 12, y - 12, x - 6, y - 6);
            Ellipse(hdc, x + 6, y - 12, x + 12, y - 6);
        }
        
        DeleteObject(whiteBrush);
        DeleteObject(blueBrush);
    }
}

void DrawFruit(HDC hdc, int x, int y, int type) {
    // Different colors for each fruit
    COLORREF fruitColors[] = {
        RGB(255, 0, 0),     // Cherry - red
        RGB(255, 0, 127),   // Strawberry - pink/red
        RGB(200, 255, 0),   // Pear - yellow-green
        RGB(255, 0, 0),     // Apple - red
        RGB(255, 165, 0),   // Orange - orange
        RGB(255, 255, 0)    // Banana - yellow
    };
    
    HBRUSH brush = CreateSolidBrush(fruitColors[type]);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 2, fruitColors[type]);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // Draw simple fruit shape (circle with stem)
    Ellipse(hdc, x - 12, y - 12, x + 12, y + 12);
    
    // Draw stem (green line on top)
    HPEN stemPen = CreatePen(PS_SOLID, 3, RGB(0, 180, 0));
    SelectObject(hdc, stemPen);
    MoveToEx(hdc, x, y - 12, NULL);
    LineTo(hdc, x, y - 18);
    
    // Add leaf
    POINT leaf[3];
    leaf[0] = {x, y - 18};
    leaf[1] = {x + 6, y - 15};
    leaf[2] = {x, y - 14};
    Polygon(hdc, leaf, 3);
    
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    DeleteObject(stemPen);
}

void Render(HDC hdc) {
    // Use pre-created black brush for better performance
    static HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    RECT fullRect = {0, 0, SCREEN_WIDTH, VIEWPORT_HEIGHT};
    FillRect(hdc, &fullRect, blackBrush);
    
    SetBkMode(hdc, TRANSPARENT);
    
    // Create brushes and pens once, reuse them
    static HBRUSH wallBrush = CreateSolidBrush(RGB(33, 33, 255));
    static HPEN wallPen = CreatePen(PS_SOLID, 1, RGB(33, 33, 255));
    static HBRUSH dotBrush = CreateSolidBrush(RGB(255, 184, 174));
    static HPEN dotPen = CreatePen(PS_SOLID, 1, RGB(255, 184, 174));
    static HBRUSH pelletBrush = CreateSolidBrush(RGB(255, 255, 255));
    static HPEN pelletPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    
    // Apply flash effect when ghost is eaten
    if (game.ghostEatenFlash > 0) {
        // Subtle flash effect at ghost position
        int flashX = (int)(game.ghostEatenPos.x * TILE_SIZE + TILE_SIZE / 2);
        int flashY = (int)roundf(game.ghostEatenPos.y * TILE_SIZE + TILE_SIZE / 2 - game.cameraY);
        int flashRadius = (15 - game.ghostEatenFlash) * 8;
        
        // Only draw if visible
        if (flashY > -100 && flashY < VIEWPORT_HEIGHT + 100) {
            int brightness = (game.ghostEatenFlash * 200) / 15;
            HPEN flashPen = CreatePen(PS_SOLID, 2, RGB(brightness, brightness, brightness / 3));
            HPEN oldPen = (HPEN)SelectObject(hdc, flashPen);
            
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Ellipse(hdc, flashX - flashRadius, flashY - flashRadius, flashX + flashRadius, flashY + flashRadius);
            
            SelectObject(hdc, oldPen);
            DeleteObject(flashPen);
        }
    }
    
    // Draw maze with camera offset
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int px = x * TILE_SIZE;
            // Use precise float-to-int conversion for pixel-perfect rendering
            int py = (int)roundf(y * TILE_SIZE - game.cameraY);
            
            // Cull off-screen tiles
            if (py < -TILE_SIZE || py > VIEWPORT_HEIGHT) continue;
            
            if (gameMap[y][x] == 1) {
                RECT r = {px, py, px + TILE_SIZE, py + TILE_SIZE};
                FillRect(hdc, &r, wallBrush);
            } else if (gameMap[y][x] == 2) {
                HBRUSH old = (HBRUSH)SelectObject(hdc, dotBrush);
                HPEN oldP = (HPEN)SelectObject(hdc, dotPen);
                Ellipse(hdc, px + 20, py + 20, px + 28, py + 28);
                SelectObject(hdc, old);
                SelectObject(hdc, oldP);
            } else if (gameMap[y][x] == 3) {
                HBRUSH old = (HBRUSH)SelectObject(hdc, pelletBrush);
                HPEN oldP = (HPEN)SelectObject(hdc, pelletPen);
                Ellipse(hdc, px + 14, py + 14, px + 34, py + 34);
                SelectObject(hdc, old);
                SelectObject(hdc, oldP);
            }
        }
    }
    
    // Draw ghosts with camera offset
    for (int i = 0; i < 4; i++) {
        int dx[] = {1, 0, -1, 0};
        int dy[] = {0, 1, 0, -1};
        float renderX = game.ghosts[i].pos.x + dx[game.ghosts[i].dir] * game.ghosts[i].subPixel;
        float renderY = game.ghosts[i].pos.y + dy[game.ghosts[i].dir] * game.ghosts[i].subPixel;
        
        int gx = (int)(renderX * TILE_SIZE + TILE_SIZE / 2);
        int gy = (int)roundf(renderY * TILE_SIZE + TILE_SIZE / 2 - game.cameraY);
        
        // Only draw if visible
        if (gy > -50 && gy < VIEWPORT_HEIGHT + 50) {
            DrawGhost(hdc, gx, gy, game.ghosts[i].color, game.ghosts[i].mode, game.ghosts[i].animFrame);
        }
    }
    
    // Draw claudia with camera offset
    int dx[] = {1, 0, -1, 0};
    int dy[] = {0, 1, 0, -1};
    float renderX = game.claudia.x + dx[game.pacDir] * game.pacSubPixel;
    float renderY = game.claudia.y + dy[game.pacDir] * game.pacSubPixel;
    
    int px = (int)(renderX * TILE_SIZE + TILE_SIZE / 2);
    int py = (int)roundf(renderY * TILE_SIZE + TILE_SIZE / 2 - game.cameraY);
    DrawClaudia(hdc, px, py, game.pacDir, game.pacMouthOpen);
    
    // Draw fruit if active
    if (game.fruit.active) {
        int fx = (int)(game.fruit.pos.x * TILE_SIZE + TILE_SIZE / 2);
        int fy = (int)roundf(game.fruit.pos.y * TILE_SIZE + TILE_SIZE / 2 - game.cameraY);
        
        // Only draw if visible
        if (fy > -50 && fy < VIEWPORT_HEIGHT + 50) {
            DrawFruit(hdc, fx, fy, game.fruit.type);
        }
    }
    
    // Draw UI (fixed at bottom of viewport)
    SetTextColor(hdc, RGB(255, 255, 255));
    char text[100];
    sprintf(text, "SCORE: %d     LIVES: %d     LEVEL: %d", game.score, game.lives, game.level);
    TextOut(hdc, 20, VIEWPORT_HEIGHT - 80, text, (int)strlen(text));
    
    // Display FPS
    SetTextColor(hdc, RGB(0, 255, 0));
    char fpsText[50];
    sprintf(fpsText, "FPS: %.1f", game.currentFPS);
    TextOut(hdc, SCREEN_WIDTH - 150, VIEWPORT_HEIGHT - 80, fpsText, (int)strlen(fpsText));
    
    // Show slow motion indicator (fixed at top)
    if (game.slowMotionTimer > 0) {
        SetTextColor(hdc, RGB(255, 255, 0));
        const char* slowMoText = "SLOW MOTION!";
        TextOut(hdc, SCREEN_WIDTH / 2 - 80, 30, slowMoText, (int)strlen(slowMoText));
    }
    
    // Show sound status
    if (!game.sound.isSoundEnabled()) {
        SetTextColor(hdc, RGB(255, 100, 100));
        const char* muteText = "SOUND MUTED (M)";
        TextOut(hdc, 20, 30, muteText, (int)strlen(muteText));
    }
    
    if (game.gameOver) {
        SetTextColor(hdc, RGB(255, 0, 0));
        const char* msg = "GAME OVER";
        TextOut(hdc, SCREEN_WIDTH/2 - 70, VIEWPORT_HEIGHT/2 - 40, msg, (int)strlen(msg));
        
        SetTextColor(hdc, RGB(255, 255, 255));
        const char* restartMsg = "Press SPACE to Restart";
        TextOut(hdc, SCREEN_WIDTH/2 - 120, VIEWPORT_HEIGHT/2 + 20, restartMsg, (int)strlen(restartMsg));
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            InitializeCriticalSection(&bufferCS);
            HDC hdc = GetDC(hwnd);
            
            backBufferDC = CreateCompatibleDC(hdc);
            backBufferBitmap = CreateCompatibleBitmap(hdc, SCREEN_WIDTH, VIEWPORT_HEIGHT);
            oldBackBufferBitmap = (HBITMAP)SelectObject(backBufferDC, backBufferBitmap);
            
            // Set high quality rendering
            SetBkMode(backBufferDC, TRANSPARENT);
            SetStretchBltMode(backBufferDC, HALFTONE);
            
            ReleaseDC(hwnd, hdc);
            return 0;
        }
        
        case WM_DESTROY:
        if (backBufferDC) SelectObject(backBufferDC, oldBackBufferBitmap);
        DeleteObject(backBufferBitmap);
        DeleteDC(backBufferDC);
        DeleteCriticalSection(&bufferCS);
        PostQuitMessage(0);
        return 0;
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            EnterCriticalSection(&bufferCS);
            if (bufferReady) {
                BitBlt(hdc, 0, 0, SCREEN_WIDTH, VIEWPORT_HEIGHT, backBufferDC, 0, 0, SRCCOPY);
            }
            LeaveCriticalSection(&bufferCS);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_ERASEBKGND:
        return 1;
        
        case WM_KEYDOWN:
        if (wParam == VK_UP) game.nextDir = 3;
        else if (wParam == VK_DOWN) game.nextDir = 1;
        else if (wParam == VK_LEFT) game.nextDir = 2;
        else if (wParam == VK_RIGHT) game.nextDir = 0;
        else if (wParam == VK_ESCAPE) PostQuitMessage(0);
        else if (wParam == VK_SPACE && game.waitingForRestart) {
            // Restart the game
            game.currentMazeIndex = 0;
            game.loadMaze(0);
            game.reset();
        }
        // Cheat codes
        else if (wParam == 'N') {
            // Skip level cheat (N for "Next")
            printf("\n[CHEAT] N key pressed - Skipping level!\n");
            fflush(stdout);
            game.completeLevel();
        }
        else if (wParam == 'M') {
            // Toggle sound mute (M for "Mute")
            game.sound.toggleSound();
        }
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    srand(time(0));
    
    // Create console for debugging
    AllocConsole();
    FILE* consoleOut;
    freopen_s(&consoleOut, "CONOUT$", "w", stdout);
    freopen_s(&consoleOut, "CONOUT$", "w", stderr);
    
    printf("MS-CLAUDIA Debug Console\n");
    printf("=======================\n");
    printf("CHEAT CODES:\n");
    printf("  N - Skip to next level\n");
    printf("  M - Toggle sound mute\n");
    printf("=======================\n\n");
    
    const char CLASS_NAME[] = "MSClaudia-Window";
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MSCLAUDIA_ICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_OWNDC;
    
    RegisterClass(&wc);
    
    hwnd = CreateWindowEx(
                          0, CLASS_NAME, "MS-CLAUDIA",
                          WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          SCREEN_WIDTH + 16, VIEWPORT_HEIGHT + 39,
                          NULL, NULL, hInstance, NULL
                          );
    
    if (hwnd == NULL) return 0;
    
    ShowWindow(hwnd, nCmdShow);
    
    MSG msg = {};
    DWORD lastTime = GetTickCount();
    
    // Initial render
    Render(backBufferDC);
    bufferReady = true;
    
    // Set higher process and thread priority for better performance
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    
    while (true) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        DWORD currentTime = GetTickCount();
        if (currentTime - lastTime >= 8) { // 8ms per frame = ~125 FPS
            game.update();
            
            // Render to back buffer
            EnterCriticalSection(&bufferCS);
            Render(backBufferDC);
            LeaveCriticalSection(&bufferCS);
            
            // Copy to screen
            HDC screenDC = GetDC(hwnd);
            BitBlt(screenDC, 0, 0, SCREEN_WIDTH, VIEWPORT_HEIGHT, backBufferDC, 0, 0, SRCCOPY);
            GdiFlush();
            ReleaseDC(hwnd, screenDC);
            
            lastTime = currentTime;
        } else {
            // Don't sleep - busy wait for better timing precision
            // Sleep(0) yields to other threads but maintains high FPS
            Sleep(0);
        }
    }
    
    return 0;
}
