#!/bin/bash
# Скрипт для тестирования программы filecopy

PROG="./filecopy"
TEST_DIR="test_env"
PIPE_NAME="myfifo"

# Создаём тестовое окружение
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR" || exit 1

# Создаём тестовые файлы
echo "Содержимое файла A" > fileA.txt
echo "Содержимое файла B, немного длиннее." > fileB.txt
dd if=/dev/urandom of=fileC.bin bs=1024 count=10 2>/dev/null

echo "=== Тест 1: Безымянный канал, два файла ==="
../"$PROG" fileA.txt fileB.txt
if diff fileA.txt fileA.txt.copy >/dev/null && diff fileB.txt fileB.txt.copy >/dev/null; then
    echo "OK: копии совпадают"
else
    echo "FAIL: копии не совпадают"
fi

echo -e "\n=== Тест 2: Именованный канал (-p) ==="
../"$PROG" -p "$PIPE_NAME" fileC.bin
if cmp -s fileC.bin fileC.bin.copy; then
    echo "OK: бинарная копия совпадает"
else
    echo "FAIL: бинарная копия повреждена"
fi

echo -e "\n=== Тест 3: Несуществующий файл ==="
../"$PROG" no_such_file.txt 2> err.log
if [ -s err.log ]; then
    echo "OK: выведено диагностическое сообщение"
    cat err.log
else
    echo "FAIL: сообщение об ошибке отсутствует"
fi

echo -e "\n=== Тест 4: Нет аргументов ==="
../"$PROG" 2> err2.log
if [ -s err2.log ]; then
    echo "OK: выведена ошибка при отсутствии файлов"
else
    echo "FAIL: ошибка не выведена"
fi

echo -e "\n=== Тест 5: Некорректный ключ ==="
../"$PROG" -x fileA.txt 2> err3.log
if [ -s err3.log ]; then
    echo "OK: неизвестный ключ обработан"
else
    echo "FAIL: неизвестный ключ проигнорирован"
fi

# Очистка
cd ..
rm -rf "$TEST_DIR"
echo -e "\nТестирование завершено."
