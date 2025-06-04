// THIS FILE IS LICENSED MIT

#include "fourkdotcpp.h"

#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <sstream>

using namespace std;
using namespace Fourkdotcpp;

using u64 = uint64_t;
using i32 = int;

enum
{
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    None
};

static std::string pc_to_str[] = {"Pawn", "Knight", "Bishop", "Rook", "Queen", "King", "None"};

struct [[nodiscard]] Position {
    array<int, 4> castling = { true, true, true, true };
    array<u64, 2> colour = { 0xFFFFULL, 0xFFFF000000000000ULL };
    array<u64, 6> pieces = { 0xFF00000000FF00ULL,
                            0x4200000000000042ULL,
                            0x2400000000000024ULL,
                            0x8100000000000081ULL,
                            0x800000000000008ULL,
                            0x1000000000000010ULL };
    u64 ep = 0x0ULL;
    int flipped = false;

    auto operator<=>(const Position&) const = default;
};

[[nodiscard]] static u64 flip(u64 bb) {
    u64 result;
    char* bb_ptr = reinterpret_cast<char*>(&bb);
    char* result_ptr = reinterpret_cast<char*>(&result);
    for (int i = 0; i < sizeof(bb); i++)
    {
        result_ptr[i] = bb_ptr[sizeof(bb) - 1 - i];
    }
    return result;
}

[[nodiscard]] static auto lsb(const u64 bb) {
    return countr_zero(bb);
}

[[nodiscard]] static auto count(const u64 bb) {
    return popcount(bb);
}

[[nodiscard]] static auto east(const u64 bb) {
    return (bb << 1) & ~0x0101010101010101ULL;
}

[[nodiscard]] static auto west(const u64 bb) {
    return (bb >> 1) & ~0x8080808080808080ULL;
}

[[nodiscard]] static u64 north(const u64 bb) {
    return bb << 8;
}

[[nodiscard]] static u64 south(const u64 bb) {
    return bb >> 8;
}

[[nodiscard]] static u64 nw(const u64 bb) {
    return north(west(bb));
}

[[nodiscard]] static u64 ne(const u64 bb) {
    return north(east(bb));
}

[[nodiscard]] static u64 sw(const u64 bb) {
    return south(west(bb));
}

[[nodiscard]] static u64 se(const u64 bb) {
    return south(east(bb));
}

static void flip(Position& pos) {
    pos.colour[0] = flip(pos.colour[0]);
    pos.colour[1] = flip(pos.colour[1]);
    for (int i = 0; i < 6; ++i) {
        pos.pieces[i] = flip(pos.pieces[i]);
    }
    pos.ep = flip(pos.ep);
    swap(pos.colour[0], pos.colour[1]);
    swap(pos.castling[0], pos.castling[2]);
    swap(pos.castling[1], pos.castling[3]);
    pos.flipped = !pos.flipped;
}

template <typename F>
[[nodiscard]] auto ray(const int sq, const u64 blockers, F f) {
    u64 mask = f(1ULL << sq);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    mask |= f(mask & ~blockers);
    return mask;
}

[[nodiscard]] u64 knight(const int sq, const u64) {
    const u64 bb = 1ULL << sq;
    return (((bb << 15) | (bb >> 17)) & 0x7F7F7F7F7F7F7F7FULL) | (((bb << 17) | (bb >> 15)) & 0xFEFEFEFEFEFEFEFEULL) |
        (((bb << 10) | (bb >> 6)) & 0xFCFCFCFCFCFCFCFCULL) | (((bb << 6) | (bb >> 10)) & 0x3F3F3F3F3F3F3F3FULL);
}

[[nodiscard]] static auto bishop(const int sq, const u64 blockers) {
    return ray(sq, blockers, nw) | ray(sq, blockers, ne) | ray(sq, blockers, sw) | ray(sq, blockers, se);
}

[[nodiscard]] static auto rook(const int sq, const u64 blockers) {
    return ray(sq, blockers, north) | ray(sq, blockers, east) | ray(sq, blockers, south) | ray(sq, blockers, west);
}

