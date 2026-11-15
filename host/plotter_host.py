"""Speech Plotter host controller.

Records a phrase, transcribes it with Google Speech-to-Text, and sends the
uppercased text to the plotter over serial. Also exposes manual text entry, a
motor-test submenu, and a confirmed emergency stop.

Environment:
    GOOGLE_APPLICATION_CREDENTIALS   path to a service account JSON with the
                                    Speech-to-Text API enabled
    PLOTTER_PORT                    serial port, e.g. COM5 or /dev/ttyUSB0
"""

import os
import threading
import time
from datetime import datetime

import serial
import sounddevice as sd
from google.cloud import speech

# ==================== CONFIGURATION ====================
if not os.environ.get("GOOGLE_APPLICATION_CREDENTIALS"):
    raise SystemExit(
        "Set GOOGLE_APPLICATION_CREDENTIALS to a service account JSON file "
        "with the Speech-to-Text API enabled."
    )

SAMPLE_RATE = 16000  # Hz

# For a real Arduino, use its port (COM3, /dev/ttyUSB0, /dev/cu.usbmodem...).
# For a virtual COM pair, use the paired port (emulator on COM4, set COM3 here).
ARDUINO_PORT = os.environ.get("PLOTTER_PORT", "COM5")
BAUD_RATE = 9600
# =======================================================

# Create Speech client once
client = speech.SpeechClient()

# Global flag for monitoring thread
monitoring_active = True

# Open serial connection once
try:
    ser = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=0.1)
    print(f"Connected to Arduino on {ARDUINO_PORT}")
    print("(Works with real Arduino or virtual COM port emulator)")
    print("Arduino output will appear below...\n")
    time.sleep(2)  # Give Arduino time to reset
except Exception as e:
    print(f"Error connecting to Arduino on {ARDUINO_PORT}: {e}")
    print("\nTroubleshooting:")
    print("- Check if Arduino is connected and port is correct")
    print("- For virtual COM: ensure com0com or similar is installed")
    print("- For virtual COM: ensure emulator is running on paired port")
    exit(1)


def monitor_arduino():
    """Background thread to monitor Arduino output"""
    global monitoring_active

    while monitoring_active:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    timestamp = datetime.now().strftime("%H:%M:%S")

                    # Color-code messages
                    if "ERROR" in line.upper() or "FAIL" in line.upper():
                        prefix = "[ERROR]"
                    elif "COMPLETE" in line.upper() or "DONE" in line.upper():
                        prefix = "[DONE]"
                    elif "Processing" in line or "RECEIVED" in line.upper():
                        prefix = "[CMD]"
                    elif "Lifting" in line or "Lowering" in line or "home" in line.lower():
                        prefix = "[PEN]"
                    elif "Ready" in line.lower():
                        prefix = "[READY]"
                    else:
                        prefix = "[INFO]"

                    print(f"[{timestamp}] {prefix} {line}")
        except Exception:
            pass

        time.sleep(0.01)


# Start background monitoring thread
monitor_thread = threading.Thread(target=monitor_arduino, daemon=True)
monitor_thread.start()
print("✓ Arduino monitor started\n")


def show_menu():
    print("\n" + "=" * 50)
    print("SPEECH-TO-TEXT PLOTTER CONTROLLER")
    print("=" * 50)
    print("1. Record & Transcribe Speech")
    print("2. Manual Text Entry (for testing)")
    print("3. Motor Test Commands")
    print("4. EMERGENCY STOP (halt Arduino)")
    print("5. Quit")
    print("=" * 50)


def show_test_menu():
    print("\n" + "=" * 50)
    print("MOTOR TEST COMMANDS")
    print("=" * 50)
    print("Pen Lift (Z-axis):")
    print("  UP    - Lift pen up")
    print("  DOWN  - Lower pen down")
    print("\nHorizontal Movement (X-axis - Belt):")
    print("  LEFT  - Move pen left 20mm")
    print("  RIGHT - Move pen right 20mm")
    print("\nVertical Movement (Y-axis - Platform):")
    print("  FORWARD  - Move platform forward 20mm")
    print("  BACK     - Move platform back 20mm")
    print("\nOther:")
    print("  HOME  - Return to home position")
    print("  0     - Return to main menu")
    print("=" * 50)


def record_audio(duration):
    """Record audio from the microphone for a given duration."""
    print(f"\nRecording for {duration} seconds... Speak now!")
    audio_data = sd.rec(int(duration * SAMPLE_RATE), samplerate=SAMPLE_RATE, channels=1, dtype='int16')
    sd.wait()
    print("Recording complete.")
    return audio_data


