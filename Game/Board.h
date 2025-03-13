#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL.h>
#include <SDL_image.h>
#endif

using namespace std;

class Board
{
public:
    Board() = default;
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
        // Конструктор инициализирует доску с шириной W и высотой H. Значения по умолчанию - 0.
    }

    // Инициализирует и отрисовывает начальную игровую доску. Возвращает 0 при успехе, 1 при ошибке
    int start_draw()
    {
        // Инициализация библиотеки SDL2; обработка возможных ошибок.
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("Ошибка инициализации библиотеки SDL2");
            return 1;
        }

        // Определение размеров доски: используется разрешение экрана, если W и H не заданы
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("Ошибка получения параметров экрана");
                return 1; 
            }
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }

        // Создание игрового окна
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("Ошибка создания окна");
            return 1;
        }

        // Создание рендера
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("Ошибка создания рендера");
            return 1;
        }

        // Загрузка текстур игры
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());

        // Проверка успешной загрузки текстур
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("Ошибка загрузки текстур из " + textures_path);
            return 1; 
        }

        // Получение размера вывода рендера
        SDL_GetRendererOutputSize(ren, &W, &H);
        make_start_mtx(); 
        rerender();       
        return 0;         
    }

    // Перерисовывает доску, очищая предыдущие данные игры
    void redraw() {
        game_results = -1; 
        history_mtx.clear(); 
        history_beat_series.clear();
        make_start_mtx();    
        clear_active();      
        clear_highlight();   
    }

    // Перемещает фигуру на доске, обрабатывая взятия
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1)
        {
            // Удаляем взятую фигуру с доски
            mtx[turn.xb][turn.yb] = 0;
        }
        // Делаем ход.
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Основная функция для перемещения фигуры, обрабатывает превращение в ферзя и проверку на ошибки
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        // Проверка корректности хода: клетка назначения должна быть пуста, клетка начала должна содержать фигуру
        if (mtx[i2][j2])
        {
            throw runtime_error("Клетка назначения занята; нельзя переместить фигуру.");
        }
        if (!mtx[i][j])
        {
            throw runtime_error("Клетка начала пуста; нельзя переместить фигуру.");
        }

        // Превращение обычной фигуры в ферзя, если она достигает противоположного края доски
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;

        // Перемещаем фигуру
        mtx[i2][j2] = mtx[i][j];
        drop_piece(i, j);
        add_history(beat_series);
    }

    // Удаляет фигуру с доски в указанных координатах
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender(); 
    }

    // Превращает фигуру в ферзя. Выбрасывает исключение, если превращение некорректно.
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("Некорректная фигура или позиция для превращения в ферзя.");
        }
        mtx[i][j] += 2;
        rerender(); 
    }

    // Возвращает текущее состояние игровой доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Подсвечивает указанные клетки на доске
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender();
    }

    // Очищает все подсвеченные клетки на доске
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender();
    }

    // Устанавливает текущую активную клетку на доске
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();
    }

    // Очищает текущую активную клетку на доске
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();
    }

    // Проверяет, подсвечена ли данная клетка в данный момент.
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Отменяет последний сделанный ход
    void rollback()
    {
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }
        // Восстанавливаем доску до состояния перед последним ходом
        mtx = *(history_mtx.rbegin());
        // Очищаем все подсвеченные или активные клетки
        clear_highlight();
        clear_active();
    }

    // Отображает финальный экран результата игры
    void show_final(const int res)
    {
        game_results = res;
        rerender(); // Перерисовываем доску, чтобы показать финальный результат
    }

    // Сбрасывает размер доски, если изменяется размер окна
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender(); // Новая генерация доски с новыми размерами
    }

    // Завершает игру
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    ~Board()
    {
        if (win)
            quit(); // Очищаем ресурсы SDL
    }

private:
    // Добавляет ход в историю игры
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }

    // Настраивает начальное состояние игровой доски
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2; 
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1; 
            }
        }
        add_history(); // Добавляем начальное состояние доски в историю
    }
    //Заново добавляем всё
    void rerender()
    {
        // Очищаем рендер
        SDL_RenderClear(ren);

        // Добавленяем доску
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Добавленяем игровые фигуры
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j])
                    continue;
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };
                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1)
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen;
                else
                    piece_texture = b_queen;

                
                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // Рисуем подсвеченные клетки
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // Рисуем активную клетку
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0); 
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1);

        // Выводим на экран "Назад" и "Повторить"
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // Выводим на экран результата игры
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path;
            else if (game_results == 2)
                result_path = black_path;
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("Ошибка загрузки изображения результата игры из " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        // Обновляем экран.
        SDL_RenderPresent(ren);
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Записывает ошибки в файл
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Ошибка: " << text << ". " << SDL_GetError() << endl;
        fout.close();
    }

public:
    int W = 0;
    int H = 0;
    // История состояний доски
    vector<vector<vector<POS_T>>> history_mtx;

private:
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    // Текстуры для элементов игры
    SDL_Texture* board = nullptr;
    SDL_Texture* w_piece = nullptr;
    SDL_Texture* b_piece = nullptr;
    SDL_Texture* w_queen = nullptr;
    SDL_Texture* b_queen = nullptr;
    SDL_Texture* back = nullptr;
    SDL_Texture* replay = nullptr;
    // Пути к файлам текстур
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";
    // Координаты активной клетки
    int active_x = -1, active_y = -1;
    // Результат игры, если она окончена)
    int game_results = -1;
    // Матрица, указывающая на подсвеченные клетки
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));
    // Матрица игровой доски (0:пусто, 1:белый, 2:черный, 3:белый ферзь, 4:черный ферзь)
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
    // История серий взятий для каждого хода
    vector<int> history_beat_series;
};