[[nodiscard]] static u64 king(const int sq, const u64) {
    const u64 bb = 1ULL << sq;
    return (bb << 8) | (bb >> 8) | (((bb >> 1) | (bb >> 9) | (bb << 7)) & 0x7F7F7F7F7F7F7F7FULL) |
        (((bb << 1) | (bb << 9) | (bb >> 7)) & 0xFEFEFEFEFEFEFEFEULL);
}

static void set_fen(Position& pos, const string& fen) {
    // Clear
    pos.colour = {};
    pos.pieces = {};
    pos.castling = {};

    stringstream ss{ fen };
    string word;

    ss >> word;
    int i = 56;
    for (const auto c : word) {
        if (c >= '1' && c <= '8') {
            i += c - '1' + 1;
        }
        else if (c == '/') {
            i -= 16;
        }
        else {
            const int side = c == 'p' || c == 'n' || c == 'b' || c == 'r' || c == 'q' || c == 'k';
            const int piece = (c == 'p' || c == 'P') ? Pawn
                : (c == 'n' || c == 'N') ? Knight
                : (c == 'b' || c == 'B') ? Bishop
                : (c == 'r' || c == 'R') ? Rook
                : (c == 'q' || c == 'Q') ? Queen
                : King;
            pos.colour.at(side) ^= 1ULL << i;
            pos.pieces.at(piece) ^= 1ULL << i;
            i++;
        }
    }

    // Side to move
    ss >> word;
    const bool black_move = word == "b";

    // Castling permissions
    ss >> word;
    for (const auto c : word) {
        pos.castling[0] |= c == 'K';
        pos.castling[1] |= c == 'Q';
        pos.castling[2] |= c == 'k';
        pos.castling[3] |= c == 'q';
    }

    // En passant
    ss >> word;
    if (word != "-") {
        const int sq = word[0] - 'a' + 8 * (word[1] - '1');
        pos.ep = 1ULL << sq;
    }

    // Flip the board if necessary
    if (black_move) {
        flip(pos);
    }
}

[[nodiscard]] static u64 get_mobility(const i32 sq, const i32 piece,
    const Position* pos) {
    u64 moves = 0;
/*    if (piece == Pawn) {
        moves = north(1ULL << sq);
    } else*/ if (piece == Knight) {
        moves = knight(sq, 0);
    }
    else if (piece == King) {
        moves = king(sq, 0);
    }
    else {
        const u64 blockers = pos->colour[0] | pos->colour[1];
        if (piece == Rook || piece == Queen) {
            moves |= rook(sq, blockers);
        }
        if (piece == Bishop || piece == Queen) {
            moves |= bishop(sq, blockers);
        }
    }
    return moves;
}

struct Trace
{
    int score;
    tune_t endgame_scale;

    int material[6][2]{};
    int pst_rank[48][2]{};
    int pst_file[48][2]{};
    int mobilities[6][2]{};
    int king_attacks[6][2]{};
    int open_files[2][6][2]{};
    //int protected_pawn[2]{};
    //int passed_pawns[7][2]{};
    //int passed_pawn[2]{};
    //int phalanx_pawn[2]{};
    int bishop_pair[2]{};
};

