#ifndef BLACKJACKSTATE_HPP
#define BLACKJACKSTATE_HPP

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <optional>

struct BlackJackState {
    enum class result_code { UNKNOWN, WIN, LOSE, TIE };
    struct round_status { ssize_t player_power, opponent_power; result_code result; };

    std::vector<size_t> player_cards, opponent_cards;
    std::unordered_set<size_t> player_selection, opponent_selection;
    std::optional<round_status> cur_round;
    size_t n_rounds, cur_round_idx, player_success, opponent_success;

    BlackJackState(size_t n_rounds_);
    ~BlackJackState();

    void select_card(size_t card_idx) { player_selection.insert(card_idx); }
    void unselect_card(size_t card_idx) { player_selection.erase(card_idx); }
    bool card_selected(size_t card_idx) { return player_selection.contains(card_idx); }

    void toggle_card(size_t card_idx);

    bool finished() { return cur_round_idx >= n_rounds; }
    bool can_submit();
    void submit_round();
    void next_round();
    result_code status();
};

#endif // BLACKJACKSTATE_HPP
