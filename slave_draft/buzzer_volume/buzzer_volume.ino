#define buzzer A0

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:
  for (unsigned long freq = 125; freq <= 15000; freq += 10) {  
      toneAC(freq); // Play the frequency (125 Hz to 15 kHz in 10 Hz steps).
      delay(1);     // Wait 1 ms so you can hear it.
    }
}
