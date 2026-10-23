#!/bin/bash

# 1. Парсим аргумент --pishitesuda=
for arg in "$@"; do
    case $arg in
        --pishitesuda=*)
        PIPE_PATH="${arg#*=}"
        ;;
    esac
done

if [ -z "$PIPE_PATH" ]; then
    echo "Ошибка: не указан путь к каналу (--pishitesuda=)"
    exit 1
fi

LOG_FILE="cuckoo.log"

# 2. Создаем именованный канал (FIFO)
mkfifo "$PIPE_PATH" 2>/dev/null

# 3. Записываем Startup
echo "$(date '+%Y-%m-%d %H:%M:%S') Startup!" >> "$LOG_FILE"

# 4. Перехватываем сигнал SIGTERM для корректного завершения
trap 'echo "$(date "+%Y-%m-%d %H:%M:%S") Shutdown!" >> "$LOG_FILE"; rm -f "$PIPE_PATH"; exit 0' SIGTERM

# 5. Бесконечный цикл чтения из канала
while true; do
    if read -r line < "$PIPE_PATH"; then
        # Проверяем, что формат сообщения верный: NAME[PID]: how much time do I have?
        if [[ "$line" =~ ^(.*)\[([0-9]+)\]:\ how\ much\ time\ do\ I\ have\?$ ]]; then
            NAME="${BASH_REMATCH[1]}"
            PID="${BASH_REMATCH[2]}"

            # Генерируем N от 2 до 10
            N=$((RANDOM % 9 + 2))

            # Логируем
            echo "$(date '+%Y-%m-%d %H:%M:%S') $NAME[$PID] $N" >> "$LOG_FILE"

            # Отправляем ответ обратно в канал
            echo "$N" > "$PIPE_PATH"
        fi
    fi
done
