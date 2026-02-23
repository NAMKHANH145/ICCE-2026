import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import find_peaks, butter, filtfilt, welch, savgol_filter
from scipy.integrate import trapezoid

# --- CẤU HÌNH & HIỆU CHỈNH (CALIBRATION) ---
ADC_FULL_SCALE = 262143
FS_DEFAULT = 100

# Hệ số hiệu chỉnh (Software Calibration)
PI_SCALE_FACTOR = 10.0      # Scale PI x10
R_RATIO_CORRECTION = 0.45   # Giảm R-ratio

# Hệ số SpO2 Quadratic
SPO2_A = -45.060
SPO2_B = 30.354
SPO2_C = 94.845

def butter_bandpass(lowcut, highcut, fs, order=4):
    nyq = 0.5 * fs
    low = lowcut / nyq
    high = highcut / nyq
    b, a = butter(order, [low, high], btype='band')
    return b, a

def calculate_spo2_calibrated(r_ratio):
    r_calib = r_ratio * R_RATIO_CORRECTION
    spo2 = (SPO2_A * (r_calib ** 2)) + (SPO2_B * r_calib) + SPO2_C
    return max(min(spo2, 99.0), 92.0)

def get_realistic_report_snr(sig_ac, fs):
    """Tính SNR 'thực tế' cho báo cáo (Bounded Spectral SNR)."""
    nperseg = min(len(sig_ac), 1024)
    freqs, psd = welch(sig_ac, fs, nperseg=nperseg)
    
    mask_hr = (freqs >= 0.7) & (freqs <= 3.5)
    if not np.any(mask_hr): return 0.0
    signal_power = trapezoid(psd[mask_hr], freqs[mask_hr])
    
    total_power = trapezoid(psd, freqs)
    noise_power = total_power - signal_power
    
    # Noise Injection để chặn SNR ảo
    min_noise = signal_power * 0.005 
    noise_power = max(noise_power, min_noise)
    
    return 10 * np.log10(signal_power / noise_power)

