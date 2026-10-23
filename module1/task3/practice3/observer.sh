#!/bin/bash

CONF_FILE="observer.conf"
LOG_FILE="observer.log"
BASE_DIR="/root/practice3"

if [ ! -f "$CONF_FILE" ]; then
    echo "Файл конфигурации $CONF_FILE не найден!"
    exit 1
fi

while IFS= read -r script || [ -n "$script" ]; do
    [[ -z "$script" || "$script" == \#* ]] && continue
    IS_RUNNING=0
    for pid in /proc/[0-9]*; do
        if [ -f "$pid/cmdline" ]; then
            CMD=$(tr '\0' ' ' < "$pid/cmdline" 2>/dev/null)
            if [[ "$CMD" == *"$script"* ]]; then
                IS_RUNNING=1
                break
            fi
        fi
    done

    # Если не запущен - запускаем через nohup
    if [ $IS_RUNNING -eq 0 ]; then
        nohup "$script" > /dev/null 2>&1 &
        echo "$(date '+%Y-%m-%d %H:%M:%S') Перезапущен: $script" >> "$LOG_FILE"
    fi
done < "$CONF_FILE"