const i32 phases[] = {0, 1, 1, 2, 4, 0};
const i32 material[] = {S(89, 147), S(350, 521), S(361, 521), S(479, 956), S(1046, 1782), 0};
const i32 pst_rank[] = {
    0,         S(-3, 0),  S(-3, -1), S(-1, -1), S(1, 0),  S(5, 3),  0,        0,          // Pawn
    S(-2, -5), S(0, -3),  S(1, -1),  S(3, 3),   S(4, 4),  S(5, 1),  S(2, 0),  S(-15, 1),  // Knight
    S(0, -2),  S(2, -1),  S(2, 0),   S(2, 0),   S(2, 0),  S(2, 0),  S(-1, 0), S(-10, 2),  // Bishop
    S(0, -3),  S(-1, -3), S(-2, -2), S(-2, 0),  S(0, 2),  S(2, 2),  S(1, 3),  S(2, 1),    // Rook
    S(2, -11), S(3, -8),  S(2, -3),  S(0, 2),   S(0, 5),  S(-1, 5), S(-4, 7), S(-2, 4),   // Queen
    S(-1, -6), S(1, -2),  S(-1, 0),  S(-4, 3),  S(-1, 5), S(5, 4),  S(5, 2),  S(5, -6),   // King
};
const i32 pst_file[] = {
    S(-1, 1),  S(-2, 1),  S(-1, 0), S(0, -1), S(1, 0),  S(2, 0),  S(2, 0),  S(-1, 0),   // Pawn
    S(-4, -3), S(-1, -1), S(0, 1),  S(2, 3),  S(2, 3),  S(2, 0),  S(1, -1), S(-1, -3),  // Knight
    S(-2, -1), 0,         S(1, 0),  S(0, 1),  S(1, 1),  S(0, 1),  S(2, 0),  S(-1, -1),  // Bishop
    S(-2, 0),  S(-1, 1),  S(0, 1),  S(1, 0),  S(2, -1), S(1, 0),  S(1, 0),  S(-1, -1),  // Rook
    S(-2, -3), S(-1, -1), S(-1, 0), S(0, 1),  S(0, 2),  S(1, 2),  S(2, 0),  S(1, -1),   // Queen
    S(-2, -5), S(2, -1),  S(-1, 1), S(-4, 2), S(-4, 2), S(-2, 2), S(2, -1), S(0, -5),   // King
};
const i32 open_files[2][6] = { 0 };
const i32 mobilities[] = { 0,0,0,0,0,0 };
const i32 king_attacks[] = { 0,0,0,0,0,0 };
//const i32 protected_pawn = 0;
//const i32 passed_pawn = 0;
//const i32 passed_pawns[] = { 0,0,0,0,0,0,0 };
//const i32 phalanx_pawn = 0;
const i32 bishop_pair = 0;

#define TraceIncr(parameter) trace.parameter[color]++
#define TraceAdd(parameter, count) trace.parameter[color] += count

static Trace eval(Position& pos) {
    Trace trace{};
    int score = S(16, 16);
    int phase = 0;

    for (int c = 0; c < 2; ++c) {
        const int color = pos.flipped;

        const u64 own_pawns = pos.colour[0] & pos.pieces[Pawn];
        const u64 opp_pawns = pos.colour[1] & pos.pieces[Pawn];
        u64 no_passers = pos.colour[1] & pos.pieces[Pawn];
        no_passers |= se(no_passers) | sw(no_passers);
        const u64 opp_king_zone = king(lsb(pos.colour[1] & pos.pieces[King]), 0);
        //const u64 pawn_protected = nw(own_pawns) | ne(own_pawns);

        //score += protected_pawn * count(own_pawns & (nw(own_pawns) | ne(own_pawns)));
        //TraceAdd(protected_pawn, count(own_pawns & (nw(own_pawns) | ne(own_pawns))));

        //score += phalanx_pawn * count(own_pawns & west(own_pawns));
        //TraceAdd(phalanx_pawn, count(own_pawns & west(own_pawns)));

        if (count(pos.colour[0] & pos.pieces[Bishop]) == 2) {
            score += bishop_pair;
            TraceIncr(bishop_pair);
        }

        // For each piece type
        for (int p = 0; p < 6; ++p) {
            auto copy = pos.colour[0] & pos.pieces[p];
            while (copy) {
                const int sq = lsb(copy);
                copy &= copy - 1;

                // Material
                phase += phases[p];
                score += material[p];
                TraceIncr(material[p]);

                const int rank = sq / 8;
                const int file = sq % 8;

                // Split quantized PSTs
                score += pst_rank[p * 8 + rank] * 1;
                TraceAdd(pst_rank[p * 8 + rank], 1);

                score += pst_file[p * 8 + file] * 1;
                TraceAdd(pst_file[p * 8 + file], 1);

                if ((north(0x101010101010101UL << sq) & own_pawns) == 0) {
                    score += open_files[(north(0x101010101010101UL << sq) & opp_pawns) == 0][p];
                    TraceIncr(open_files[(north(0x101010101010101UL << sq) & opp_pawns) == 0][p]);
                }

                //if (p == Pawn && !(0x101010101010101ull << sq & no_passers)) {
                //    score += passed_pawns[rank];
                //    TraceIncr(passed_pawns[rank]);
                //}

                const u64 mobility = get_mobility(sq, p /*== King ? Queen : p*/, &pos);
                if (p > Knight) {
                    score += mobilities[p] * count(mobility & ~pos.colour[0]);
                    TraceAdd(mobilities[p], count(mobility & ~pos.colour[0]));

                    if (p != King && p != Pawn) {
                        score += king_attacks[p] * count(mobility & opp_king_zone);
                        TraceAdd(king_attacks[p], count(mobility & opp_king_zone));
                    }
                }
            }
        }

        flip(pos);

        score = -score;
    }

#if TAPERED
    // Tapered eval with endgame scaling based on remaining pawn count of the stronger side
    int stronger_colour = score < 0;
    auto stronger_colour_pieces = pos.colour[stronger_colour];
    auto stronger_colour_pawns = stronger_colour_pieces & pos.pieces[Pawn];
    auto stronger_colour_pawn_count = count(stronger_colour_pawns);
    auto stronger_colour_pawns_missing = 8 - stronger_colour_pawn_count;
    auto scale = (128 - stronger_colour_pawns_missing * stronger_colour_pawns_missing) / static_cast<tune_t>(128);

    scale = 1;
    trace.endgame_scale = scale;
    trace.score = ((short)score * phase + ((score + 0x8000) >> 16) * scale * (24 - phase)) / 24;
#else
    trace.endgame_scale = 1;
    trace.score = score;
#endif

    if (pos.flipped)
    {
        trace.score = -trace.score;
    }
    return trace;
}

