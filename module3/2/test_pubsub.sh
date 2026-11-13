#!/bin/bash
# Тестовый сценарий для задания 2
set -u

PROG="./pubsub"
BROKER_PID=""; SUB1_PID=""; SUB2_PID=""; PUB_PID=""

cleanup() {
    [ -n "$PUB_PID" ]    && kill -INT "$PUB_PID"    2>/dev/null
    [ -n "$SUB1_PID" ]   && kill -INT "$SUB1_PID"   2>/dev/null
    [ -n "$SUB2_PID" ]   && kill -INT "$SUB2_PID"   2>/dev/null
    [ -n "$BROKER_PID" ] && kill -INT "$BROKER_PID" 2>/dev/null
    wait 2>/dev/null
}
trap cleanup EXIT

rm -f sub1.log sub2.log pub.log broker.log

echo "=== 1. Запуск брокера ==="
$PROG -b > broker.log 2>&1 &
BROKER_PID=$!
sleep 0.5

echo "=== 2. Запуск подписчиков ==="
$PROG -s sports           > sub1.log 2>&1 &
SUB1_PID=$!

$PROG -s news sports      > sub2.log 2>&1 &
SUB2_PID=$!
sleep 0.5

echo "=== 3. Запуск издателя ==="
$PROG -p sports > pub.log 2>&1 &
PUB_PID=$!
sleep 3.5

echo "=== 4. Останавливаем издателя ==="
kill -INT "$PUB_PID"; wait "$PUB_PID" 2>/dev/null; PUB_PID=""

echo "=== 5. Останавливаем брокера ==="
kill -INT "$BROKER_PID"; wait "$BROKER_PID" 2>/dev/null; BROKER_PID=""

echo "=== 6. Ждём подписчиков ==="
wait "$SUB1_PID" 2>/dev/null; SUB1_PID=""
wait "$SUB2_PID" 2>/dev/null; SUB2_PID=""

echo
echo "--- Subscriber 1 (sports) ---"
cat sub1.log
echo "--- Subscriber 2 (news+sports) ---"
cat sub2.log

echo
echo "=== 7. Второй брокер должен отказаться ==="
$PROG -b >/dev/null 2>&1 &
BROKER_PID=$!
sleep 0.3
$PROG -b >/dev/null 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo "OK: второй брокер отказался запускаться"
else
    echo "FAIL: второй брокер запустился!"
fi
kill -INT "$BROKER_PID"; wait "$BROKER_PID" 2>/dev/null; BROKER_PID=""

echo
echo "Тестирование завершено."
