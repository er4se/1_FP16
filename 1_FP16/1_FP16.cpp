#include <iostream>
#include <cstdint>
#include <bitset>

uint16_t fp16_cast(unsigned int value) {                                            // Извлечение знака, экспоненты и мантиссы
    uint16_t sign = (value >> 31) & 0x1;                                            // Получение бита знака
    uint16_t exponent = 15;                                                         // Получение битов экспоненты
    uint16_t mantissa = 0;                                                          // Получение битов мантиссы
                                                                                    // Извлечение и обработка целой части и дробной части числа value
    uint16_t intPart = (value >> 23) & 0xFF;                                        // Извлечение целой части (8 бит)
    uint16_t fracPart = value & 0x7FFFFF;                                           // Извлечение дробной части (23 бита)

    while (!(intPart & 0x80) && (exponent > 0)) {                                   // Нормализация мантиссы в диапазоне [0.5, 1)
        intPart <<= 1;
        fracPart <<= 1;
        exponent--;
    }
                                                                                    // Смещение экспоненты с учетом bias (в FP16 bias = 15)
    exponent += 15;
                                                                                    // Запись экспоненты и мантиссы в формат FP16
    exponent &= 0x1F;                                                               // Ограничение экспоненты 5 битами
    mantissa = (fracPart >> 13) & 0x3FF;                                            // Оставляем 10 бит для мантиссы

    return (sign << 15) | (exponent << 10) | mantissa;                              // Сборка числа в формате FP16
}

uint16_t fp16_mul2(uint16_t x) {
    if ((x & 0x7C00) != 0x7C00) {                                                   // Проверяем не является ли число NaN или Inf
        if (x & 0x8000) {                                                           // Если число отрицательное
            x -= 0x8000;                                                            // Сбрасываем знак
        }
        if ((x & 0x7C00) != 0x7C00) {                                               // Проверяем, не является ли число NaN или Inf
            uint16_t exp = (x & 0x7C00) >> 10;                                      // Получаем значение экспоненты
            if (exp != 0) {
                exp += 1;                                                           // Увеличиваем экспоненту на 1
                if (exp == 0x1F) {                                                  // Если экспонента равна максимальному значению, получаем Inf
                    return 0x7C00;
                }
                x = (x & 0x8000) | (exp << 10) | (x & 0x03FF);                      // Собираем число с увеличенной экспонентой
            }
            else {                                                                  // Если экспонента равна 0, умножаем только мантиссу
                x <<= 1;
            }
        }
    }
    return x;
}

uint16_t fp16_div2(uint16_t x) {
    if ((x & 0x7C00) != 0x7C00) {                                                   // Проверяем не является ли число NaN или Inf
        uint16_t exp = (x & 0x7C00) >> 10;                                          // Получаем значение экспоненты
        if (exp != 0) {
            exp -= 1;                                                               // Уменьшаем экспоненту на 1
            x = (x & 0x8000) | (exp << 10) | (x & 0x03FF);                          // Собираем число с уменьшенной экспонентой
        }
        else {                                                                      // Если экспонента равна 0, делим только мантиссу
            x >>= 1;
        }
    }
    return x;
}

uint16_t fp16_neg(uint16_t x) {
    return x ^ 0x8000;                                                              // Инвертируем бит знака
}

uint16_t fp16_add(uint16_t x, uint16_t y) {                                         // Извлекаем значения знака, экспоненты и мантиссы из чисел x и y
    uint16_t sign_x = x >> 15;
    uint16_t sign_y = y >> 15;


    uint16_t exp_x = (x >> 10) & 0x1F;
    uint16_t exp_y = (y >> 10) & 0x1F;

    uint16_t mantissa_x = x & 0x3FF;
    uint16_t mantissa_y = y & 0x3FF;

    if (exp_x > exp_y) {                                                            // Выравниваем экспоненты, сдвигая мантиссы на разницу в экспонентах
        mantissa_y >>= (exp_x - exp_y);
        exp_y = exp_x;
    }
    else {
        mantissa_x >>= (exp_y - exp_x);
        exp_x = exp_y;
    }

    uint16_t sum_mantissa =                                                         // Складываем мантиссы с учетом знаков
        (sign_x == sign_y) ? mantissa_x + mantissa_y : mantissa_x - mantissa_y;

    if (sum_mantissa < 0) {                                                         // Обработка переполнения мантиссы
        sum_mantissa = -sum_mantissa;
        sign_x = ~sign_x;
    }
    if (sum_mantissa >= 0x400) {
        sum_mantissa >>= 1;
        exp_x++;
    }

    if (exp_x > 0x1F) {                                                             // Обработка переполнения экспоненты
        return 0x7C00;
    }

    return (sign_x << 15) | (exp_x << 10) | (sum_mantissa & 0x3FF);                 // Собираем результат сложения с учетом знака, экспоненты и мантиссы
}

int fp16_cmp(uint16_t x, uint16_t y) {
    bool is_x_inf = (x & 0x7FFF) == 0x7C00;
    bool is_y_inf = (y & 0x7FFF) == 0x7C00;
    bool is_x_nan = (x & 0x7FFF) > 0x7C00;
    bool is_y_nan = (y & 0x7FFF) > 0x7C00;

    if (is_x_nan || is_y_nan) return -2;                                            //Если хотя бы одно из чисел NaN, результат неопределен (return -2 это условно показывает)
    else if (is_x_inf && !is_y_inf) {                                               //Если число одно из чисел Inf, то второе соответственно будет меньше
        return 1;
    }
    else if (is_x_inf && is_y_inf) {                                                //Если оба числа Inf, то считаем, что они равны
        return 0;
    }
    else if (!is_x_inf && is_y_inf) {
        return -1;
    }

    if (x < y) {
        return -1;
    }
    else if (x == y) {
        return 0;
    }
    else {
        return 1;
    }
}

int main()
{
    setlocale(LC_ALL, "");
    uint16_t num1 = 0b0100001010000000; // = 6.5
    uint16_t num2 = 0b0011111000000000; // = 0.75

    uint16_t result_add = fp16_add(num1, num2);
    uint16_t result_mul2 = fp16_mul2(num2);
    uint16_t result_div2 = fp16_mul2(num1);
    uint16_t result_neg = fp16_neg(num2);
    int result_cmp = fp16_cmp(num1, num2);

    std::cout << "Сложение: " << std::bitset<16>(result_add) << std::endl;
    std::cout << "Умножение на 2: " << std::bitset<16>(result_mul2) << std::endl;
    std::cout << "Деление на 2: " << std::bitset<16>(result_div2) << std::endl;
    std::cout << "Смена знака: " << std::bitset<16>(result_neg) << std::endl;
    std::cout << "Сравнение: " << result_cmp << std::endl;

    unsigned int num3 = 6;
    uint16_t result_cast = fp16_cast(num3);

    std::cout << "Cast: " << std::bitset<16>(result_cast) << std::endl;
}
