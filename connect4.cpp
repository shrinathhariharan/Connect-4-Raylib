#include "raylib.h"
#include <string>
#include <string_view>
#include <array>

namespace Settings
{
    constexpr int windowWidth{800};
    constexpr int windowHeight{800};

    constexpr int boardCols{7};
    constexpr int boardRows{6};

    constexpr std::string_view yellowTexture { "Connect4 Yellow Piece.png" };
    constexpr std::string_view redTexture    { "Connect4 Red Piece.png"    };
    constexpr std::string_view boardTexture  { "Connect4 Board.png"        };

    constexpr float srcImageSize { 1000.0f };

    constexpr float srcFirstCol{259.0f}; //sizing info
    constexpr float srcColStep {80.8f};
    constexpr float srcFirstRow{296.0f};
    constexpr float srcRowStep{77.2f};

    constexpr float srcPieceSize{500.0f};
}

class Piece
{
public:
    enum class Player { p1, p2, empty };

private:
    Player m_player { Player::empty };

public:
    explicit Piece(Player p = Player::empty) : m_player{p} {}

    Player getPlayer() const { return m_player; }
    bool isEmpty() const { return m_player == Player::empty; }

    bool operator==(Player p)       const { return m_player == p; }
    bool operator==(const Piece& o) const { return m_player == o.m_player; }
    bool operator!=(Player p)       const { return !(*this == p); }
    bool operator!=(const Piece& o) const { return !(*this == o); }
};

class Board
{
    std::array<std::array<Piece, Settings::boardRows>, Settings::boardCols> m_cells {};
    int m_moveCount { 0 };

public:
    Board() = default;

    Piece at(int col, int row) const { return m_cells[col][row]; }

    int moveCount() const { return m_moveCount; }

    int dropPiece(int col, Piece::Player player)
    {
        for (int row = Settings::boardRows - 1; row >= 0; --row)
        {
            if (m_cells[col][row].isEmpty())
            {
                m_cells[col][row] = Piece{player};
                ++m_moveCount;
                return row;
            }
        }
        return -1;
    }

    bool isColumnFull(int col) const {return !m_cells[col][0].isEmpty();}

    bool isBoardFull() const
    {
        for (int c = 0; c < Settings::boardCols; ++c)
            if (!isColumnFull(c)) return false;
        return true;
    }

    Piece::Player checkWinner() const
    {
        constexpr std::array dx{1, 0, 1, 1};
        constexpr std::array dy{0, 1, 1, -1};

        for (int col = 0; col < Settings::boardCols; ++col)
        {
            for (int row = 0; row < Settings::boardRows; ++row)
            {
                Piece p { m_cells[col][row] };
                if (p.isEmpty()) continue;

                for (int d = 0; d < 4; ++d)
                {
                    int count { 1 };
                    for (int step{1}; step < 4; ++step)
                    {
                        int nc{col + dx[d] * step};
                        int nr { row + dy[d] * step };
                        if (nc < 0 || nc >= Settings::boardCols) break;
                        if (nr < 0 || nr >= Settings::boardRows) break;
                        if (m_cells[nc][nr] != p) break;
                        ++count;
                    }
                    if (count == 4) return p.getPlayer();
                }
            }
        }
        return Piece::Player::empty;
    }
};

struct RenderLayout
{
    float  scale{};   // board image draw scale
    float  boardX{};   // top-left of board on screen
    float  boardY{};   // top-left of board on screen
    float  cellW{};   // pixel width  of one grid cell (on screen)
    float  cellH{};   // pixel height of one grid cell (on screen)
    float  firstCellX{};   // screen X of col-0 circle center
    float  firstCellY{};   // screen Y of row-0 circle center
};

RenderLayout computeLayout(int screenW, int screenH)
{
    constexpr float statusBarH{50.0f};
    float availH {static_cast<float>(screenH) - statusBarH};
    float availW {static_cast<float>(screenW)};

    // Fit board (square source image) into available area
    float scale { std::min(availW, availH) / Settings::srcImageSize };

    float boardW { Settings::srcImageSize * scale };
    float boardH { Settings::srcImageSize * scale };

    float boardX { (availW - boardW) / 2.0f };
    float boardY { statusBarH + (availH - boardH) / 2.0f };

    float cellW { Settings::srcColStep * scale };
    float cellH { Settings::srcRowStep * scale };

    float firstCellX { boardX + Settings::srcFirstCol * scale };
    float firstCellY { boardY + Settings::srcFirstRow * scale };

    return RenderLayout{scale, boardX, boardY, cellW, cellH, firstCellX, firstCellY};
}

Vector2 cellCenter(const RenderLayout& L, int col, int row)
{
    return Vector2 {
        L.firstCellX + col * L.cellW,
        L.firstCellY + row * L.cellH
    };
}

int mouseToColumn(const RenderLayout& L, float mouseX)
{
    // Each column spans ±(cellW/2) around its center
    float half { L.cellW / 2.0f };
    for (int c = 0; c < Settings::boardCols; ++c)
    {
        float cx { L.firstCellX + c * L.cellW };
        if (mouseX >= cx - half && mouseX < cx + half)
            return c;
    }
    return -1;
}

// Draw a single piece texture centered at (cx, cy) scaled so the
// piece circle matches the board hole.
void drawPiece(Texture2D tex, float cx, float cy, float cellW, float alpha = 255.0f)
{
    // We want the piece to fill ~90% of a cell width
    float destSize  { cellW * 0.90f };
    float pieceScale { destSize / Settings::srcPieceSize };

    float drawX { cx - destSize / 2.0f };
    float drawY { cy - destSize / 2.0f };

    Color tint { 255, 255, 255, static_cast<unsigned char>(alpha) };
    DrawTextureEx(tex, Vector2 { drawX, drawY }, 0.0f, pieceScale, tint);
}

