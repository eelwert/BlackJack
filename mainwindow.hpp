#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPainter>
#include "coro_host.hpp"
#include "blackjackstate.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
    QPainter painter;
    QSize vscreen_size;
    std::unordered_set<int> pressed_keys;
    utils::coro_host cohost;
    std::optional<BlackJackState> state;
    size_t select_cursor = 0;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QRect render_region();
    void render_bg(const std::string& path);
    void render_img(const std::string& path, const QRect& pos);

    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
};

#endif // MAINWINDOW_H
