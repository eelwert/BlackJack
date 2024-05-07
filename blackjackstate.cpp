#include <algorithm>
#include <random>
#include "blackjackstate.hpp"

static std::random_device randev;

static const size_t card_per_round = 3;
static const size_t power_limit = 21;
static const size_t max_card_id = 52;
static inline size_t card_power(size_t card_id) { return (card_id - 1) % 13 + 1; }
static inline size_t card_type(size_t card_id) { return (card_id - 1) / 13; }
static inline size_t cut_power(size_t power) { return (power > power_limit) ? power_limit - power : power; }

static inline std::vector<size_t> shuffle_iota(size_t start, size_t stop) {
    std::vector<size_t> result(stop - start, 0);
    for (size_t i = 0; i < result.size(); ++i) { result[i] = i + start; }
    std::mt19937 rangen(randev());
    std::shuffle(result.begin(), result.end(), rangen);
    return result;
}

BlackJackState::BlackJackState(size_t n_rounds_):
n_rounds(n_rounds_), cur_round_idx(0), player_success(0), opponent_success(0),
player_cards(n_rounds_ * card_per_round, 0), opponent_cards(n_rounds_ * card_per_round, 0) {
    auto card_arr_size = n_rounds_ * card_per_round;
    auto shuffled = shuffle_iota(1, max_card_id + 1);
    auto copy_pos = shuffled.begin();
    std::copy_n(copy_pos, card_arr_size, player_cards.begin());
    copy_pos += card_arr_size;
    std::copy_n(copy_pos, card_arr_size, opponent_cards.begin());
}

BlackJackState::~BlackJackState() {}

void BlackJackState::toggle_card(size_t card_idx) {
    if (card_selected(card_idx)) {
        unselect_card(card_idx);
    } else if (player_selection.size() < card_per_round) {
        select_card(card_idx);
    }
}

template <typename T>
static inline auto erase_idxs(std::vector<T>& vec, const std::unordered_set<size_t>& idxs) {
    return std::erase_if(vec, [&](const size_t& v) { return idxs.contains(&v - &vec.front()); });
}

bool BlackJackState::can_submit() {
    return player_selection.size() == card_per_round;
}

void BlackJackState::submit_round() {
    if (finished()) { return; }

    auto choice_vec = shuffle_iota(0, opponent_cards.size());
    for (size_t i = 0; i < card_per_round; ++i) { opponent_selection.insert(choice_vec[i]); }

    cur_round.emplace();

    cur_round->player_power = 0;
    for (auto&& idx : player_selection) { cur_round->player_power += card_power(player_cards[idx]); }
    cur_round->player_power = cut_power(cur_round->player_power);

    cur_round->opponent_power = 0;
    for (auto&& idx : opponent_selection) { cur_round->opponent_power += card_power(opponent_cards[idx]); }
    cur_round->opponent_power = cut_power(cur_round->opponent_power);

    if (cur_round->player_power == cur_round->opponent_power) {
        if (cur_round->player_power == power_limit) {
            ++player_success; ++opponent_success;
        }
        cur_round->result = result_code::TIE;
    } else if (cur_round->player_power > cur_round->opponent_power) {
        ++player_success; cur_round->result = result_code::WIN;
    } else if (cur_round->player_power < cur_round->opponent_power) {
        ++opponent_success; cur_round->result = result_code::LOSE;
    }
}

void BlackJackState::next_round() {
    erase_idxs(player_cards, player_selection); player_selection.clear();
    erase_idxs(opponent_cards, opponent_selection); opponent_selection.clear();
    cur_round.reset();
    ++cur_round_idx;
}

BlackJackState::result_code BlackJackState::status() {
    if (!finished()) { return result_code::UNKNOWN; }
    if (player_success == opponent_success) {
        return result_code::TIE;
    } else if (player_success > opponent_success) {
        return result_code::WIN;
    } else if (player_success < opponent_success) {
        return result_code::LOSE;
    } else {
        return result_code::UNKNOWN; // 不可能发生
    }
}
