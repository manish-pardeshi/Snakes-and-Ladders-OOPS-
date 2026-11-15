// ===================== BACKEND (pure game logic, no raylib) — included by frontend.cpp =====================
#pragma once
#include <cctype>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

constexpr int BOARD_SIZE = 100;
constexpr int MIN_STANDARD = 1;
constexpr int MAX_STANDARD = 10;

enum class ElementType { Snake, Ladder };
enum class Landing { None, Snake, Ladder, Overshoot };

// ---------------------------------- Dice ----------------------------------
class Dice {
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
    int lastValue;

public:
    Dice() : rng(std::random_device{}()), dist(1, 6), lastValue(1) {}
    int roll() { return lastValue = dist(rng); }
    int last() const { return lastValue; }
};

// --------------------------------- Question ---------------------------------
// One multiple-choice question for one standard (class).
class Question {
    int standard;
    std::string text;
    std::vector<std::string> options; // always 4
    int correctIndex;                 // 0..3

public:
    Question(int s, const std::string& t, const std::vector<std::string>& o, char answerLetter)
        : standard(s), text(t), options(o), correctIndex(answerLetter - 'A') {}

    int getStandard() const { return standard; }
    const std::string& getText() const { return text; }
    const std::string& getOption(int i) const { return options[i]; }
    int getCorrectIndex() const { return correctIndex; }
    bool isCorrect(int index) const { return index == correctIndex; }
};

// ------------------------------- QuestionBank -------------------------------
// Loads questions.txt  ->  standard|question|A|B|C|D|correctLetter   (one per line)
class QuestionBank {
    std::vector<Question> questions;
    std::vector<bool> used;
    std::mt19937 rng;

public:
    QuestionBank() : rng(std::random_device{}()) {}

    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) return false;
        questions.clear();
        used.clear();

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows line ends
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::vector<std::string> parts;
            std::string part;
            while (std::getline(ss, part, '|')) parts.push_back(part);
            if (parts.size() != 7 || parts[6].empty()) continue;

            try {
                int s = std::stoi(parts[0]);
                char ans = (char)std::toupper((unsigned char)parts[6][0]);
                if (ans < 'A' || ans > 'D') continue;
                questions.emplace_back(s, parts[1], std::vector<std::string>(parts.begin() + 2, parts.begin() + 6), ans);
                used.push_back(false);
            } catch (...) {
                // bad line in the file: skip it
            }
        }
        return !questions.empty();
    }

    bool isLoaded() const { return !questions.empty(); }

    // random unused question of this standard (nullptr if none exist)
    const Question* pick(int standard) {
        std::vector<int> list;
        for (size_t i = 0; i < questions.size(); ++i)
            if (questions[i].getStandard() == standard && !used[i]) list.push_back((int)i);

        if (list.empty()) { // all used: make this standard's questions available again
            for (size_t i = 0; i < questions.size(); ++i)
                if (questions[i].getStandard() == standard) { used[i] = false; list.push_back((int)i); }
        }
        if (list.empty()) return nullptr;

        int chosen = list[rng() % list.size()];
        used[chosen] = true;
        return &questions[chosen];
    }
};

// --------------------------------- Player ---------------------------------
class Player {
    std::string name;
    int position;   // 0 = not yet on board
    int colorIndex; // frontend maps this to an actual colour
    int standard;   // class the student studies in (decides quiz difficulty)
    int asked;      // quiz questions asked
    int correct;    // quiz questions answered correctly

public:
    Player(const std::string& n, int color)
        : name(n), position(0), colorIndex(color), standard(MIN_STANDARD), asked(0), correct(0) {}

    const std::string& getName() const { return name; }
    int getPosition() const { return position; }
    int getColorIndex() const { return colorIndex; }
    int getStandard() const { return standard; }
    int getAsked() const { return asked; }
    int getCorrect() const { return correct; }

    void setName(const std::string& n) { name = n; }
    void setStandard(int s) { standard = s; }
    void setPosition(int p) { position = p; }
    void recordAnswer(bool wasCorrect) {
        ++asked;
        if (wasCorrect) ++correct;
    }
};

// ------------------- BoardElement (abstract) -> Snake / Ladder -------------------
class BoardElement {
protected:
    int start; // square where you land (snake head / ladder bottom)
    int end;   // square you are moved to (snake tail / ladder top)

public:
    BoardElement(int s, int e) : start(s), end(e) {
        if (s < 1 || s > BOARD_SIZE || e < 1 || e > BOARD_SIZE || s == e)
            throw std::invalid_argument("Invalid board element squares");
    }
    virtual ~BoardElement() = default;
    virtual ElementType type() const = 0;
    virtual std::string describe() const = 0;
    int getStart() const { return start; }
    int getEnd() const { return end; }
};

class Snake : public BoardElement {
public:
    Snake(int head, int tail) : BoardElement(head, tail) {
        if (head <= tail) throw std::invalid_argument("Snake head must be above its tail");
    }
    ElementType type() const override { return ElementType::Snake; }
    std::string describe() const override { return "Snake"; }
};

class Ladder : public BoardElement {
public:
    Ladder(int bottom, int top) : BoardElement(bottom, top) {
        if (bottom >= top) throw std::invalid_argument("Ladder top must be above its bottom");
    }
    ElementType type() const override { return ElementType::Ladder; }
    std::string describe() const override { return "Ladder"; }
};

// ---------------------------------- Board ----------------------------------
class Board {
    std::vector<std::unique_ptr<BoardElement>> elements;

public:
    Board() { loadDefault(); }