enum class GamePhase { playing, won, draw };

struct GameState
{
    Board           board       {};
    Piece::Player   current     { Piece::Player::p1 };
    GamePhase       phase       { GamePhase::playing };
    Piece::Player   winner      { Piece::Player::empty };

    void switchPlayer() {current = (current == Piece::Player::p1) ? Piece::Player::p2 : Piece::Player::p1;}

    void reset()
    {
        board   = Board {};
        current = Piece::Player::p1;
        phase   = GamePhase::playing;
        winner  = Piece::Player::empty;
    }
};

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(Settings::windowWidth, Settings::windowHeight, "Connect 4");
    SetTargetFPS(60);

    // Load textures
    Texture2D texBoard  { LoadTexture(Settings::boardTexture.data())  };
    Texture2D texRed    { LoadTexture(Settings::redTexture.data())    };
    Texture2D texYellow { LoadTexture(Settings::yellowTexture.data()) };

    GameState game {};

    while (!WindowShouldClose())
    {
        // ── Layout (recalculated every frame to support window resize) ────
        int screenW {GetScreenWidth()};
        int screenH {GetScreenHeight()};
        RenderLayout layout { computeLayout(screenW, screenH) };

        // ── Mouse column ──────────────────────────────────────────────────
        Vector2 mouse     {GetMousePosition()};
        int     hoverCol  {mouseToColumn(layout, mouse.x)};

        // ── Input ─────────────────────────────────────────────────────────
        if (game.phase == GamePhase::playing)
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverCol >= 0)
            {
                int landedRow {game.board.dropPiece(hoverCol, game.current)};
                if (landedRow >= 0)          // valid move
                {
                    Piece::Player w { game.board.checkWinner() };
                    if (w != Piece::Player::empty)
                    {
                        game.winner = w;
                        game.phase  = GamePhase::won;
                    }
                    else if (game.board.isBoardFull())
                    {
                        game.phase = GamePhase::draw;
                    }
                    else
                    {
                        game.switchPlayer();
                    }
                }
            }
        }
        else
        {
            // Any click after game over → restart
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                game.reset();
        }

        //Draw
        BeginDrawing();
        ClearBackground(BLACK);

        // Status bar (above board)
        {
            std::string statusText;
            Color       statusColor { WHITE };

            if (game.phase == GamePhase::playing)
            {
                statusText  = (game.current == Piece::Player::p1)
                            ? "Player 1 (Red) - click a column"
                            : "Player 2 (Yellow) - click a column";
                statusColor = (game.current == Piece::Player::p1) ? RED : YELLOW;
            }
            else if (game.phase == GamePhase::won)
            {
                statusText  = (game.winner == Piece::Player::p1)
                            ? "Player 1 (Red) wins!  Click to play again"
                            : "Player 2 (Yellow) wins!  Click to play again";
                statusColor = (game.winner == Piece::Player::p1) ? RED : YELLOW;
            }
            else
            {
                statusText  = "It's a draw!  Click to play again";
                statusColor = WHITE;
            }

            int fontSize { 22 };
            int textW    { MeasureText(statusText.c_str(), fontSize) };
            DrawText(statusText.c_str(),
                     screenW / 2 - textW / 2, 12,
                     fontSize, statusColor);
        }

        // Column hover highlight
        if (game.phase == GamePhase::playing && hoverCol >= 0
            && !game.board.isColumnFull(hoverCol))
        {
            float cx   { layout.firstCellX + hoverCol * layout.cellW };
            float top  { layout.firstCellY - layout.cellH * 0.5f     };
            float bot  { layout.firstCellY + (Settings::boardRows - 0.5f) * layout.cellH };
            float w    { layout.cellW };
            float h    { bot - top };

            Color highlightColor { 255, 255, 255, 60 };
            DrawRectangleRec(Rectangle { cx - w / 2.0f, top, w, h }, highlightColor);
        }

        // Board texture (drawn on top of highlight so the highlight shows
        // through the holes in the board)
        DrawTextureEx(texBoard,
                      Vector2 { layout.boardX, layout.boardY },
                      0.0f, layout.scale, WHITE);

        // Pieces already placed
        for (int col{0}; col < Settings::boardCols; ++col)
        {
            for (int row{0}; row < Settings::boardRows; ++row)
            {
                Piece p { game.board.at(col, row) };
                if (p.isEmpty()) continue;

                Vector2 center { cellCenter(layout, col, row) };
                Texture2D& tex { (p == Piece::Player::p1) ? texRed : texYellow };
                drawPiece(tex, center.x, center.y, layout.cellW);
            }
        }

        // Ghost piece: show where the next drop would land
        if (game.phase == GamePhase::playing && hoverCol >= 0
            && !game.board.isColumnFull(hoverCol))
        {
            // Find landing row
            int landRow{-1};
            for (int r{Settings::boardRows - 1}; r >= 0; --r)
            {
                if (game.board.at(hoverCol, r).isEmpty())
                {
                    landRow = r;
                    break;
                }
            }

            if (landRow >= 0)
            {
                Vector2 center {cellCenter(layout, hoverCol, landRow)};
                Texture2D& tex {(game.current == Piece::Player::p1) ? texRed : texYellow};
                drawPiece(tex, center.x, center.y, layout.cellW, 120.0f); // semi-transparent
            }
        }

        EndDrawing();
    }

    UnloadTexture(texBoard);
    UnloadTexture(texRed);
    UnloadTexture(texYellow);

    CloseWindow();
    return 0;
}