#!/bin/bash
echo "Đang khởi động Camera..."
ros2 run pkg camera_node &
CAMERA_PID=$!

echo "Đang khởi động ESP32..."
ros2 run pkg esp32_node &
ESP32_PID=$!

# Bắt tín hiệu Ctrl+C để tắt luôn các node chạy ngầm
trap "echo 'Đang tắt các node...'; kill $CAMERA_PID $ESP32_PID; exit" INT TERM EXIT

echo "Đang khởi động điều khiển bằng bàn phím..."
# Chạy ở foreground để nhận phím
ros2 run pkg keyboard_control_node
