#include "weak.h"

#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace Weak;


struct Trace
{
    // One psqt for each piece for every king square - helps with king safety
    // [piece][kingsq (^56 for white)][sq (^56 for white)][color]
    // int32_t king_rel_psqt[5][64][64][2]{};
    // int32_t king_psqt[64][2]{};

    // Piece Square Tables
    int32_t psqt[6][64][2]{};

    // Score for piece mobility
    // Better to attack more squares
    // [piece][attacks][color]
    int32_t mobilities[4][28][2]{};

    // Piece attacking queen Bonus
    // [piece][king square][color]
    // int32_t piece_attack_queen[5][64][2];
};

// S(mg,eg) 
// constexpr array<array<array<int32_t, 64>, 64>, 5> king_rel_psqt{};
constexpr array<array<int32_t, 64>, 6> psqt{};
constexpr array<array<int32_t, 28>, 4> mobilities{};
// constexpr array<int32_t, 64> king_psqt{};
// constexpr array<array<int32_t, 64>,5> piece_attack_queen{};

static int gamePhaseInc[] = { 0, 1, 1, 2, 4, 0 };


static Trace trace_evaluate_extern(const chess::Board& board) {
    Trace trace{};

    int whiteKingSq = board.kingSq(chess::Color::WHITE).index();
    int blackKingSq = board.kingSq(chess::Color::BLACK).index();
    chess::Bitboard whiteQueens = board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE);
    chess::Bitboard blackQueens = board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK);
    bool isWhiteQueen = !whiteQueens.empty();
    bool isBlackQueen = !blackQueens.empty();
    int whiteQueenSq;
    int blackQueenSq;
    if (isWhiteQueen) whiteQueenSq = whiteQueens.lsb();
    if (isBlackQueen) blackQueenSq = blackQueens.lsb();

    chess::Bitboard wp = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    chess::Bitboard wn = board.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE);
    chess::Bitboard wb = board.pieces(chess::PieceType::BISHOP, chess::Color::WHITE);
    chess::Bitboard wr = board.pieces(chess::PieceType::ROOK, chess::Color::WHITE);
    chess::Bitboard wq = board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE);
    chess::Bitboard wk = board.pieces(chess::PieceType::KING, chess::Color::WHITE);
    chess::Bitboard bp = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK);
    chess::Bitboard bn = board.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK);
    chess::Bitboard bb = board.pieces(chess::PieceType::BISHOP, chess::Color::BLACK);
    chess::Bitboard br = board.pieces(chess::PieceType::ROOK, chess::Color::BLACK);
    chess::Bitboard bq = board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK);
    chess::Bitboard bk = board.pieces(chess::PieceType::KING, chess::Color::BLACK);
    chess::Bitboard all[] = {wp,wn,wb,wr,wq,wk,bp,bn,bb,br,bq,bk};

    for (int i = 0; i < 12; i++){        
        chess::Bitboard curr_bb = all[i];
        while (!curr_bb.empty()) {
            int sq = curr_bb.pop();
            bool isWhite = i < 6;
            int j = isWhite ? i : i-6;

            trace.psqt[j][isWhite ? sq ^ 56 : sq][isWhite ? 0 : 1]++;

            /*

            // < kings
            if (j < 5) {
                //PST
                trace.king_rel_psqt[j][isWhite ? whiteKingSq ^ 56 : blackKingSq][sq ^ (isWhite ? 56 : 0)][isWhite ? 0 : 1]++;
                
                // Piece Attack Queen
                bool attackQueen = false;
                if ((!isWhite && isWhiteQueen) || (isWhite && isBlackQueen)){
                    switch (j)
                    {
                        // pawns
                        case 0:
                            if (chess::attacks::pawn((isWhite ? chess::Color::WHITE : chess::Color::BLACK), static_cast<chess::Square>(sq)) & (isWhite ? bq : wq)) attackQueen = true;
                            break;
                        // knights
                        case 1:
                            if (chess::attacks::knight(static_cast<chess::Square>(sq)) & (isWhite ? bq : wq)) attackQueen = true;
                            break;
                        // bishops
                        case 2:

                            if (chess::attacks::bishop(static_cast<chess::Square>(sq), board.occ()) & (isWhite ? bq : wq)) attackQueen = true;
                            break;
                        // rooks
                        case 3:
                            if (chess::attacks::rook(static_cast<chess::Square>(sq), board.occ()) & (isWhite ? bq : wq)) attackQueen = true;
                            break;
                        // queens
                        case 4:
                            if (chess::attacks::queen(static_cast<chess::Square>(sq), board.occ()) & (isWhite ? bq : wq)) attackQueen = true;
                            break;
                        
                        default:
                            break;
                    }
                }
                if (attackQueen) trace.piece_attack_queen[j][isWhite ? blackQueenSq ^ 56 : whiteQueenSq][isWhite ? 0 : 1]++;
            }
            else {
                // PST
                trace.king_psqt[isWhite ? sq ^ 56 : sq][isWhite ? 0 : 1]++;
            }
                */


            // Mobilities for knight - queen
            if (j > 0 && j < 5){
                int attacks;
                switch (j)
                {
                    // knights
                    case 1:
                        attacks = chess::attacks::knight(static_cast<chess::Square>(sq)).count();
                        break;
                    // bishops
                    case 2:
                        attacks = chess::attacks::bishop(static_cast<chess::Square>(sq), board.occ()).count();
                        break;
                    // rooks
                    case 3:
                        attacks = chess::attacks::rook(static_cast<chess::Square>(sq), board.occ()).count();
                        break;
                    // queens
                    case 4:
                        attacks = chess::attacks::queen(static_cast<chess::Square>(sq), board.occ()).count();
                        break;
                    
                    default:
                        break;
                }
                trace.mobilities[j-1][attacks][isWhite ? 0 : 1]++;
            }
                
        }

    }

    return trace;
}