static int32_t round_value(tune_t value)
{
    return static_cast<int32_t>(round(value));
}

#if TAPERED

static void print_parameter(std::stringstream& ss, const pair_t parameter)
{
    const auto mg = round_value(parameter[static_cast<int32_t>(PhaseStages::Midgame)]);
    const auto eg = round_value(parameter[static_cast<int32_t>(PhaseStages::Endgame)]);
    if (mg == 0 && eg == 0)
    {
        ss << 0;
    }
    else
    {
        ss << "S(" << mg << ", " << eg << ")";
    }
}

static void print_parameter_tapered(std::stringstream& ss, const PhaseStages phase, const pair_t parameter)
{
    const auto val = round_value(parameter[static_cast<int32_t>(phase)]);
    ss << val;
}

#else
static void print_parameter(std::stringstream& ss, const tune_t parameter)
{
    ss << round_value(std::round(parameter));
}
#endif

static void print_single_tapered(std::stringstream& ss, const parameters_t& parameters, int& index, const PhaseStages phase, const std::string& name)
{
    ss << "." << name << " = ";
    print_parameter_tapered(ss, phase, parameters[index]);
    index++;

    ss << "," << endl;
}

static void print_single(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name)
{
    ss << "const i8 " << name << " = ";
    print_parameter(ss, parameters[index]);
    index++;

    ss << ";" << endl;
}

static void print_array_tapered(std::stringstream& ss, const parameters_t& parameters, int& index, const PhaseStages phase, const std::string& name, const int count)
{
    ss << "." << name << " = {";
    for (auto i = 0; i < count; i++)
    {
        print_parameter_tapered(ss, phase, parameters[index]);
        index++;

        if (i != count - 1)
        {
            ss << ", ";
        }
    }
    ss << "}," << endl;
}

