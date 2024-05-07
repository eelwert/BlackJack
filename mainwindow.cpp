#include <QKeyEvent>
#include <fmt/core.h>
#include <coutils.hpp>
#include "mainwindow.hpp"

static std::unordered_map<QString, QImage> loaded_images;

static QImage& load_image_cached(const QString& path) {
    if (auto img_it = loaded_images.find(path); img_it != loaded_images.end()) {
        return img_it->second;
    } else {
        auto [new_it, inserted] = loaded_images.emplace(std::piecewise_construct,
            std::forward_as_tuple(path),
            std::forward_as_tuple(path)
        );
        return new_it->second;
    }
}

static QRect rect_relative(const QRect& rect, const QSize& parent, const QRect& target) {
    int x = double(rect.x()) / double(parent.width()) * target.width();
    int y = double(rect.y()) / double(parent.height()) * target.height();
    int w = double(rect.width()) / double(parent.width()) * target.width();
    int h = double(rect.height()) / double(parent.height()) * target.height();
    return {target.x() + x, target.y() + y, w, h};
}

MainWindow::MainWindow(QWidget *parent):
QMainWindow(parent), vscreen_size(1024, 768) {
    this->resize(1024, 768);
    coutils::sync::unleash_lambda([this]() -> coutils::async_fn<void> {
        auto wait_key = [&](Qt::Key key) -> coutils::async_fn<void> {
            while (true) {
                co_await cohost.wait_event({cohost.EVT_KEY_CHANGE});
                if (pressed_keys.contains(key)) { break; }
            }
        };
        co_await wait_key(Qt::Key_Z);
        state.emplace(3); // 初始化一轮三局21点
        while (true) {
            while (true) {
                co_await cohost.wait_event({cohost.EVT_KEY_CHANGE});
                if (pressed_keys.contains(Qt::Key_Z)) {
                    if (select_cursor == state->player_cards.size() && state->can_submit()) {
                        state->submit_round(); select_cursor = 0; break;
                    } else {
                        state->toggle_card(select_cursor);
                    }
                } else if (pressed_keys.contains(Qt::Key_Left)) {
                    if (select_cursor != 0) { --select_cursor; }
                } else if (pressed_keys.contains(Qt::Key_Right)) {
                    if (select_cursor < state->player_cards.size()) { ++select_cursor; }
                }
            }
            co_await wait_key(Qt::Key_Z);
            state->next_round();
            if (state->finished()) {
                co_await wait_key(Qt::Key_Z);
                state.reset(); break;
            }
        }
    });
}

MainWindow::~MainWindow() {}

QRect MainWindow::render_region() {
    int w = height() * vscreen_size.width() / vscreen_size.height();
    int h = width() * vscreen_size.height() / vscreen_size.width();
    w = std::min(w, width()); h = std::min(h, height());
    QRect region; QPoint center(width() / 2, height() / 2);
    region.setRect(0, 0, w, h); region.moveCenter(center);
    return region;
}

void MainWindow::render_bg(const std::string& path) {
    auto& img = load_image_cached(QString::fromStdString(path));
    painter.drawImage(render_region(), img);
}

void MainWindow::render_img(const std::string& path, const QRect& pos) {
    auto& img = load_image_cached(QString::fromStdString(path));
    auto target = rect_relative(pos, vscreen_size, render_region());
    painter.drawImage(target, img);
}

void MainWindow::paintEvent(QPaintEvent* event) {
    painter.begin(this);
    render_bg("://res/black_jack_table.png");
    if (state) {
        auto render_poker = [&](size_t card_id, const QRect& pos) {
            render_img(fmt::format("://res/pokers/{}.png", card_id), pos);
        };
        auto render_number = [&](ssize_t n, const QRect& pos) {
            auto ns = std::to_string(n);
            auto rpos = pos;
            for (auto&& nc : ns) {
                render_img(fmt::format("://res/power_numbers/{}.png", nc), rpos);
                rpos.translate(pos.width(), 0);
            }
        };

        QRect my_poker_start_pos = {80, 540, 132, 182};
        for (size_t i = 0; i < state->player_cards.size(); ++i) {
            auto poker_pos = my_poker_start_pos.translated(80 * i, 0);
            if (!state->cur_round) {
                if (i == select_cursor) { poker_pos.translate(0, -40); }
                if (state->player_selection.contains(i)) { poker_pos.translate(0, -40); }
            } else {
                if (state->player_selection.contains(i)) { poker_pos.translate(0, -80); }
            }
            render_poker(state->player_cards[i], poker_pos);
        }
        bool btn_selected = select_cursor == state->player_cards.size();
        render_img(btn_selected ? "://res/go_button_focus.png" : "://res/go_button_idle.png", {886, 598, 106, 64});

        render_number(state->opponent_success, {640, 64, 48, 96});
        render_number(state->player_success, {900, 480, 48, 96});

        if (state->cur_round) {
            QRect his_poker_start_pos = {336, 240, 132, 182};
            for (size_t i = 0; auto&& idx : state->opponent_selection) {
                auto poker_pos = his_poker_start_pos.translated(160 * i, 0);
                render_poker(state->opponent_cards[idx], poker_pos); ++i;
            }

            render_number(state->cur_round->opponent_power, {64, 256, 24, 48});
            render_number(state->cur_round->player_power, {64, 360, 24, 48});
        }

        if (state->finished()) {
            using enum BlackJackState::result_code;
            auto result = state->status();
            if (result == LOSE || result == TIE) { render_img("://res/trophy.png", {288, 32, 128, 128}); }
            if (result == WIN || result == TIE) { render_img("://res/trophy.png", {32, 512, 128, 128}); }
        }
    }
    painter.end();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (!event->isAutoRepeat()) {
        pressed_keys.insert(event->key());
        cohost.on_key_change();
    }
    cohost.on_key_signal();
    update();
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
    pressed_keys.erase(event->key());
    cohost.on_key_change();
    cohost.on_key_signal();
    update();
}