    void addSnake(int head, int tail) { elements.push_back(std::make_unique<Snake>(head, tail)); }
    void addLadder(int bottom, int top) { elements.push_back(std::make_unique<Ladder>(bottom, top)); }

    const BoardElement* elementAt(int pos) const {
        for (const auto& e : elements)
            if (e->getStart() == pos) return e.get();
        return nullptr;
    }
    int resolve(int pos) const {
        const BoardElement* e = elementAt(pos);
        return e ? e->getEnd() : pos;
    }
    const std::vector<std::unique_ptr<BoardElement>>& getElements() const { return elements; }

private:
    void loadDefault() {
        // snakes: head -> tail
        addSnake(99, 78); addSnake(95, 75); addSnake(92, 73);
        addSnake(74, 53); addSnake(64, 60); addSnake(62, 19);
        addSnake(49, 11); addSnake(46, 25); addSnake(16, 6);
        // ladders: bottom -> top
        addLadder(2, 38);  addLadder(7, 14);  addLadder(8, 31);
        addLadder(15, 26); addLadder(21, 42); addLadder(28, 84);
        addLadder(36, 44); addLadder(51, 67); addLadder(71, 91);
        addLadder(87, 94);
    }
};

// ------------------------- Result of one turn (for the UI) -------------------------
struct TurnResult {
    int playerIndex = 0;
    int roll = 0;
    int from = 0;     // square before the move
    int landed = 0;   // square reached by the dice (before snake/ladder)
    int to = 0;       // final square (for a snake: decided after the quiz answer)
    Landing landing = Landing::None;
    bool extraTurn = false;
    bool won = false;
    std::string message;
};

// ---------------------------------- Game ----------------------------------
class Game {
    Board board;
    Dice dice;
    QuestionBank bank;
    std::vector<Player> players;
    int current = 0;
    int winner = -1;

public:
    explicit Game(int numPlayers = 2) { reset(numPlayers); }

    void reset(int numPlayers) {
        if (numPlayers < 2) numPlayers = 2;
        if (numPlayers > 4) numPlayers = 4;
        players.clear();
        for (int i = 0; i < numPlayers; ++i)
            players.emplace_back("Player " + std::to_string(i + 1), i);
        current = 0;
        winner = -1;
    }

    // ---- quiz related ----
    bool loadQuestions(const std::string& filename) { return bank.loadFromFile(filename); }
    bool hasQuestions() const { return bank.isLoaded(); }

    void setPlayerInfo(int i, const std::string& name, int standard) {
        players[i].setName(name);
        players[i].setStandard(standard);
    }

    // question matching the player's standard (nullptr if the file has none)
    const Question* pickQuestion(int playerIndex) {
        return bank.pick(players[playerIndex].getStandard());
    }

    // player answered the snake question: right = no bite, wrong = bitten
    void answerQuiz(TurnResult& r, bool correct) {
        players[r.playerIndex].recordAnswer(correct);
        resolveSnake(r, correct);
    }

    // no question available for this standard: snake bites normally
    void biteWithoutQuiz(TurnResult& r) { resolveSnake(r, false); }

    // ---- one dice turn ----
    TurnResult playTurn() {
        TurnResult r;
        Player& p = players[current];
        r.playerIndex = current;
        r.roll = dice.roll();
        r.from = p.getPosition();
        r.landed = r.to = r.from;

        const std::string who = p.getName() + " rolled " + std::to_string(r.roll) + ". ";
        int target = r.from + r.roll;

        if (target > BOARD_SIZE) { // exact roll needed to finish
            r.landing = Landing::Overshoot;
            r.message = who + "Needs an exact roll to reach 100.";
        } else {
            r.landed = target;
            const BoardElement* e = board.elementAt(target);
            if (e && e->type() == ElementType::Snake) {
                r.landing = Landing::Snake;
                r.to = target; // not decided yet: depends on the quiz answer
                r.message = who + "A snake is here! Answer the question to avoid the bite.";
            } else if (e) {
                r.landing = Landing::Ladder;
                r.to = e->getEnd();
                r.message = who + "Climbed a ladder! " + std::to_string(r.landed) + " -> " + std::to_string(r.to);
            } else {
                r.to = target;
                r.message = who + "Moved to " + std::to_string(r.to) + ".";
            }
            p.setPosition(r.to);
        }

        r.won = (p.getPosition() == BOARD_SIZE);
        r.extraTurn = (r.roll == 6 && !r.won);
        if (r.won) {
            winner = current;
            r.message = p.getName() + " reached 100 and WINS!";
        } else if (r.extraTurn) {
            r.message += " Rolled a 6, roll again!";
        } else {
            current = (current + 1) % (int)players.size();
        }
        return r;
    }

    const Board& getBoard() const { return board; }
    const std::vector<Player>& getPlayers() const { return players; }
    const Player& getCurrentPlayer() const { return players[current]; }
    int getCurrentIndex() const { return current; }
    int getWinner() const { return winner; }
    bool isOver() const { return winner != -1; }

private:
    void resolveSnake(TurnResult& r, bool saved) {
        Player& p = players[r.playerIndex];
        if (saved) {
            r.to = r.landed;
            r.message = p.getName() + " answered correctly! The snake cannot bite. Stays on " +
                        std::to_string(r.landed) + ".";
        } else {
            r.to = board.resolve(r.landed); // tail of the snake
            p.setPosition(r.to);
            r.message = p.getName() + ": wrong answer! Bitten by the snake! " + std::to_string(r.landed) +
                        " -> " + std::to_string(r.to);
        }
        if (r.extraTurn) r.message += " Rolled a 6, roll again!";
    }
};