void setup(){
  Serial.begin();
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  digitalWrite(D2, LOW);
  digitalWrite(D3, LOW);
  digitalWrite(D4, HIGH);
}

void loop(){
  digitalWrite(D2, HIGH);
  digitalWrite(D3, LOW);
  delay(1000);
  digitalWrite(D2, LOW);
  digitalWrite(D3, HIGH);
}