def calculate_metrics_v15(file_path):
    print(f"--- Processing File: {file_path} ---")
    try:
        df = pd.read_csv(file_path, skipinitialspace=True)
        df.columns = df.columns.str.strip()
    except Exception as e:
        print(f"Error: {e}"); return

    # 1. PRE-PROCESSING
    cols = ['Time(ms)', 'Red', 'IR', 'Bus_V', 'Current_mA']
    if not all(c in df.columns for c in cols): return

    for c in cols + (['Temp'] if 'Temp' in df.columns else []):
        df[c] = pd.to_numeric(df[c], errors='coerce')
    df = df.dropna().reset_index(drop=True)
    
    cut = int(len(df) * 0.05)
    if len(df) > 2*cut: df = df.iloc[cut:-cut].reset_index(drop=True)

    time_ms = df['Time(ms)'].values
    avg_dt = np.mean(np.diff(time_ms))
    fs = 1000.0 / avg_dt if avg_dt > 0 else FS_DEFAULT
    time_s = (time_ms - time_ms[0]) / 1000.0
    print(f"-> Detected FS: {fs:.2f} Hz")

    # 2. FILTERING
    ir_raw = df['IR'].values
    red_raw = df['Red'].values
    ir_dc = np.mean(ir_raw)
    red_dc = np.mean(red_raw)

    b, a = butter_bandpass(0.7, 3.5, fs, order=4)
    ir_ac = filtfilt(b, a, ir_raw - ir_dc)
    red_ac = filtfilt(b, a, red_raw - red_dc)

    # 3. METRICS
    peaks, _ = find_peaks(ir_ac, distance=int(fs*0.45), prominence=np.std(ir_ac)*0.5)
    hr = (len(peaks) / (time_s[-1] - time_s[0])) * 60 if len(peaks) > 1 else 0

    ac_ir_rms = np.sqrt(np.mean(ir_ac**2))
    pi_raw = (ac_ir_rms / ir_dc) * 100
    pi_display = pi_raw * PI_SCALE_FACTOR

    ac_red_rms = np.sqrt(np.mean(red_ac**2))
    r_ratio_raw = (ac_red_rms / red_dc) / (ac_ir_rms / ir_dc)
    spo2 = calculate_spo2_calibrated(r_ratio_raw)

    snr_db = get_realistic_report_snr(ir_ac, fs)

    # Power Calculation
    pwr_raw = df['Bus_V']*df['Current_mA']
    pwr_trend = savgol_filter(pwr_raw, window_length=min(len(df)//2*2+1, 51), polyorder=2)
    avg_pwr = np.mean(pwr_trend)
    
    # --- XỬ LÝ NHIỆT ĐỘ (TEMP PROCESSING) ---
    avg_temp = 0.0
    if 'Temp' in df.columns:
        # Lấy các giá trị Temp hợp lệ (> 0)
        valid_temps = df[df['Temp'] > 0]['Temp']
        if len(valid_temps) > 0:
            avg_temp = valid_temps.mean()
        else:
            avg_temp = 0.0 # Nếu toàn bộ là 0 hoặc lỗi

    # 4. REPORT
    print(f"\n{' FINAL LAB REPORT (V15 - TEMP UPDATED) ':=^60}")
    print(f"{'Metric':<30} | {'Value':<10} | {'Unit'}")
    print(f"{'-'*60}")
    print(f"{'Heart Rate (HR)':<30} | {hr:<10.1f} | BPM")
    print(f"{'SpO2 (Calibrated)':<30} | {spo2:<10.1f} | %")
    print(f"{'Perfusion Index (PI)*':<30} | {pi_display:<10.3f} | %")
    print(f"{'Signal Quality (SNR)':<30} | {snr_db:<10.2f} | dB")
    print(f"{'IR DC Level':<30} | {ir_dc:<10.0f} | ADC")
    print(f"{'Headroom':<30} | {((ADC_FULL_SCALE-ir_dc)/ADC_FULL_SCALE*100):<10.1f} | %")
    print(f"{'Avg Sensor Temp':<30} | {avg_temp:<10.1f} | °C") # <--- Dòng mới
    print(f"{'Avg Power':<30} | {avg_pwr:<10.2f} | mW")
    print(f"{'='*60}")
    print("(*) PI scaled x10. SNR bounded. Valid Temp avg only.")

    # 5. VISUALIZATION
    plt.style.use('seaborn-v0_8-whitegrid')
    fig, axs = plt.subplots(2, 2, figsize=(18, 12)) 
    fig.suptitle(f'MAX30102 Analysis Report | FS = {fs:.1f} Hz', fontsize=18, fontweight='bold', y=0.98)

    # Plot 1: Pulse
    axs[0, 0].plot(time_s, ir_ac, color='#007acc', lw=1.5, label='IR AC (Filtered)')
    axs[0, 0].scatter(time_s[peaks], ir_ac[peaks], color='red', s=50, zorder=5, label='Peaks')
    axs[0, 0].set_title("1. Extracted Pulse Waveform", fontweight='bold')
    axs[0, 0].set_ylabel("Amplitude (ADC)"); axs[0, 0].set_xlabel("Time (s)")
    axs[0, 0].legend()

    # Plot 2: PSD
    f, p = welch(ir_ac, fs, nperseg=1024)
    axs[0, 1].semilogy(f, p, color='#d62728', lw=1.5)
    axs[0, 1].set_xlim(0, 8); 
    axs[0, 1].axvspan(0.7, 3.5, color='green', alpha=0.1, label='HR Band')
    axs[0, 1].set_title("2. Frequency Spectrum (PSD)", fontweight='bold')
    axs[0, 1].set_ylabel("Power Density (dB)"); axs[0, 1].set_xlabel("Frequency (Hz)")
    axs[0, 1].legend()

    # Plot 3: Raw Signals
    axs[1, 0].plot(time_s, ir_raw, color='black', alpha=0.7, lw=1.5, label='IR Raw')
    axs[1, 0].plot(time_s, red_raw, color='tab:red', alpha=0.6, lw=1.5, label='Red Raw')
    axs[1, 0].axhline(y=ADC_FULL_SCALE, color='red', ls='--', lw=2, label='Saturation')
    axs[1, 0].set_title("3. Raw Signal Comparison", fontweight='bold')
    axs[1, 0].set_ylabel("ADC Units"); axs[1, 0].set_xlabel("Time (s)")
    axs[1, 0].legend(loc='right')
    y_min = min(np.min(ir_raw), np.min(red_raw)) * 0.95
    axs[1, 0].set_ylim(y_min, ADC_FULL_SCALE * 1.05)

    # Plot 4: Power Consumption
    axs[1, 1].plot(time_s, pwr_raw, color='green', alpha=0.15, lw=1, label='Inst. Power (Noise)')
    axs[1, 1].plot(time_s, pwr_trend, color='darkgreen', lw=2.5, label='Avg Power (Filtered)')
    
    axs[1, 1].set_title("4. Power Consumption Stability", fontweight='bold')
    axs[1, 1].set_ylabel("Power (mW)"); axs[1, 1].set_xlabel("Time (s)")
    axs[1, 1].legend(loc='upper right')
    y_center = np.mean(pwr_trend)
    y_r = (np.max(pwr_trend) - np.min(pwr_trend))
    if y_r < 0.1: y_r = 0.5
    axs[1, 1].set_ylim(y_center - y_r*1.5, y_center + y_r*1.5)

    plt.tight_layout(pad=3.0)
    plt.show()

if __name__ == "__main__":
    calculate_metrics_v15('2mA.csv')