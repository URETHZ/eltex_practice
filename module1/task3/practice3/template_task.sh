#!/bin/bash

SCRIPT_NAME=$(basename "$0")
PIPE_PATH="/tmp/cuckoo.pipe"

if [ "$SCRIPT_NAME" = "template_task.sh" ]; then
    echo "я бригадир, сам не работаю"
    exit 0
fi

LOG_FILE="report_${SCRIPT_NAME}.log"

echo "$(date '+%Y-%m-%d %H:%M:%S') [$$] Скрипт запущен" >> "$LOG_FILE"
echo "${SCRIPT_NAME}[$$]: how much time do I have?" > "$PIPE_PATH"

read -r N < "$PIPE_PATH"

sleep "$N"

echo "$(date '+%Y-%m-%d %H:%M:%S') [$$] Скрипт завершился, работал $N секунд." >> "$LOG_FILE"
