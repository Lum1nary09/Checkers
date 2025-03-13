#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        // Генератор случайных чисел
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color) {
        // Очищаем список следующих ходов
        next_move.clear();
        // Очищаем список следующих лучших состояний
        next_best_state.clear();

        // Осуществляем поиск наилучшего первого хода
        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        // Начальное состояние
        int state = 0;

        // Результат (список наилучших ходов)
        vector<move_pos> res;

        do
        {
            // Добавляем следующий ход из текущего состояния
            res.push_back(next_move[state]);
            // Переходим в следующее состояние
            state = next_best_state[state];
            // Заканчиваем, если текущее состояние или состояние после серии ходов побития становится -1
        } while (state != -1 && next_move[state].x != -1);

        return res;
    }

private:
    // Выполнение хода
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }
    // Оценка доски
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF;
        if (b + bq == 0)
            return 0;
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        // Отношение суммарных значений фигур
        return (b + bq * q_coef) / (w + wq * q_coef);
    }
    // Поиск первого лучшего хода
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
                                double alpha = -1){
        // Добавляем пустой ход
         next_move.emplace_back(-1, -1, -1, -1);
 
         // Начальное состояние
         next_best_state.push_back(-1);
 
         // Проверяем состояние (для определения наличия списка ходов)
         // Если state == 0, значит до этого были побиты фигуры противника и искать ходы не нужно
         if (state != 0) {
             // Осуществляем поиск ходов
             find_turns(x, y, mtx);
         }
 
         // Копируем список доступных ходов
         const auto now_turns = turns;
 
         // Копируем значения флага
         const auto now_have_beats = have_beats;
 
         // Если мы можем побить фигуру противника
         if (!now_have_beats && state != 0) {
             // Возвращаем наилучший ход
             return find_best_turns_rec(mtx, 1 - color, 0, alpha);
         }
 
         // Наилучший результат на текущий момент
         double best_score = -1;
 
         // Перебираем доступные ходы
         for (const auto turn : now_turns) {
             // Новое состояние
             const size_t new_state = next_move.size();
 
             // Полученный результат
             double score;
 
             // Если есть кого бить
             if (now_have_beats) {
                 score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, new_state, best_score);
             // Если никого не бьем
             } else {
                 score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
             }
 
             // Если полученный результат превосходит наилучший
             if (score > best_score) {
                 // Обновляем наилучший результат
                 best_score = score;
                 next_move[state] = turn;
                 next_best_state[state] = (now_have_beats ? int(new_state) : -1);
             }
         }
 
         return best_score;
     }
                                }
    // Рекурсивный поиск хода
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
                                double beta = INF + 1, const POS_T x = -1, const POS_T y = -1) {
            // Условие выхода из рекурсии
            if (depth == Max_depth) {
                // Возвращаем наилучший результат
                return calc_score(mtx, (depth % 2 == color));
            }
    
            // Если есть серия побитий
            if (x != -1) {
                // Ищем ходы от координаты
                find_turns(x, y, mtx);
            }
            else {
                // Ищем ходы для выбранного цвета игрока
                find_turns(color, mtx);
            }
    
            // Копируем список доступных ходов
            const auto now_turns = turns;
    
            // Копируем значения флага
            const bool now_have_beats = have_beats;
    
            // Если нет фигур для побития и была серия побитий
            if (!now_have_beats && x != -1) {
                // Возвращаем наилучший ход
                return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
            }
    
            // Проверка наличия ходов
            if (turns.empty()) {
                // Возвращаем результат игры
                return (depth % 2 ? 0 : INF);
            }
    
            // Минимальный результат
            double min_score = INF + 1;
            // Максимальный результат
            double max_score = -1;
    
            // Перебираем доступные ходы
            for (const auto turn : now_turns) {
                // Текущий результат
                double score = 0.0;
    
                // Если есть серия побитий
                if (now_have_beats) {
                    // Продолжаем серию побитий
                    score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
                }
                else {
                    score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
                }
    
                // Обновляем минимум
                min_score = min(min_score, score);
                // Обновляем Максимум
                max_score = max(max_score, score);
    
                // Альфа-бета отсечение
                // Если ход игрока
                if (depth % 2) {
                    // Двигаем левую границу
                    alpha = max(alpha, max_score);
                // Ход противника
                } else {
                    // Двигаем правую границу
                    beta = min(beta, min_score);
                }
    
                // Оптимизация
                if (optimization != "O0" && alpha > beta) {
                    break;
                }
                if (optimization == "O2" && alpha == beta) {;
                    return (depth % 2 ? max_score + 1 : min_score - 1);
                }
            }
    
            // Возвращаем результат ходившего игрока
            return (depth % 2 ? max_score : min_score);
        }

public:
// Поиск ходов для цвета
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }
    // Поиск ходов для клетки
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Поиск ходов для всех фигур цвета
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }
    // Поиск ходов для выбранной клетки
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns;
    bool have_beats;
    int Max_depth;

  private:
    default_random_engine rand_eng;
    string scoring_mode;
    string optimization;
    vector<move_pos> next_move;
    vector<int> next_best_state;
    Board *board;
    Config *config;
};