static void print_array(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, const int count)
{
    ss << "__attribute__((aligned(8))) static const i8 " << name << "[] = {";
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

static void print_pst_tapered(std::stringstream& ss, const parameters_t& parameters, int& index, const PhaseStages phase, const std::string& name)
{
    ss << "." << name << " = {";
    for (auto i = 0; i < 48; i++)
    {
        print_parameter_tapered(ss, phase, parameters[index]);
        index++;

        ss << ", ";

        if (i % 8 == 7)
        {
            ss << "// " << pc_to_str[i / 8] << "\n";
        }
    }
    ss << "}," << endl;
}

static void print_pst(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name)
{
    ss << "__attribute__((aligned(8))) static const i8 " << name << "[] = {";
    for (auto i = 0; i < 48; i++)
    {
        print_parameter(ss, parameters[index]);
        index++;

        ss << ", ";

        if (i % 8 == 7)
        {
            ss << "// " << pc_to_str[i / 8] << "\n";
        }
    }
    ss << "};" << endl;
}

static void print_array_2d_tapered(std::stringstream& ss, const parameters_t& parameters, int& index, const PhaseStages phase, const std::string& name, const int count1, const int count2)
{
    ss << "." << name << " = {\n";
    for (auto i = 0; i < count1; i++)
    {
        ss << "    {";
        for (auto j = 0; j < count2; j++)
        {
            print_parameter_tapered(ss, phase, parameters[index]);
            index++;

            if (j != count2 - 1)
            {
                ss << ", ";
            }
        }
        ss << "},\n";
    }
    ss << "},\n";
}

static void print_array_2d(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, const int count1, const int count2)
{
    ss << "const i32 " << name << "[][" << count2 << "] = {\n";
    for (auto i = 0; i < count1; i++)
    {
        ss << "    {";
        for (auto j = 0; j < count2; j++)
        {
            print_parameter(ss, parameters[index]);
            index++;

            if (j != count2 - 1)
            {
                ss << ", ";
            }
        }
        ss << "},\n";
    }
    ss << "};\n";
}

static void rebalance_psts(parameters_t& parameters, const int32_t pst_offset, bool pawn_exclusion, const int32_t pst_size, const int32_t quantization)
{
    for (auto pieceIndex = 0; pieceIndex < 5; pieceIndex++)
    {
        const int pstStart = pst_offset + pieceIndex * pst_size;
#if TAPERED
        for (int stage = 0; stage < 2; stage++)
        {
#endif
            double sum = 0;
            for (auto i = 0; i < pst_size; i++)
            {
                if (pieceIndex == 0 && pawn_exclusion && (i == 0 || i == pst_size - 1 || i == pst_size - 2))
                {
                    continue;
                }
                const auto pstIndex = pstStart + i;
#if TAPERED
                sum += parameters[pstIndex][stage];
#else
                sum += parameters[pstIndex];
#endif
            }

            const auto average = sum / (pieceIndex == 0 && pawn_exclusion ? pst_size - 3 : pst_size);
            //const auto average = sum / pst_size;
#if TAPERED
            parameters[pieceIndex][stage] += average * quantization;
#else
            parameters[pieceIndex] += average * quantization;
#endif
            for (auto i = 0; i < pst_size; i++)
            {
                if (pieceIndex == 0 && pawn_exclusion && (i == 0 || i == pst_size - 1 || i == pst_size - 2))
                {
                    continue;
                }
                const auto pstIndex = pstStart + i;
#if TAPERED
                parameters[pstIndex][stage] -= average;
#else
                parameters[pstIndex] -= average;
#endif
            }
#if TAPERED
        }
#endif
    }
}

parameters_t FourkdotcppEval::get_initial_parameters()
{
    parameters_t parameters;
    get_initial_parameter_array(parameters, material, 6);
    get_initial_parameter_array(parameters, pst_rank, 48);
    get_initial_parameter_array(parameters, pst_file, 48);
    get_initial_parameter_array(parameters, mobilities, 6);
    get_initial_parameter_array(parameters, king_attacks, 6);
    get_initial_parameter_array_2d(parameters, open_files, 2, 6);
    get_initial_parameter_single(parameters, bishop_pair);
    return parameters;
}

static coefficients_t get_coefficients(const Trace& trace)
{
    coefficients_t coefficients;
    get_coefficient_array(coefficients, trace.material, 6);
    get_coefficient_array(coefficients, trace.pst_rank, 48);
    get_coefficient_array(coefficients, trace.pst_file, 48);
    get_coefficient_array(coefficients, trace.mobilities, 6);
    get_coefficient_array(coefficients, trace.king_attacks, 6);
    get_coefficient_array_2d(coefficients, trace.open_files, 2, 6);
    get_coefficient_single(coefficients, trace.bishop_pair);
    return coefficients;
}

static void print_parameters_tapered(const parameters_t& parameters)
{
    stringstream ss;

    for (auto i = 0; i < 2; i++)
    {
        const auto phase = static_cast<PhaseStages>(i);
        ss << (phase == PhaseStages::Midgame ? "MIDGAME:" : "ENDGAME:") << endl;
        int index = 0;

        print_array_tapered(ss, parameters, index, phase, "material", 6);
        print_pst_tapered(ss, parameters, index, phase, "pst_rank");
        print_pst_tapered(ss, parameters, index, phase, "pst_file");
        print_array_tapered(ss, parameters, index, phase, "mobilities", 6);
        print_array_tapered(ss, parameters, index, phase, "king_attacks", 6);
        print_array_2d_tapered(ss, parameters, index, phase, "open_files", 2, 6);
        print_single_tapered(ss, parameters, index, phase, "bishop_pair");
    }

    cout << ss.str() << "\n";
}

void FourkdotcppEval::print_parameters(const parameters_t& parameters)
{
    parameters_t parameters_copy = parameters;
    rebalance_psts(parameters_copy, 6, true, 8, 1);
    rebalance_psts(parameters_copy, 6 + 6 * 8, false, 8, 1);

    print_parameters_tapered(parameters_copy);

    return;

    //int index = 0;
    //stringstream ss;
    //print_array(ss, parameters_copy, index, "material", 6);
    //print_pst(ss, parameters_copy, index, "pst_rank");
    //print_pst(ss, parameters_copy, index, "pst_file");
    //print_array(ss, parameters_copy, index, "mobilities", 6);
    //print_array(ss, parameters_copy, index, "king_attacks", 6);
    //print_array(ss, parameters_copy, index, "open_files", 6);
    //print_single(ss, parameters_copy, index, "bishop_pair");
    //cout << ss.str() << "\n";
}

static Position get_position_from_external(const chess::Board& board)
{
    Position position;

    position.flipped = false;

    position.colour[0] = board.us(chess::Color::WHITE).getBits();
    position.colour[1] = board.them(chess::Color::WHITE).getBits();

    position.pieces[Pawn] = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE).getBits() | board.pieces(chess::PieceType::PAWN, chess::Color::BLACK).getBits();
    position.pieces[Knight] = board.pieces(chess::PieceType::KNIGHT, chess::Color::WHITE).getBits() | board.pieces(chess::PieceType::KNIGHT, chess::Color::BLACK).getBits();
    position.pieces[Bishop] = board.pieces(chess::PieceType::BISHOP, chess::Color::WHITE).getBits() | board.pieces(chess::PieceType::BISHOP, chess::Color::BLACK).getBits();
    position.pieces[Rook] = board.pieces(chess::PieceType::ROOK, chess::Color::WHITE).getBits() | board.pieces(chess::PieceType::ROOK, chess::Color::BLACK).getBits();
    position.pieces[Queen] = board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE).getBits() | board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK).getBits();
    position.pieces[King] = board.pieces(chess::PieceType::KING, chess::Color::WHITE).getBits() | board.pieces(chess::PieceType::KING, chess::Color::BLACK).getBits();

    position.castling[0] = board.castlingRights().has(chess::Color::WHITE, chess::Board::CastlingRights::Side::KING_SIDE);
    position.castling[1] = board.castlingRights().has(chess::Color::WHITE, chess::Board::CastlingRights::Side::QUEEN_SIDE);
    position.castling[2] = board.castlingRights().has(chess::Color::BLACK, chess::Board::CastlingRights::Side::KING_SIDE);
    position.castling[3] = board.castlingRights().has(chess::Color::BLACK, chess::Board::CastlingRights::Side::QUEEN_SIDE);

    position.ep = board.enpassantSq().index();
    if(position.ep == 64)
    {
        position.ep = 0;
    }
    if(position.ep != 0)
    {
        position.ep = 1ULL << position.ep;
    }

    if (board.sideToMove() == chess::Color::BLACK)
    {
        flip(position);
    }

    //Position position2;
    //set_fen(position2, board.getFen());
    //if(position != position2)
    //{
    //    throw std::runtime_error("Position mismatch");
    //}

    return position;
}

EvalResult FourkdotcppEval::get_fen_eval_result(const string& fen)
{
    Position position;
    set_fen(position, fen);
    const auto trace = eval(position);
    EvalResult result;
    result.coefficients = get_coefficients(trace);
    result.score = trace.score;
    result.endgame_scale = trace.endgame_scale;
    return result;
}

EvalResult FourkdotcppEval::get_external_eval_result(const chess::Board& board)
{
    auto position = get_position_from_external(board);
    const auto trace = eval(position);
    EvalResult result;
    result.coefficients = get_coefficients(trace);
    result.score = trace.score;
    result.endgame_scale = trace.endgame_scale;

    return result;
}