static coefficients_t get_coefficients(const Trace& trace)
{
    coefficients_t coefficients;
    
    /*
    // King-Relatie PST
    for (int i = 0; i < 5; i++){
        for (int j = 0; j < 64; j++){
            get_coefficient_array(coefficients, trace.king_rel_psqt[i][j], 64);
        }
    }
    // King PST
    get_coefficient_array(coefficients, trace.king_psqt, 64);

    */

    // PST
    for (int i = 0; i < 6; i++){
        get_coefficient_array(coefficients, trace.psqt[i], 64);
    }

    // Mobilities
    for (int i = 0; i < 4; i++) {
        get_coefficient_array(coefficients, trace.mobilities[i], 28);
    }

    /*

    // Piece Attack King
    for (int i = 0; i < 5; i++){
        get_coefficient_array(coefficients, trace.piece_attack_queen[i], 64);
    }
        */


    return coefficients;
}

parameters_t WeakEval::get_initial_parameters()
{
    parameters_t parameters;
    // PST

    for (int i = 0; i < 6; i++){
        get_initial_parameter_array(parameters, psqt[i], 64);
    }

    /*
    for (int i = 0; i < 5; i++){
        for (int j = 0; j < 64; j++){
            get_initial_parameter_array(parameters, king_rel_psqt[i][j], 64);
        }
    }

    // King PST
    get_initial_parameter_array(parameters, king_psqt, 64);

    */

    // Mobilities
    for (int i = 0; i < 4; i++){
        get_initial_parameter_array(parameters, mobilities[i], 28);
    }

    /*
    // Piece Attack King
    for (int i = 0; i < 5; i++){
        get_initial_parameter_array(parameters, piece_attack_queen[i], 64);
    }
        */

    return parameters;
}

EvalResult WeakEval::get_fen_eval_result(const string& fen) 
{
    EvalResult result;
    return result;
}

