import serial
import csv
import time
import os
#//bài test 1: ngón trỏ phải
#//bài test 2: ngón cái phải
#//bài test 3: ngón giữa phải
#//bài test 4: ngón trỏ trái
# --- Cấu hình ---
SERIAL_PORT = 'COM12'
BAUD_RATE = 115200
OUTPUT_FILE = '20mA.csv'
HEADER = ["Time(ms)", "Red", "IR", "Temp", "Bus_V", "Current_mA"]


# ----------------

def main():
    print(f"--- Đang kết nối tới {SERIAL_PORT} ở tốc độ {BAUD_RATE} ---")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        ser.flushInput()

        print(f"--- Đang bắt đầu ghi dữ liệu vào {OUTPUT_FILE} ---")

        # Kiểm tra file đã tồn tại và có dữ liệu chưa
        file_is_empty = not os.path.exists(OUTPUT_FILE) or os.stat(OUTPUT_FILE).st_size == 0

        with open(OUTPUT_FILE, mode='a', newline='') as f:
            writer = csv.writer(f)

            # 1. Tạo header nếu file mới
            if file_is_empty:
                writer.writerow(HEADER)
                print(f"[INFO]: Đã tạo header mới: {HEADER}")

            while True:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline().decode('utf-8').strip()
                        if not line: continue

                        # 2. Bỏ qua nếu ESP32 gửi lại header (tránh bị lặp dòng tiêu đề trong file)
                        if "Time(ms)" in line:
                            continue

                        print(f"[DATA]: {line}")

                        # Chia nhỏ dữ liệu và ghi
                        parts = line.split(',')
                        if len(parts) >= 2:  # Đảm bảo là dòng dữ liệu hợp lệ
                            writer.writerow(parts)
                            f.flush()

                    except UnicodeDecodeError:
                        continue

    except serial.SerialException as e:
        print(f"Lỗi kết nối Serial: {e}")
    except KeyboardInterrupt:
        print("- -- Đã dừng ghi.File lưu tại: " + os.path.abspath(OUTPUT_FILE))
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
