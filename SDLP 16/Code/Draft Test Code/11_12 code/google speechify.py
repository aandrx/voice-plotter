import sounddevice as sd
import numpy as np
from google.cloud import speech
import serial
import time
import os

# Set Google Cloud credentials
os.environ['GOOGLE_APPLICATION_CREDENTIALS'] = r'/Users/katiema/Documents/SLDPspeech/eg-katie-027d0a0f4e83.json'

# Configuration
SAMPLE_RATE = 16000  # Hz
ARDUINO_PORT = 'COM6'  # Windows side of virtual pair mac side: '/dev/cu.usbmodemXXXX' or '/dev/cu.usbserial-XXXX'
BAUD_RATE = 9600

# Create Speech client once
client = speech.SpeechClient()

# Open serial connection once
try:
    ser = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=1)
    print(f"Connected to Arduino on {ARDUINO_PORT}\n")
    time.sleep(0.5)
except Exception as e:
    print(f"Error connecting to Arduino: {e}")
    exit(1)

# Global variable to store recording
audio_data = None
is_recording = False

def show_menu():
    print("\n" + "="*50)
    print("SPEECH-TO-TEXT CONTROLLER")
    print("="*50)
    print("1. Start Recording")
    print("2. Stop Recording & Transcribe")
    print("3. Quit")
    print("="*50)

def start_recording():
    global audio_data, is_recording
    if is_recording:
        print("Already recording!")
        return
    
    print("\nRecording started... (Press 2 to stop)")
    is_recording = True
    # Start recording in background (non-blocking)
    audio_data = []

def stop_and_transcribe():
    global audio_data, is_recording
    if not is_recording:
        print("Not recording! Press 1 to start recording first.")
        return
    
    is_recording = False
    print("Recording stopped. Processing...\n")
    
    # Get the recorded audio
    duration = float(input("How many seconds did you record? "))
    print(f"\nRecording {duration} seconds of audio...")
    audio_chunk = sd.rec(int(duration * SAMPLE_RATE), samplerate=SAMPLE_RATE, channels=1, dtype='int16')
    sd.wait()
    
    print("Transcribing...")
    
    # Convert to bytes
    audio_content = audio_chunk.flatten().tobytes()
    
    # Configure audio
    audio = speech.RecognitionAudio(content=audio_content)
    config = speech.RecognitionConfig(
        encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16,
        sample_rate_hertz=SAMPLE_RATE,
        language_code="en-US"
    )
    
    # Recognize speech
    try:
        response = client.recognize(config=config, audio=audio)
        
        # Process results and send to Arduino
        if response.results:
            for result in response.results:
                transcript = result.alternatives[0].transcript
                print(f"\nRecognized: '{transcript}'")
                
                # Send to Arduino
                ser.write((transcript + '\n').encode('utf-8'))
                ser.flush()
                print(f"Sent to Arduino")
        else:
            print("No speech detected")
    except Exception as e:
        print(f"Transcription error: {e}")

# Main loop
try:
    while True:
        show_menu()
        choice = input("\nEnter your choice (1-3): ").strip()
        
        if choice == '1':
            duration = float(input("How many seconds to record? "))
            print(f"\nRecording for {duration} seconds... Speak now!")
            audio_data = sd.rec(int(duration * SAMPLE_RATE), samplerate=SAMPLE_RATE, channels=1, dtype='int16')
            sd.wait()
            print("Recording complete! Press 2 to transcribe.")
            
        elif choice == '2':
            if audio_data is None:
                print("No recording found! Press 1 to record first.")
                continue
                
            print("Transcribing...")
            
            # Convert to bytes
            audio_content = audio_data.flatten().tobytes()
            
            # Configure audio
            audio = speech.RecognitionAudio(content=audio_content)
            config = speech.RecognitionConfig(
                encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16,
                sample_rate_hertz=SAMPLE_RATE,
                language_code="en-US"
            )
            
            # Recognize speech
            try:
                response = client.recognize(config=config, audio=audio)
                
                # Process results and send to Arduino
                if response.results:
                    for result in response.results:
                        transcript = result.alternatives[0].transcript
                        print(f"\nRecognized: '{transcript}'")
                        
                        # Send to Arduino
                        ser.write((transcript + '\n').encode('utf-8'))
                        ser.flush()
                        print(f"Sent to Arduino")
                else:
                    print("No speech detected")
            except Exception as e:
                print(f"Transcription error: {e}")
            
            # Clear the recording
            audio_data = None
            
        elif choice == '3':
            print("\nClosing application...")
            ser.close()
            print("Connection closed. Goodbye!")
            break
            
        else:
            print("Invalid choice! Please enter 1, 2, or 3.")
            
except KeyboardInterrupt:
    print("\n\nInterrupted by user...")
    ser.close()
    print("Connection closed.")