EvalResult WeakEval::get_external_eval_result(const chess::Board& board)
{
    auto trace = trace_evaluate_extern(board);
    EvalResult result;
    result.coefficients = get_coefficients(trace);
    return result;
}

static void print_parameter(std::stringstream& ss, const pair_t parameter)
{
    ss << "S(" << parameter[static_cast<int32_t>(PhaseStages::Midgame)] << ", " << parameter[static_cast<int32_t>(PhaseStages::Endgame)] << ")";
}

static void print_single(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name)
{
    ss << "constexpr int " << name << " = ";
    print_parameter(ss, parameters[index]);
    ss << ";" << endl;
    index++;
}

static void print_array(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, int count)
{
    ss << "constexpr int " << name << "[] = {";
    for (auto i = 0; i < count; i++)
    {
        print_parameter(ss, parameters[index]);
        index++;

        if (i != count - 1)
        {
            ss << ", ";
        }
    }
    ss << "};" << endl;
}


void WeakEval::print_parameters(const parameters_t& parameters)
{
    int index = 0;

    /*

    // PST
    cout << "King-Relative PST";
    for (int piece = 0; piece < 5; piece++) {
        std::stringstream ss;
        ss << "{\n    ";
        for (int kSq = 0; kSq < 64; kSq++){
            ss << "    {\n        ";
            for (int sq = 0; sq < 64; sq++) {
                const auto& pair = parameters[index++];
                tune_t mg = pair[0];
                tune_t eg = pair[1];

                ss << "S(" << (int)mg << ", " << (int)eg << ")";

                if (sq != 63)
                    ss << ", ";

                if ((sq + 1) % 8 == 0 && sq != 63)
                    ss << "\n        ";

            }
            ss << "\n    },\n\n";
        }

        ss << "\n},\n\n";
        std::cout << ss.str();
    }

    cout << "\nKing PST\n    ";
    for (int sq = 0; sq < 64; sq++) {
        const auto& pair = parameters[index++];
        tune_t mg = pair[0];
        tune_t eg = pair[1];

        cout << "S(" << (int)mg << ", " << (int)eg << ")";

        if (sq != 63)
            cout << ", ";

        if ((sq + 1) % 8 == 0 && sq != 63)
            cout << "\n    ";

    }

    cout << "\n\n";
    */

    // PST
    cout << "PSTs:\n    ";
    for (int pc = 0; pc < 6; pc++){
        cout << "{\n";
        for (int sq = 0; sq < 64; sq++) {
            const auto& pair = parameters[index++];
            tune_t mg = pair[0];
            tune_t eg = pair[1];

            cout << "S(" << (int)mg << ", " << (int)eg << ")";

            if (sq != 63)
                cout << ", ";

            if ((sq + 1) % 8 == 0 && sq != 63)
                cout << "\n    ";

        }

        cout << "\n}, \n\n";
    }



    // Mobilities
    cout << "\nMobilities\n\n";
    for (int piece = 1; piece < 5; piece++) {
        std::stringstream ss;
        ss << "{\n    ";

        for (int i = 0; i < 28; i++) {
            const auto& pair = parameters[index++];
            tune_t mg = pair[0];
            tune_t eg = pair[1];

            ss << "S(" << (int)mg << ", " << (int)eg << "), ";
        }

        ss << "\n},\n\n";
        std::cout << ss.str();
    }

    /*

    cout << "\nPiece Attacking Queen\n";
    for (int i = 0; i < 5; i++){
        cout << "{\n    ";
        for (int sq = 0; sq < 64; sq++) {
            const auto& pair = parameters[index++];
            tune_t mg = pair[0];
            tune_t eg = pair[1];

            cout << "S(" << (int)mg << ", " << (int)eg << ")";

            if (sq != 63)
                cout << ", ";

            if ((sq + 1) % 8 == 0 && sq != 63)
                cout << "\n    ";

        }
        cout << "\n},\n\n";
    }

    */

}