def transcribe(audio_data):
    """Send recorded audio to Google Speech API and return transcript."""
    print("Transcribing...")
    audio_content = audio_data.flatten().tobytes()

    audio = speech.RecognitionAudio(content=audio_content)
    config = speech.RecognitionConfig(
        encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16,
        sample_rate_hertz=SAMPLE_RATE,
        language_code="en-US"
    )

    try:
        response = client.recognize(config=config, audio=audio)
        if response.results:
            transcript = response.results[0].alternatives[0].transcript.upper().strip()
            print(f"\nRecognized: '{transcript}'")
            return transcript
        else:
            print("No speech detected.")
            return None
    except Exception as e:
        print(f"Transcription error: {e}")
        return None


def send_to_arduino(text):
    """Send transcript to Arduino without waiting for response."""
    if not text:
        print("[DEBUG] send_to_arduino called with empty/None text")
        print("No text to send.")
        return

    print(f"\n{'=' * 50}")
    print(f"Sending to Arduino: {text}")
    print(f"Port: {ARDUINO_PORT}")
    print(f"Message bytes: {(text + '\\n').encode('utf-8')}")
    print(f"{'=' * 50}")

    try:
        print(f"[DEBUG] Resetting input buffer...")
        ser.reset_input_buffer()

        print(f"[DEBUG] Writing to serial port...")
        bytes_written = ser.write((text + '\n').encode('utf-8'))
        print(f"✓ Wrote {bytes_written} bytes to serial port")

        print(f"[DEBUG] Flushing output buffer...")
        ser.flush()
        print(f"✓ Flushed output buffer")

        print(f"\n✓ Text sent to Arduino! Watch above for Arduino responses.")
        print(f"Ready for next command.\n")

    except Exception as e:
        print(f"[ERROR] Failed to send to Arduino: {e}")
        import traceback
        traceback.print_exc()


# ============================================================

try:
    while True:
        show_menu()
        choice = input("\nEnter your choice (1-5): ").strip()

        if choice == '1':
            try:
                duration = float(input("How many seconds to record? "))
                print(f"\n[DEBUG] Starting recording for {duration} seconds...")
                audio_data = record_audio(duration)
                print(f"[DEBUG] Recording complete, audio data shape: {audio_data.shape}")

                print(f"[DEBUG] Starting transcription...")
                transcript = transcribe(audio_data)

                if transcript:
                    print(f"[DEBUG] Transcription successful: '{transcript}'")
                    send_to_arduino(transcript)
                else:
                    print(f"[DEBUG] Transcription returned None")
                    print("WARNING: No text to send - either no speech detected or transcription failed")
                    print("Try speaking louder or closer to the microphone\n")

            except ValueError:
                print("Please enter a valid number for seconds.")
            except Exception as e:
                print(f"[ERROR] Unexpected error: {e}")
                import traceback

                traceback.print_exc()

        elif choice == '2':
            # Manual text entry for testing
            text = input("Enter text to send to Arduino: ").strip().upper()
            if text:
                print(f"[DEBUG] Manual text entry: '{text}'")
                send_to_arduino(text)
            else:
                print("No text entered.")

        elif choice == '3':
            # Motor test submenu
            while True:
                show_test_menu()
                test_cmd = input("\nEnter test command (or 0 to return): ").strip().upper()

                if test_cmd == '0':
                    break
                elif test_cmd in ['UP', 'DOWN', 'LEFT', 'RIGHT', 'FORWARD', 'BACK', 'HOME']:
                    print(f"Sending test command: {test_cmd}")
                    send_to_arduino(test_cmd)
                else:
                    print("Invalid command! Use: UP, DOWN, LEFT, RIGHT, FORWARD, BACK, HOME, or 0")

        elif choice == '4':
            # Emergency stop
            print("\n" + "=" * 50)
            print("WARNING: EMERGENCY STOP")
            print("=" * 50)
            confirm = input("Send STOP command to Arduino? (yes/no): ").strip().lower()
            if confirm == 'yes':
                print("Sending STOP command...")
                send_to_arduino("STOP")
                print("\nSTOP: Arduino should now be HALTED!")
                print("   Press Arduino reset button to restart.\n")
            else:
                print("Emergency stop cancelled.")

        elif choice == '5':
            print("\nClosing program...")
            monitoring_active = False
            time.sleep(0.5)
            ser.close()
            print("Connection closed.")
            break
        else:
            print("Invalid choice! Please enter 1-5.")

except KeyboardInterrupt:
    print("\n\nInterrupted by user.")
    monitoring_active = False
    time.sleep(0.5)  # Give monitor thread time to stop
    ser.close()
    print("Connection closed.")

