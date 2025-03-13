#pragma once

enum class Response
{
    //Ход сделан
    OK,
    //Отмена кода
    BACK,
    //Начать игру заново
    REPLAY,
    //Выход из игры
    QUIT,
    //Выбор клетки
    CELL
};
