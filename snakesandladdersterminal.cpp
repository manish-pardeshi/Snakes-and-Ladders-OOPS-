// ============================================================
//  SNAKES AND LADDERS - Simple terminal version (OOP project)
//  Classes: Dice, Player, Board, Game
// ============================================================
#include <iostream>
#include <string>
#include <cstdlib>   // rand(), srand()
#include <ctime>     // time()
#include <iomanip>   // setw()
using namespace std;

// ------------------------------------------------------------
// CLASS 1: Dice - rolls a number and shows it like a real dice
// ------------------------------------------------------------
class Dice {
public:
    int roll() {
        return rand() % 6 + 1;        // gives a number from 1 to 6
    }

    void show(int v) {
        cout << "\n +-------+\n";
        if (v == 1) cout << " |       |\n |   o   |\n |       |\n";
        else if (v == 2) cout << " | o     |\n |       |\n |     o |\n";
        else if (v == 3) cout << " | o     |\n |   o   |\n |     o |\n";
        else if (v == 4) cout << " | o   o |\n |       |\n | o   o |\n";
        else if (v == 5) cout << " | o   o |\n |   o   |\n | o   o |\n";
        else             cout << " | o   o |\n | o   o |\n | o   o |\n";
        cout << " +-------+\n";
        cout << " You rolled a " << v << "\n";
    }
};

// ------------------------------------------------------------
// CLASS 2: Player - stores a player's name and position
// ------------------------------------------------------------
class Player {
private:
    string name;
    int position;

public:
    Player() {                 // constructor: runs when object is created
        name = "";
        position = 0;          // 0 = not on the board yet
    }
    void setName(string n) { name = n; }
    string getName() { return name; }
    int getPosition() { return position; }
    void setPosition(int p) { position = p; }
};

// ------------------------------------------------------------
// CLASS 3: Board - knows where snakes and ladders are
// ------------------------------------------------------------
class Board {
private:
    int snakeHead[9];      // snake starts here ...
    int snakeTail[9];      // ... and takes you down to here
    int ladderBottom[10];  // ladder starts here ...
    int ladderTop[10];     // ... and takes you up to here

public:
    Board() {              // constructor: fills the snakes and ladders
        int h[9] = {99, 95, 92, 74, 64, 62, 49, 46, 16};
        int t[9] = {78, 75, 73, 53, 60, 19, 11, 25, 6};
        int b[10] = {2, 7, 8, 15, 21, 28, 36, 51, 71, 87};
        int u[10] = {38, 14, 31, 26, 42, 84, 44, 67, 91, 94};

        for (int i = 0; i < 9; i++) {
            snakeHead[i] = h[i];
            snakeTail[i] = t[i];
        }
        for (int i = 0; i < 10; i++) {
            ladderBottom[i] = b[i];
            ladderTop[i] = u[i];
        }
    }

    // if there is a snake on this square, return the tail, else same square
    int checkSnake(int pos) {
        for (int i = 0; i < 9; i++) {
            if (snakeHead[i] == pos)
                return snakeTail[i];
        }
        return pos;
    }

    // if there is a ladder on this square, return the top, else same square
    int checkLadder(int pos) {
        for (int i = 0; i < 10; i++) {
            if (ladderBottom[i] == pos)
                return ladderTop[i];
        }
        return pos;
    }

    void showSnakesAndLadders() {
        cout << "\nSNAKES (head -> tail): ";
        for (int i = 0; i < 9; i++)
            cout << snakeHead[i] << "->" << snakeTail[i] << "  ";
        cout << "\nLADDERS (bottom -> top): ";
        for (int i = 0; i < 10; i++)
            cout << ladderBottom[i] << "->" << ladderTop[i] << "  ";
        cout << "\n";
    }

    // prints the 10 x 10 board. A square with players shows P1, P2 ...
    void display(Player players[], int n) {
        cout << "\n";
        for (int row = 9; row >= 0; row--) {
            for (int col = 0; col < 10; col++) {
                int num;
                if (row % 2 == 0)
                    num = row * 10 + col + 1;      // left to right
                else
                    num = row * 10 + (10 - col);   // right to left

                string cell = "";
                for (int i = 0; i < n; i++) {
                    if (players[i].getPosition() == num)
                        cell = cell + "P" + to_string(i + 1);
                }
                if (cell == "")
                    cell = to_string(num);

                cout << setw(6) << cell;
            }
            cout << "\n\n";
        }
    }
};

// ------------------------------------------------------------
// CLASS 4: Game - controls turns and the rules of the game
// ------------------------------------------------------------
class Game {
private:
    Player players[4];
    int numPlayers;
    Board board;
    Dice dice;

public:
    Game(int n) {
        numPlayers = n;
        for (int i = 0; i < numPlayers; i++)
            players[i].setName("Player " + to_string(i + 1));
    }

    void play() {
        cout << "\n===== SNAKES AND LADDERS =====\n";
        board.showSnakesAndLadders();
        board.display(players, numPlayers);

        int turn = 0;              // whose turn it is (0 = Player 1)
        bool gameOver = false;

        while (gameOver == false) {
            cout << "------------------------------------------\n";
            cout << players[turn].getName() << " (square "
                 << players[turn].getPosition() << ") - press Enter to roll...";
            cin.get();

            int d = dice.roll();
            dice.show(d);

            int newPos = players[turn].getPosition() + d;

            if (newPos > 100) {
                cout << "You need an exact number to reach 100. Turn skipped.\n";
            } else {
                cout << "Moved to square " << newPos << "\n";

                int afterSnake = board.checkSnake(newPos);
                if (afterSnake != newPos) {
                    cout << "Oh no! A snake! Slide down to " << afterSnake << "\n";
                    newPos = afterSnake;
                }

                int afterLadder = board.checkLadder(newPos);
                if (afterLadder != newPos) {
                    cout << "Yay! A ladder! Climb up to " << afterLadder << "\n";
                    newPos = afterLadder;
                }

                players[turn].setPosition(newPos);
            }

            board.display(players, numPlayers);

            if (players[turn].getPosition() == 100) {
                cout << "*** " << players[turn].getName() << " WINS THE GAME! ***\n";
                gameOver = true;
            } else if (d == 6) {
                cout << "You got a 6, roll again!\n";
            } else {
                turn = turn + 1;                 // next player
                if (turn == numPlayers)
                    turn = 0;                    // go back to Player 1
            }
        }
    }
};

// ------------------------------------------------------------
// main
// ------------------------------------------------------------
int main() {
    srand(time(0));                // makes dice numbers different every run

    int n;
    cout << "Enter number of players (2 to 4): ";
    cin >> n;
    while (n < 2 || n > 4) {
        cout << "Please enter a number between 2 and 4: ";
        cin >> n;
    }
    cin.ignore(1000, '\n');        // removes the Enter key left after typing n

    Game game(n);
    game.play();

    return 0;
}