import chess
import chess.pgn
from random import randint, seed

filename = "testing/lichess_db_standard_rated_2013-01.pgn"
gameCount = 100
maxDiff = 200

seed(0)

def pass1():
    print("Filtrage des parties")
    file = open(filename, "r")
    lines = file.readlines()
    file.close()

    result = []

    for line in lines:
        if line.startswith("1.") and "eval" in line:
            result.append(line)

    file = open("testing/pass1.pgn", "w")
    file.write("\n".join(result))
    file.close()


print("Chargement des parties")
filename = "testing/pass1.pgn"
file = open(filename, "r")

games: list[chess.pgn.Game] = []

while (game := chess.pgn.read_game(file)) is not None:
    score = game.next().eval()
    if score is None:
        continue

    for i in range(randint(0, 50)):
        if game.is_end():
            break
        game = game.next()
    if game.is_end():
        continue

    score = game.next().eval()

    if score is None:
        continue

    value = score.relative.score()

    if value is None:
        continue

    if abs(value) > maxDiff:
        continue

    games.append(game)

    if len(games) % 10 == 0:
        print(len(games))

    if len(games) >= gameCount:
        break
file.close()

def printBB(bb):
    t = bin(bb)[2:].rjust(64, "0")
    for i in range(0, 64, 8):
        print(t[i:i+8])

print("Export des positions")
filename = "testing/positions.bin"
file = open(filename, "wb")

SHORT_CASTLE_BLACK = 0b0001
SHORT_CASTLE_WHITE = 0b0010
LONG_CASTLE_BLACK = 0b0100
LONG_CASTLE_WHITE = 0b1000

"""
Positions format (total size ) :
(BB : int64 or 8 bytes, castlingFlag : byte, isWhiteTurn : byte)

blackPawnBB blackBishopBB blackKnightBB blackRookBB blackQueenBB blackKingBB
whitePawnBB whiteBishopBB bwhiteKnightBB whiteRookBB whiteQueenBB whiteKingBB
castlingFlag
isWhiteTurn
"""
for game in games:
    board = game.board()

    file.write(board.pieces(chess.PAWN,   chess.BLACK).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.BISHOP, chess.BLACK).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.KNIGHT, chess.BLACK).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.ROOK,   chess.BLACK).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.QUEEN,  chess.BLACK).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.KING,   chess.BLACK).mask.to_bytes(8, "little"))

    file.write(board.pieces(chess.PAWN,   chess.WHITE).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.BISHOP, chess.WHITE).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.KNIGHT, chess.WHITE).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.ROOK,   chess.WHITE).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.QUEEN,  chess.WHITE).mask.to_bytes(8, "little"))
    file.write(board.pieces(chess.KING,   chess.WHITE).mask.to_bytes(8, "little"))

    castlingFlag = 0

    if board.castling_rights & chess.BB_H8:
        castlingFlag |= SHORT_CASTLE_BLACK
    if board.castling_rights & chess.BB_H1:
        castlingFlag |= SHORT_CASTLE_WHITE
    if board.castling_rights & chess.BB_A8:
        castlingFlag |= LONG_CASTLE_BLACK
    if board.castling_rights & chess.BB_A1:
        castlingFlag |= LONG_CASTLE_WHITE
    
    file.write(castlingFlag.to_bytes(1, "little"))

    file.write(bool(board.turn).to_bytes(1, "little"))

file.close()

print("Fini")
