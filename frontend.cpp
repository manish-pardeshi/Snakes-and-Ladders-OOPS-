// ===================== FRONTEND (raylib drawing + animation) =====================
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "backend.cpp"
#include "raylib.h"

constexpr int WIN_W = 1000;
constexpr int WIN_H = 740;
constexpr int CELL = 64;
constexpr int BOARD_X = 30;
constexpr int BOARD_Y = 30;

// ---------- small vector helpers ----------
inline Vector2 vAdd(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vector2 vSub(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vector2 vMul(Vector2 a, float s) { return {a.x * s, a.y * s}; }
inline float vLen(Vector2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
inline Vector2 vLerp(Vector2 a, Vector2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

inline void drawWrapped(const std::string& text, int x, int y, int maxW, int size, Color c) {
    std::istringstream words(text);
    std::string word, line;
    int cy = y;
    while (words >> word) {
        std::string test = line.empty() ? word : line + " " + word;
        if (MeasureText(test.c_str(), size) > maxW && !line.empty()) {
            DrawText(line.c_str(), x, cy, size, c);
            cy += size + 4;
            line = word;
        } else {
            line = test;
        }
    }
    if (!line.empty()) DrawText(line.c_str(), x, cy, size, c);
}

// =============================== DiceView ===============================
// Draws a real-looking die with pips (dots) and a shaking "rolling" animation.
class DiceView {
    Rectangle box;
    int face = 1;
    bool rolling = false;
    float timer = 0.f, flick = 0.f;
    static constexpr float DURATION = 0.9f;

public:
    DiceView(float x, float y, float size) : box{x, y, size, size} {}

    void setFace(int f) { face = f; }
    void startRoll() { rolling = true; timer = 0.f; flick = 0.f; }
    bool isRolling() const { return rolling; }

    // returns true on the frame the roll animation finishes
    bool update(float dt, int finalFace) {
        if (!rolling) return false;
        timer += dt;
        flick += dt;
        if (flick > 0.07f) { face = GetRandomValue(1, 6); flick = 0.f; }
        if (timer >= DURATION) { rolling = false; face = finalFace; return true; }
        return false;
    }

    void draw() const {
        static const int PIPS[7][9] = {
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 1, 0, 0, 0, 1}, {1, 0, 1, 0, 0, 0, 1, 0, 1}, {1, 0, 1, 0, 1, 0, 1, 0, 1},
            {1, 0, 1, 1, 0, 1, 1, 0, 1}};

        Rectangle r = box;
        if (rolling) { r.x += GetRandomValue(-5, 5); r.y += GetRandomValue(-5, 5); }

        DrawRectangleRounded({r.x + 6, r.y + 8, r.width, r.height}, 0.2f, 10, Fade(BLACK, 0.3f)); // shadow
        DrawRectangleRounded({r.x - 2, r.y - 2, r.width + 4, r.height + 4}, 0.2f, 10, {70, 70, 70, 255}); // edge
        DrawRectangleRounded(r, 0.2f, 10, {250, 250, 245, 255});                                          // body

        float pr = r.width * 0.075f;
        for (int i = 0; i < 9; ++i) {
            if (!PIPS[face][i]) continue;
            float px = r.x + r.width * (0.25f + 0.25f * (i % 3));
            float py = r.y + r.height * (0.25f + 0.25f * (i / 3));
            Color c = (face == 1) ? Color{200, 30, 30, 255} : Color{25, 25, 25, 255};
            DrawCircleV({px, py}, face == 1 ? pr * 1.6f : pr, c);
        }
    }
};

// ============================ TokenAnimator ============================
// Moves a point along a list of waypoints (used for walking and snake/ladder slides).
class TokenAnimator {
    std::vector<Vector2> pts;
    size_t seg = 0;
    float t = 0.f, dur = 0.2f;
    bool smooth = false;

public:
    void start(const std::vector<Vector2>& p, float segDuration, bool smoothStep = false) {
        pts = p; seg = 0; t = 0.f; dur = segDuration; smooth = smoothStep;
    }
    // returns true when finished
    bool update(float dt, Vector2& out) {
        if (pts.size() < 2) return true;
        t += dt / dur;
        while (t >= 1.f) {
            t -= 1.f;
            ++seg;
            if (seg + 1 >= pts.size()) { out = pts.back(); return true; }
        }
        float k = smooth ? t * t * (3.f - 2.f * t) : t;
        out = vLerp(pts[seg], pts[seg + 1], k);
        return false;
    }
};

// ============================== BoardView ==============================
class BoardView {
public:
    // centre of a square in pixels; square 0 = the START pad below the board
    static Vector2 cellCenter(int n) {
        if (n <= 0) return {BOARD_X + 65.f, BOARD_Y + 10.f * CELL + 36.f};
        int idx = n - 1, row = idx / 10, col = idx % 10;
        if (row % 2 == 1) col = 9 - col; // zig-zag numbering like a real board
        return {BOARD_X + col * (float)CELL + CELL / 2.f, BOARD_Y + (9 - row) * (float)CELL + CELL / 2.f};
    }

    void draw(const Board& board) const {
        drawCells();
        for (const auto& e : board.getElements())
            if (e->type() == ElementType::Ladder) drawLadder(cellCenter(e->getStart()), cellCenter(e->getEnd()));
        int snakeIdx = 0;
        for (const auto& e : board.getElements())
            if (e->type() == ElementType::Snake) drawSnake(cellCenter(e->getStart()), cellCenter(e->getEnd()), snakeIdx++);
    }

private:
    void drawCells() const {
        static const Color pal[5] = {{255, 214, 102, 255}, {129, 212, 160, 255}, {125, 189, 240, 255},
                                     {244, 143, 177, 255}, {255, 171, 115, 255}};
        DrawRectangle(BOARD_X - 10, BOARD_Y - 10, CELL * 10 + 20, CELL * 10 + 20, {101, 67, 33, 255});
        for (int n = 1; n <= BOARD_SIZE; ++n) {
            Vector2 c = cellCenter(n);
            int x = (int)c.x - CELL / 2, y = (int)c.y - CELL / 2;
            int row = (n - 1) / 10, scol = (int)(c.x - BOARD_X) / CELL;
            Color col = (n == BOARD_SIZE) ? Color{255, 215, 0, 255} : pal[(row + scol) % 5];
            DrawRectangle(x, y, CELL, CELL, col);
            DrawRectangleLines(x, y, CELL, CELL, {90, 60, 30, 255});
            DrawText(TextFormat("%d", n), x + 5, y + 4, 16, {60, 40, 20, 255});
        }
        DrawText("HOME", (int)cellCenter(100).x - 20, (int)cellCenter(100).y + 4, 14, {120, 70, 0, 255});

        // START pad
        DrawRectangleRounded({(float)BOARD_X, BOARD_Y + 10.f * CELL + 18.f, 190.f, 38.f}, 0.4f, 8, {235, 215, 170, 255});
        DrawText("START", BOARD_X + 112, BOARD_Y + 10 * CELL + 28, 18, {101, 67, 33, 255});
    }

    void drawLadder(Vector2 a, Vector2 b) const {
        Vector2 d = vSub(b, a);
        float len = vLen(d);
        Vector2 u = vMul(d, 1.f / len), n = {-u.y, u.x};
        const float w = 9.f;
        Color rail = {120, 75, 35, 255}, rung = {170, 115, 60, 255};
        for (float s = 14.f; s < len - 8.f; s += 22.f) {
            Vector2 p = vAdd(a, vMul(u, s));
            DrawLineEx(vAdd(p, vMul(n, w)), vSub(p, vMul(n, w)), 4.f, rung);
        }
        DrawLineEx(vAdd(a, vMul(n, w)), vAdd(b, vMul(n, w)), 5.f, rail);
        DrawLineEx(vSub(a, vMul(n, w)), vSub(b, vMul(n, w)), 5.f, rail);
    }

    void drawSnake(Vector2 head, Vector2 tail, int idx) const {
        static const Color cols[] = {{34, 139, 34, 255}, {160, 32, 240, 255}, {220, 60, 40, 255},
                                     {30, 120, 200, 255}, {230, 140, 20, 255}};
        Color base = cols[idx % 5];
        Color light = {(unsigned char)(base.r + (255 - base.r) / 2), (unsigned char)(base.g + (255 - base.g) / 2),
                       (unsigned char)(base.b + (255 - base.b) / 2), 255};

        Vector2 d = vSub(tail, head);
        float len = vLen(d);
        Vector2 u = vMul(d, 1.f / len), n = {-u.y, u.x};
        int N = (int)(len / 3.f);
        if (N < 20) N = 20;
        float waves = len / 90.f;
        if (waves < 1.5f) waves = 1.5f;

        for (int i = N; i >= 0; --i) { // tail first, head drawn last on top
            float t = (float)i / N;
            float off = std::sin(t * waves * 2.f * PI) * 14.f * std::sin(t * PI);
            Vector2 p = vAdd(vAdd(head, vMul(d, t)), vMul(n, off));
            float r = 10.f - 6.f * t;
            Color c = (((int)(t * len / 14.f)) % 2 == 0) ? base : light;
            DrawCircleV(p, r, c);
        }
        // head
        Vector2 front = vMul(u, -1.f); // direction the head faces
        DrawLineEx(vAdd(head, vMul(front, 10.f)), vAdd(head, vMul(front, 22.f)), 2.f, {200, 20, 20, 255}); // tongue
        DrawCircleV(head, 13.f, base);
        for (int s = -1; s <= 1; s += 2) {
            Vector2 e = vAdd(vAdd(head, vMul(front, 3.f)), vMul(n, 6.f * s));
            DrawCircleV(e, 4.f, WHITE);
            DrawCircleV(vAdd(e, vMul(front, 1.5f)), 2.f, BLACK);
        }
    }
};

// =============================== GameApp ===============================
class GameApp {
    enum class State { Menu, Setup, WaitRoll, Rolling, Moving, Quiz, QuizResult, Sliding, GameOver };

    Game game;
    BoardView boardView;
    DiceView dice;
    TokenAnimator anim;
    State state = State::Menu;
    std::vector<Vector2> tokenPos;
    TurnResult turn;
    std::string message;
    Rectangle rollBtn;
    bool questionsLoaded = false;

    // ---- player setup (name + standard) ----
    int numPlayers = 2, setupIndex = 0, setupStep = 0; // step 0 = name, 1 = standard
    std::string input, setupError;
    std::vector<std::string> names;
    std::vector<int> standards;

    // ---- quiz ----
    const Question* currentQ = nullptr;
    int chosen = -1;

    static Color playerColor(int i) {
        static const Color c[4] = {{220, 40, 40, 255}, {40, 90, 220, 255}, {30, 160, 70, 255}, {240, 170, 20, 255}};
        return c[i % 4];
    }
    static Rectangle optionRect(int i) { return {70.f, 325.f + i * 56.f, 560.f, 48.f}; }

public:
    GameApp() : game(2), dice(765.f, 175.f, 120.f), rollBtn{715.f, 330.f, 240.f, 52.f} {}

    void run() {
        questionsLoaded = game.loadQuestions("questions.txt"); // must be in the same folder
        while (!WindowShouldClose()) {
            update(GetFrameTime());
            draw();
        }
    }

private:
    // ------------------------------ game flow ------------------------------
    void beginSetup(int n) {
        numPlayers = n;
        names.assign(n, "");
        standards.assign(n, MIN_STANDARD);
        setupIndex = 0;
        setupStep = 0;
        input.clear();
        setupError.clear();
        while (GetCharPressed() > 0) {} // throw away the key used in the menu
        state = State::Setup;
    }

    void startGame() {
        game.reset(numPlayers);
        for (int i = 0; i < numPlayers; ++i) game.setPlayerInfo(i, names[i], standards[i]);
        tokenPos.assign(numPlayers, BoardView::cellCenter(0));
        message = names[0] + " begins. Roll the dice!";
        dice.setFace(1);
        state = State::WaitRoll;
    }

    void finishTurn() { state = turn.won ? State::GameOver : State::WaitRoll; }

    void startSlideOrFinish() {
        if (turn.to != turn.landed) { // ladder up, or snake bite down
            anim.start({BoardView::cellCenter(turn.landed), BoardView::cellCenter(turn.to)}, 0.9f, true);
            state = State::Sliding;
        } else {
            finishTurn();
        }
    }

    // the token reached the square the dice showed
    void afterMove() {
        if (turn.landing == Landing::Snake) {
            currentQ = game.pickQuestion(turn.playerIndex); // question of THIS player's standard
            chosen = -1;
            if (currentQ) { state = State::Quiz; return; }
            game.biteWithoutQuiz(turn); // no question for this standard in the file
            message = turn.message;
        }
        startSlideOrFinish();
    }

    // ------------------------------ update ------------------------------
    void updateSetup() {
        int ch = GetCharPressed();
        while (ch > 0) {
            if (setupStep == 0) {
                if (ch >= 32 && ch <= 126 && input.size() < 10) input += (char)ch;
            } else {
                if (ch >= '0' && ch <= '9' && input.size() < 2) input += (char)ch;
            }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !input.empty()) input.pop_back();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (setupStep == 0) {
                names[setupIndex] = input.empty() ? "Player " + std::to_string(setupIndex + 1) : input;
                input.clear();
                setupStep = 1;
                setupError.clear();
            } else {
                int s = input.empty() ? 0 : std::atoi(input.c_str());
                if (s < MIN_STANDARD || s > MAX_STANDARD) {
                    setupError = "Please enter a standard from 1 to 10";
                } else {
                    standards[setupIndex] = s;
                    input.clear();
                    setupStep = 0;
                    setupError.clear();
                    if (++setupIndex >= numPlayers) startGame();
                }
            }
        }
    }

    void update(float dt) {
        switch (state) {
        case State::Menu:
            if (IsKeyPressed(KEY_TWO)) beginSetup(2);
            if (IsKeyPressed(KEY_THREE)) beginSetup(3);
            if (IsKeyPressed(KEY_FOUR)) beginSetup(4);
            break;

        case State::Setup:
            updateSetup();
            break;

        case State::WaitRoll:
            if (IsKeyPressed(KEY_SPACE) ||
                (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), rollBtn))) {
                turn = game.playTurn(); // backend decides the result immediately
                dice.startRoll();       // frontend plays the animation
                state = State::Rolling;
            }
            break;

        case State::Rolling:
            if (dice.update(dt, turn.roll)) {
                std::vector<Vector2> pts;
                for (int c = turn.from; c <= turn.landed; ++c) pts.push_back(BoardView::cellCenter(c));
                anim.start(pts, 0.2f);
                message = turn.message;
                state = State::Moving;
            }
            break;

        case State::Moving:
            if (anim.update(dt, tokenPos[turn.playerIndex])) afterMove();
            break;

        case State::Quiz: {
            int pick = -1;
            if (IsKeyPressed(KEY_A)) pick = 0;
            if (IsKeyPressed(KEY_B)) pick = 1;
            if (IsKeyPressed(KEY_C)) pick = 2;
            if (IsKeyPressed(KEY_D)) pick = 3;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                for (int i = 0; i < 4; ++i)
                    if (CheckCollisionPointRec(GetMousePosition(), optionRect(i))) pick = i;
            if (pick >= 0) {
                chosen = pick;
                game.answerQuiz(turn, currentQ->isCorrect(pick)); // right answer = snake cannot bite
                message = turn.message;
                state = State::QuizResult;
            }
            break;
        }

        case State::QuizResult:
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                startSlideOrFinish();
            break;

        case State::Sliding:
            if (anim.update(dt, tokenPos[turn.playerIndex])) finishTurn();
            break;

        case State::GameOver:
            if (IsKeyPressed(KEY_R)) state = State::Menu;
            break;
        }
    }

    // ------------------------------ drawing ------------------------------
    void drawToken(int i) const {
        static const Vector2 off[4] = {{-8, -8}, {8, -8}, {-8, 8}, {8, 8}};
        Vector2 p = vAdd(tokenPos[i], off[i]);
        DrawCircleV({p.x + 2, p.y + 3}, 13.f, Fade(BLACK, 0.35f));
        DrawCircleV(p, 13.f, {30, 30, 30, 255});
        DrawCircleV(p, 11.f, playerColor(game.getPlayers()[i].getColorIndex()));
        DrawCircleV({p.x - 4, p.y - 4}, 3.f, Fade(WHITE, 0.7f));
    }

    void drawCentered(const char* text, int y, int size, Color c) const {
        DrawText(text, (WIN_W - MeasureText(text, size)) / 2, y, size, c);
    }

    void drawSetup() const {
        DrawRectangle(0, 0, WIN_W, WIN_H, Fade(BLACK, 0.8f));
        drawCentered(TextFormat("Player %d of %d", setupIndex + 1, numPlayers), 220, 44, GOLD);
        if (setupStep == 0) {
            drawCentered("Enter your name:", 300, 28, WHITE);
        } else {
            drawCentered(TextFormat("Hello, %s!", names[setupIndex].c_str()), 280, 26, GOLD);
            drawCentered("Enter your standard (1 to 10):", 320, 28, WHITE);
        }
        Rectangle box = {350.f, 380.f, 300.f, 52.f};
        DrawRectangleRounded(box, 0.25f, 8, {250, 250, 245, 255});
        std::string shown = input + (((int)(GetTime() * 2.0)) % 2 == 0 ? "_" : "");
        DrawText(shown.c_str(), (int)box.x + 14, (int)box.y + 13, 28, {30, 30, 30, 255});
        drawCentered("Press ENTER to continue", 460, 20, LIGHTGRAY);
        if (!setupError.empty()) drawCentered(setupError.c_str(), 500, 22, {255, 120, 120, 255});
    }

    void drawQuiz() const {
        DrawRectangle(0, 0, WIN_W, WIN_H, Fade(BLACK, 0.55f));
        Rectangle panel = {50.f, 140.f, 600.f, 470.f};
        DrawRectangleRounded({panel.x - 4, panel.y - 4, panel.width + 8, panel.height + 8}, 0.06f, 10, {101, 67, 33, 255});
        DrawRectangleRounded(panel, 0.06f, 10, {255, 250, 235, 255});

        const Player& p = game.getPlayers()[turn.playerIndex];
        DrawText("SNAKE ALERT!", 75, 155, 28, {200, 30, 30, 255});
        DrawText(TextFormat("%s (Std %d) - answer correctly to avoid the bite!", p.getName().c_str(), p.getStandard()),
                 75, 192, 18, {80, 60, 40, 255});
        drawWrapped(currentQ->getText(), 75, 228, 550, 24, {40, 30, 20, 255});

        bool reveal = (state == State::QuizResult);
        for (int i = 0; i < 4; ++i) {
            Rectangle r = optionRect(i);
            Color bg = {235, 225, 200, 255};
            if (reveal) {
                if (i == currentQ->getCorrectIndex()) bg = {140, 210, 150, 255};
                else if (i == chosen) bg = {235, 140, 140, 255};
            } else if (CheckCollisionPointRec(GetMousePosition(), r)) {
                bg = {250, 225, 150, 255};
            }
            DrawRectangleRounded(r, 0.3f, 8, bg);
            DrawText(TextFormat("%c) %s", 'A' + i, currentQ->getOption(i).c_str()), (int)r.x + 16, (int)r.y + 13, 22,
                     {40, 30, 20, 255});
        }

        if (reveal) {
            bool ok = currentQ->isCorrect(chosen);
            DrawText(ok ? "Correct! The snake cannot bite you." : "Wrong answer! The snake bites you.", 75, 553, 22,
                     ok ? Color{30, 130, 60, 255} : Color{200, 30, 30, 255});
            DrawText("Press SPACE to continue", 75, 585, 18, {110, 90, 70, 255});
        } else {
            DrawText("Click an option or press A, B, C, D", 75, 570, 18, {110, 90, 70, 255});
        }
    }

    void draw() const {
        BeginDrawing();
        ClearBackground({245, 235, 215, 255});
        boardView.draw(game.getBoard());

        if (state != State::Menu && state != State::Setup) {
            for (size_t i = 0; i < tokenPos.size(); ++i) drawToken((int)i);

            // ---- side panel ----
            DrawText("SNAKES &", 725, 30, 34, {101, 67, 33, 255});
            DrawText("LADDERS", 725, 66, 34, {101, 67, 33, 255});
            dice.draw();

            bool canRoll = (state == State::WaitRoll);
            Color bc = canRoll ? Color{40, 150, 70, 255} : Color{150, 150, 150, 255};
            DrawRectangleRounded(rollBtn, 0.3f, 8, bc);
            const char* label = canRoll ? "ROLL DICE (SPACE)" : "...";
            DrawText(label, (int)(rollBtn.x + (rollBtn.width - MeasureText(label, 20)) / 2), (int)rollBtn.y + 16, 20, WHITE);

            const auto& pl = game.getPlayers();
            DrawText("Players", 715, 395, 22, {101, 67, 33, 255});
            for (size_t i = 0; i < pl.size(); ++i) {
                int y = 425 + (int)i * 42;
                if ((int)i == game.getCurrentIndex() && !game.isOver())
                    DrawRectangleRounded({710.f, (float)y - 3, 270.f, 40.f}, 0.3f, 6, Fade(playerColor(pl[i].getColorIndex()), 0.25f));
                DrawCircleV({728.f, (float)y + 16}, 10.f, playerColor(pl[i].getColorIndex()));
                DrawText(pl[i].getName().c_str(), 748, y + 1, 18, {40, 30, 20, 255});
                DrawText(TextFormat("Sq %d", pl[i].getPosition()), 900, y + 1, 18, {80, 60, 40, 255});
                DrawText(TextFormat("Std %d   Quiz %d/%d", pl[i].getStandard(), pl[i].getCorrect(), pl[i].getAsked()),
                         748, y + 21, 14, {100, 80, 60, 255});
            }
            drawWrapped(message, 715, 610, 260, 18, {60, 40, 20, 255});
        }

        if (state == State::Menu) {
            DrawRectangle(0, 0, WIN_W, WIN_H, Fade(BLACK, 0.75f));
            drawCentered("SNAKES & LADDERS", 250, 56, GOLD);
            drawCentered("Learn while you play - a correct answer saves you from the snake", 330, 22, LIGHTGRAY);
            drawCentered("Press 2, 3 or 4 to choose the number of players", 380, 24, WHITE);
            if (!questionsLoaded)
                drawCentered("questions.txt not found (keep it in the same folder) - quiz is off", 440, 20, {255, 120, 120, 255});
        }
        if (state == State::Setup) drawSetup();
        if (state == State::Quiz || state == State::QuizResult) drawQuiz();

        if (state == State::GameOver) {
            DrawRectangle(0, 0, WIN_W, WIN_H, Fade(BLACK, 0.7f));
            std::string w = game.getPlayers()[game.getWinner()].getName() + " wins!";
            drawCentered(w.c_str(), 200, 60, GOLD);
            drawCentered("QUIZ SCORE", 290, 26, WHITE);
            const auto& pl = game.getPlayers();
            for (size_t i = 0; i < pl.size(); ++i)
                drawCentered(TextFormat("%s (Std %d): %d / %d correct", pl[i].getName().c_str(), pl[i].getStandard(),
                                        pl[i].getCorrect(), pl[i].getAsked()),
                             330 + (int)i * 32, 22, LIGHTGRAY);
            drawCentered("Press R to play again", 520, 26, WHITE);
        }
        EndDrawing();
    }
};

// ================================== main ==================================
int main() {
    InitWindow(WIN_W, WIN_H, "Snakes and Ladders - OOP Project");
    SetTargetFPS(60);
    {
        GameApp app;
        app.run();
    }
    CloseWindow();
    return 0;
}