# Useless Machine mit Charakter

Kippschalter an → ein Servo-Arm kommt aus der Box und legt den Schalter wieder um.
Das Ganze passiert jedes Mal ein bisschen anders: mal zögerlich, mal hektisch, mal beleidigt.

- Firmware: [`firmware/UselessMachine/`](firmware/UselessMachine/) (Arduino, ESP32-S3 SuperMini)
- Getestet: kompiliert mit arduino-esp32 **3.3.8** für `esp32s3`. Auf echter Hardware ist der Code **noch nicht** gelaufen. Die Kalibrierung (Abschnitt 8) ist Pflicht.

**Diese Hardware ist eingeplant:** SG90-Servo · 3× AA Alkaline · Kippschalter mit 3 Anschlüssen · Deckel, den der Arm aufdrückt und der durch sein Gewicht wieder zufällt.

---

## 1. Schaltplan in Worten

```
                  Hauptschalter (optional, empfohlen)
 3×AA  (+) ───────────o/o────────┬──────────────────────────────┬──────────► Servo V+ (rot)
                                  │                              │
                                  │                     C1 1000 µF / 10 V  (+)   ┐ direkt am
                                  │                     C2 100 nF Keramik        ┘ Servostecker
                                  │                              │
                                  │    D1 1N5819 (Schottky)      │
                                  └────►|──────┬─────────────────┼──────────► ESP32 "5V"-Pin
                                               │                 │
                                      C3 220 µF / 10 V (+)       │
                                      C4 100 nF                  │
                                               │                 │
 3×AA  (−) ────────────────────────────────────┴─────────────────┴──────────► GND-Sternpunkt:
                                                                                ESP32 GND,
                                                                                Servo GND (braun/schwarz),
                                                                                Schalter, C1−, C3−

 ESP32 GPIO5 ───[R1 330 Ω]───────────────► Servo Signal (orange/gelb)
      │
      └──[R2 10 kΩ]── GND        (Pull-down: Servo zuckt nicht beim Einschalten)

 ESP32 3V3 ───[R3 10 kΩ]───┬─────────────► ESP32 GPIO4
                           │
                           ├── Kippschalter: äußerer Pin ─┐
                           │                 mittlerer Pin (COM) ── GND
                           │                 anderer äußerer Pin: frei
                           │                               (Schalter "AN" = GPIO4 auf GND)
                           │
                           └── C5 100 nF ── GND        (optional, Entprellung/Störschutz)
```

Stückliste zusätzlich zu Board, Servo, Batteriehalter und Schalter:

| Teil | Wert | Wofür |
|---|---|---|
| D1 | **1N5819** (Schottky; gleichwertig 1N5817, SS14). **Keine** Silizium-Diode wie 1N4007/1N4148/1N5399/1N5408/FR107/FR207 | trennt die ESP-Versorgung vom Servo und schützt vor Rückspeisung in USB bzw. Batterie. **Optional**, wenn USB nie gleichzeitig mit den Batterien angeschlossen ist (dann Batterie-Plus direkt an 5V, Hauptschalter vor der Aufteilung Servo/ESP) |
| C1 | 470–1000 µF, **≥ 10 V**, Elko, low-ESR | Servo-Anlaufstrom abfangen |
| C2, C4, C5 | 100 nF Keramik | hochfrequente Störungen |
| C3 | 100–220 µF, ≥ 10 V | Stützpuffer für den ESP hinter der Diode |
| R1 | 220–470 Ω | Schutz des GPIO (Servo-Rückwirkung, Kurzschluss) |
| R2 | 10 kΩ | definiertes LOW auf der Signalleitung |
| R3 | 10 kΩ | **optional**: externer Pull-up für den Schalter, nur bei Störungen nötig (der interne Pull-up ist aktiv) |

Verdrahtungs-Regeln:
1. **Minus aller Teile an einem Punkt** (Sternpunkt am Batterie-Minus). Der Servo-Masse-Strom soll nicht über das ESP-Board fließen.
2. Servo-Plus und -Minus mit **dicken, kurzen Leitungen direkt vom Batteriehalter**. Nicht über Pins oder Leiterbahnen des ESP-Boards.
3. C1 so nah wie möglich an den Servo-Stecker löten. **Polung beachten** (Streifen = Minus).
4. Die Schalterleitungen nicht parallel zu den Servokabeln verlegen, zumindest verdrillen.

---

## 2. Pinbelegung

**Die Zahlen im Code sind GPIO-Nummern des Chips.** Auf SuperMini-Boards steht fast immer direkt die GPIO-Nummer an den Pins („4“, „IO4“, „GP4“). Es gibt aber Klone mit anderer Beschriftung oder Anordnung. **Zähle nie nach Position**, sondern lies die Beschriftung. Im Zweifel schick ein Foto von Vorder- und Rückseite.

### ESP32-S3 SuperMini

| Funktion | GPIO | Warum |
|---|---|---|
| Servo-Signal | **GPIO 5** | frei, kein Strapping |
| Schalter | **GPIO 4** | frei, kein Strapping |
| Versorgung | **5V**-Pin (über D1), **G** | |

Alternativen: GPIO 1, 2, 6–13.
**Meiden:**
- GPIO 0 (BOOT), 3, 45, 46: Strapping-Pins.
- GPIO 19/20: USB.
- GPIO 26–32: Flash. GPIO 33–37: bei Varianten mit Octal-PSRAM (z. B. „N8R8“) belegt.
- GPIO 43/44: UART0 (TX/RX).
- GPIO 48: auf vielen S3 SuperMini die RGB-LED.
- **3V3-Pin nie mit der Batterie verbinden.** Eine frische 3×AA liefert 4,8 V, der Chip verträgt max. 3,6 V.

#### Geprüftes Board: ESP32-S3 SuperMini mit Chip „ESP32-S3 FH4R2“ und LiPo-Lader

- **FH4R2** = 4 MB Flash + 2 MB PSRAM (Quad). In der Arduino IDE: Flash Size **4MB**, PSRAM **„QSPI PSRAM“** oder Disabled (die PSRAM wird nicht gebraucht).
- Randpins links: TX (43), RX (44), 1, 2, 3, **4**, **5**, 6, 7. Rechts: **5V**, **GND**, 3V3, 13, 12, 11, 10, 9, 8. Die Pads auf der Rückseite (14–18, 21, 33–48) werden nicht benötigt.
- ⚠️ **Auf der Vorderseite stehen die Beschriftungen zwischen den Pads**, jeweils *unter* dem zugehörigen Pad. Verlass dich auf die **Rückseite**, dort steht jede Zahl direkt neben ihrem Pad.
- GPIO 48 ist die RGB-LED (unten rechts).
- ⚠️ Das Board hat einen **Lade-IC für LiPo-Akkus** (Pads **B+ / B−** auf der Rückseite, LED „BAT“). **Die AA-Batterien niemals an B+/B− anschließen!** Der Lader würde versuchen, die Alkalinezellen mit 4,2 V zu laden, und die können dann auslaufen oder heiß werden. Die Batterie kommt wie im Schaltplan über D1 an **5V**. Dass die BAT-LED ohne Akku flackert oder leuchtet, ist bei solchen Boards normal.

Die Pins stellst du in [`config.h`](firmware/UselessMachine/config.h) um.

---

## 3. Stromversorgung

### Passt der Servo zu 3×AA?

| Batterie | Spannung frisch → leer | Bewertung |
|---|---|---|
| 3× **Alkaline** | 4,8 V → ca. 3,3 V | ✅ passt für Micro-Servos (SG90, MG90S). Unter ~4 V wird der Arm spürbar schwächer. |
| 3× **NiMH-Akku** | 4,2 V → 3,6 V (meist 3,6 V) | ⚠️ zu wenig. Nimm **4× NiMH** (4,8 V). |
| 4× **Alkaline** | 6,4 V frisch | ❌ zu viel für viele Micro-Servos (max. 6 V) und grenzwertig für den Spannungsregler des Boards. |

Welcher Servo?
- **SG90 / MG90S (Micro)**: Betrieb 4,8–6 V, läuft aber auch mit 4,5 V (etwas langsamer, ca. 10–15 % weniger Kraft). Blockierstrom SG90 ca. 0,6–0,8 A, MG90S ca. 1–1,2 A. **Gut geeignet.**
- **MG996R / DS3218 (Standardgröße)**: Blockierstrom 2,5 A und mehr. An 3×AA gibt das Spannungseinbrüche und Resets. **Nicht empfohlen.** Für eine Useless Machine reicht ein Micro-Servo locker.

**Kraft-Check SG90:** Er schafft ca. 1,8 kg·cm bei 4,8 V, bei 4,5 V und älteren Batterien eher **1,3–1,5 kg·cm**. Plane mit höchstens **~1 kg·cm**, damit Reserve bleibt.
Benötigtes Moment = Schaltkraft (kg) × Abstand Kontaktpunkt–Servoachse (cm).
- Schaltkraft messen: Box mit dem Schalter auf eine Küchenwaage stellen und den Hebel mit einem Stift umlegen. Der Höchstwert zählt.
- Beispiel: 250 g Schaltkraft → Kontaktpunkt höchstens **4 cm** von der Achse.
- Kürzerer Arm = mehr Kraft. Reicht es trotzdem nicht: Schalter mit weniger Schaltkraft oder MG90S (gleiche Größe, Metallgetriebe).

Dazu kommt der **Innenwiderstand** von Alkalinezellen: ca. 0,15–0,3 Ω pro Zelle, also 0,5–1 Ω für drei Stück. Beim Anlaufen zieht der Servo kurz ~1 A, die Spannung am Pack bricht dann für einige Millisekunden um **0,5–1 V ein**. Genau das resettet ungeschützte ESP32s (Brownout).

### Wie wird der ESP32 versorgt?

Das S3 SuperMini hat einen **3,3-V-Spannungsregler (LDO, meist ME6211 o. ä., max. 6 V Eingang)** hinter dem **5V-Pin**. Die Batterie gehört an **5V**, **niemals an 3V3** und natürlich nie an einen GPIO.

- Der LDO braucht nur ~0,1–0,2 V mehr als 3,3 V. Mit Diode (≈0,25 V Verlust) läuft der ESP bis zu einer Batteriespannung von ca. 3,5 V stabil, darunter bis ~3,2 V meist noch. Das ist unkritisch, weil der Servo vorher schlapp macht.
- **Warum Schottky (1N5819)?** Am ESP fließen nur ca. 40–100 mA. Dabei fallen an der 1N5819 etwa **0,25–0,35 V** ab. Silizium-Dioden verlieren **0,7–1 V** (1N4007, 1N4148, 1N5399, 1N5408; FR107/FR207 als schnelle Silizium-Dioden eher noch mehr). Mit halb leeren Batterien (3,8 V) blieben dann nur ~2,9 V für den ESP, und das führt zu Resets. Einbaurichtung: **Ring (Kathode) zum 5V-Pin des ESP**, die andere Seite an Batterie-Plus.
- **D1 + C3** bilden einen kleinen Puffer. Bricht die Servoseite ein, sperrt die Diode, und C3 versorgt den ESP für diese Millisekunden weiter. Das ist die wichtigste Maßnahme gegen Brownouts.
- **USB und Batterie gleichzeitig:** Durch D1 kann USB-5V nicht in die Batterien zurückfließen (nicht jedes SuperMini hat dafür selbst eine Diode). Zum Testen am PC also ruhig beides anschließen. Der Servo läuft dabei nur mit eingelegten Batterien.
- Den Servo **nicht** aus dem 5V-Pin des Boards speisen (USB-Port, dünne Leiterbahnen).

### Signalpegel

Der ESP gibt 3,3-V-Pulse aus. Praktisch alle Hobby-Servos erkennen das zuverlässig, besonders bei 4,5 V Versorgung. Nur wenn ein Servo gar nicht reagiert: Pegelwandler oder ein NPN-Transistor als Treiber.

### Schalter (3 Anschlüsse)

Ein Kippschalter mit 3 Pins ist ein Umschalter (SPDT). Der **mittlere Pin (COM)** ist je nach Hebelstellung mit dem linken oder rechten Pin verbunden.

- **Mittlerer Pin → GND**, **ein äußerer Pin → GPIO 4**, der andere äußere Pin bleibt frei.
- Der Kontakt schließt auf der **gegenüberliegenden** Seite der Hebelstellung: Hebel nach links → COM mit dem **rechten** Pin verbunden.
- Nimm den äußeren Pin, der verbunden ist, wenn der Hebel in der **„AN“-Stellung** steht (also der Stellung, aus der der Arm ihn zurückdrückt). Prüfen mit Multimeter (Durchgang) oder Befehl `s` im seriellen Monitor. Ist es falsch herum: einfach den anderen äußeren Pin nehmen.
- **Wichtig:** Der Schalter muss **ON-ON** sein (2 Stellungen). Ein **ON-OFF-ON** mit Mittelstellung funktioniert nicht, weil der Arm den Hebel sonst nur in die Mitte drückt.
- Schalter „aus“ = offen. So fließt in Ruhe **kein Strom** durch den Pull-up.
- Der interne Pull-up ist im Code aktiv und genügt. Nur wenn der Schalter von selbst auslöst (lange Leitungen neben dem Servokabel): 10 kΩ (R3) nach 3V3 und/oder 100 nF (C5) nach GND nachrüsten.

Die Onboard-RGB-LED (GPIO 48) wird beim Start ausgeschaltet. Auch ausgeschaltet zieht eine WS2812 noch ca. 0,5–1 mA. Das lässt sich nur durch Ablöten vermeiden.

### Ein/Aus und Batterielaufzeit (grobe Werte)

Die Box wird über den **Hauptschalter** ein- und ausgeschaltet. Einen Tiefschlaf gibt es nicht, der ESP ist bei eingeschalteter Box immer wach.

| Zustand | Strom |
|---|---|
| ESP32-S3 wach, ohne WLAN | ~35–45 mA |
| Servo in Ruhe (ohne Pulse) | ~5–10 mA (typabhängig) |
| Board-LEDs (Power/BAT, WS2812 aus) | ~1–5 mA |

Eingeschaltet in Ruhe zieht die Box mit dem S3 also **~50 mA**. Mit 3× AA Alkaline (~2000 mAh) sind das grob **40 Stunden eingeschaltete Zeit**. Jede Aktion kostet zusätzlich nur wenig. **Nach dem Spielen den Hauptschalter ausmachen**, sonst sind die Batterien nach knapp 2 Tagen leer. Ausgeschaltet fließt kein Strom.

---

## 4. Der Kondensator am Servo

**Empfehlung: 1000 µF, 10 V oder 16 V, Elko (low-ESR), plus 100 nF Keramik parallel, direkt am Servostecker.**

- Für deinen SG90 an 3× Alkaline sind **470 µF bereits ausreichend**. Hast du 1000 µF da, nimm den. 1000 µF geben mehr Reserve bei schwachen Batterien oder einem MG90S. Mehr als ~2200 µF bringt bei Micro-Servos kaum noch etwas.
- Spannungsfestigkeit **mindestens 10 V**. 6,3 V wären bei 4,8 V formal okay, aber ohne Reserve und mit schnellerer Alterung.
- „Low-ESR“ (z. B. für Schaltnetzteile gedacht) ist wichtiger als möglichst viele µF.
- Der 100-nF-Keramikkondensator filtert die schnellen Störspitzen des Servomotors, die der Elko nicht erwischt.
- **Polung!** Ein verpolter Elko wird heiß und kann aufplatzen.

Für den ESP hinter der Diode: **100–220 µF + 100 nF** am 5V/GND-Pin des Boards.

---

## 5. Softwarearchitektur

```
UselessMachine.ino   Zustandsautomat, Auswahl der Persönlichkeit, "Launen", Serial
       │
       ├── scripts.h    Daten: Gesten (Bytecode), 10 Stimmungen, 53 Persönlichkeiten
       ├── motion.h     Engine: Schalter entprellen · Servo (LEDC, µs) · moveTo/wiggle/jitter/push · Interpreter
       ├── bytecode.h   Die Skriptsprache: Opcodes + Makros
       └── config.h     Pins, Kalibrierung, Verhalten
```

### Ebene 1 – Bewegungs-Primitive (`motion.h`)

| Primitive | Bedeutung |
|---|---|
| `moveTo(pos, speed, ease)` | fährt interpoliert mit Beschleunigungsprofil (linear, sanft, bremsend, beschleunigend) |
| `waitMs(ms)` | wartet **und überwacht dabei den Schalter** |
| `wiggle(amp, n, speed)` | wackelt um die aktuelle Position |
| `jitter(amp, dauer)` | zufälliges nervöses Zittern |
| `push(speed, ease)` | legt den Schalter um, **prüft den Erfolg**, versucht es notfalls erneut und fährt immer vom Anschlag weg |

Positionen sind **0–100 %** statt Winkel: 0 = Ruhe, 30 = Arm berührt den Deckel, 90 = Hebel berührt, 100 = geschaltet. Die Umrechnung in µs passiert an genau einer Stelle über vier Kalibrierwerte. Alle Skripte funktionieren dadurch unabhängig von Servo, Einbaulage und Armlänge.

### Ebene 2 – Mini-Skriptsprache (`bytecode.h`)

Jede Geste ist ein kleines Byte-Array im Flash, Befehl = 1 Byte + Argumente:

```cpp
// "Mehrere Anläufe, dann näher ran"
const uint8_t A_RUNUPS[] = {
  LOOP(2, 3),                          // 2- bis 3-mal:
    MOVER(35, 60, S_FAST, E_OUT),      //   zufällig 35..60 % weit vorschnellen, abbremsen
    WAIT(150, 500),                    //   150..500 ms warten
    REL(-20, S_MED, E_SMOOTH),         //   20 % zurückweichen
    WAIT(100, 400),
  NEXT,
  MOVE(P_NEAR, S_MED, E_SMOOTH),       // dann vor den Schalter
  END };
```

Befehle: `MOVE`, `MOVER` (zufälliges Ziel), `REL`, `WAIT`, `NAP`, `WIGGLE`, `JITTER`, `LOOP/NEXT` (zufällige Anzahl, verschachtelbar), `CHANCE` (nächsten Befehl nur mit x % Wahrscheinlichkeit), `PUSH`, `HOME`.
Alle **58 Gesten zusammen belegen 684 Bytes**, jede Persönlichkeit 6 Bytes plus Name.

Sicherheitsnetz im Interpreter: Gesten werden auf max. 94 % begrenzt. Nur `PUSH` darf bis 100 % und damit an den Schalter. Unbekannte Opcodes und Endlosschleifen werden abgefangen.

### Ebene 3 – Ablauf einer Vorstellung (`.ino`)

```
Schalter AN
  → Laune aktualisieren (genervt?)  → Persönlichkeit wählen → Stimmung anpassen
  → R Reaktion  → A Anfahrt  → N Theater vor dem Schalter      [Mensch schaltet selbst aus? → "Nanu?"-Geste]
  → K Klick (mit Erfolgskontrolle)                               [klappt nicht? → Retry, dann Fehlerhinweis]
  → Z Rückzug                                                    [Mensch schaltet wieder an? → sofort nochmal, genervter]
  → evtl. später "nachgucken" → PWM aus → warten auf den nächsten Schalter
```

---

## 6. Wie aus Bausteinen 50+ Persönlichkeiten werden

Vier Schichten multiplizieren sich:

1. **Gesten-Bibliothek in 5 Phasen**: 9 Reaktionen × 13 Anfahrten × 15 Theater × 8 Klicks × 11 Rückzüge = **154.440 Abläufe**.
2. **10 Stimmungen** skalieren jede Geste: Tempo, Geduld (Pausenlänge), Nervosität (Wackelstärke), Schlampigkeit (Positionsstreuung). Aus dem gleichen „Zögern“ wird so müdes Zögern oder panisches Zögern.
3. **53 kuratierte Persönlichkeiten** als 6-Byte-Rezept, z. B.
   `{"Genervter Beamter", mAnnoyed, rLong, aStroll, nSigh, kReluctant, zSulk}`.
   Rezepte dürfen `RND` enthalten („Chaot“, „Launisch“, „Wundertüte“). 30 % der Auslösungen sind komplett frei kombiniert („Freestyle“).
4. **Gedächtnis und Laune**:
   - Wird die Maschine schnell hintereinander ausgelöst, steigt `annoy`. Ab 60 % kommen bevorzugt genervte, hektische oder wütende Charaktere, und alles wird schneller und ungeduldiger.
   - Wer während des Rückzugs wieder einschaltet, bekommt sofort eine gereizte Reaktion.
   - Erste Aktion nach dem Einschalten der Box: mit 50 % Wahrscheinlichkeit verschlafen.
   - Die letzten 5 Persönlichkeiten werden nicht sofort wiederholt.
   - Dazu Zufall in **jedem** Tempo (±15 %), jeder Pause (±10 % plus Min/Max-Bereich) und jeder Zielposition.

Eine neue Persönlichkeit ist eine Zeile in `scripts.h`, eine neue Geste ein Byte-Array plus Eintrag in Tabelle und Enum. `static_assert` meldet beim Kompilieren, wenn Tabelle und Enum nicht zusammenpassen.

Neue Gesten prüfen (Opcodes, Argumente, LOOP/NEXT-Paare, Größe):
```bash
python3 tools/validate_scripts.py
```

---

## 6b. Der Deckel

Der Deckel ist passiv: Der Arm drückt ihn auf, das Eigengewicht schließt ihn wieder. Daraus folgt:

**In der Software**
- Zwischen HOME und dem Punkt, an dem der Arm den Deckel berührt (Kalibrierpunkt DECKEL, Position 30 %), bewegt sich der Arm **unsichtbar**. Langsame Bewegungen fahren diesen Leerweg deshalb automatisch zügig (`LID_FAST_TRAVEL_PCT_S`), damit kein totes Warten entsteht.
- Gesten, die in der Box beginnen, arbeiten mit dem Deckel: **Deckel klappern** (`R_RATTLE`), Aufwachen mit Deckelspalt und Zittern, Anklopfen, Schreck mit Deckel-Aufschnappen, **knarrende Tür** (`Z_CREAK`: Deckel halb offen halten und langsam absenken), Flucht mit nachträglichem Zuhalten.
- Das spätere „Nachgucken“ hebt den Deckel einen Spalt (45 %).
- Die PWM wird nur abgeschaltet, wenn der Arm auf HOME steht, also wenn der Deckel auf der Box liegt und nicht auf dem Arm.

**In der Mechanik**
- **Öffnungswinkel unter ca. 70° begrenzen** (Anschlag, Schnur oder kurzer Arm). Geht der Deckel über 90° auf, bleibt er offen stehen.
- **Deckel leicht halten** (dünnes Sperrholz oder 3D-Druck, am besten < 40 g). Der SG90 hebt ihn und legt gleichzeitig den Schalter um.
- Der Arm sollte den Deckel **möglichst weit vom Scharnier entfernt** anheben. Dann braucht er weniger Kraft.
- Kontaktstelle glatt machen (Filz, PTFE-Band oder abgerundete Kante), sonst hakt der Deckel am Arm.
- Scharnier leichtgängig, damit der Deckel auch bei langsamem Rückzug zuverlässig mitkommt.
- Filz- oder Moosgummipunkte am Deckelrand dämpfen das Zuknallen bei schnellen Rückzügen.
- In HOME muss der Arm **2–3 mm Abstand** zum geschlossenen Deckel haben. Drückt er dagegen, brummt der Servo und zieht Strom.

---

## 7. Arduino-IDE-Einstellungen

Boardpaket: **esp32 von Espressif** (Version 3.x empfohlen, 2.x wird ebenfalls unterstützt). Keine zusätzlichen Libraries nötig.

| Einstellung | ESP32-S3 SuperMini (FH4R2) |
|---|---|
| Board | „ESP32S3 Dev Module“ |
| USB CDC On Boot | **Enabled** (sonst keine Serial-Ausgabe) |
| Flash Size | **4MB** |
| PSRAM | „QSPI PSRAM“ (oder Disabled, wird nicht gebraucht) |

Per CLI:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=4M,PSRAM=enabled firmware/UselessMachine
```

---

## 8. Inbetriebnahme & Kalibrierung (wichtig!)

Die Kalibrierung funktioniert mit **fertig montiertem Arm** und wird **im ESP gespeichert**. Sie bleibt auch nach neuem Flashen erhalten, solange „Erase All Flash Before Sketch Upload“ aus ist. `config.h` muss dafür nicht angefasst werden.

Solange nichts gespeichert ist, sendet der ESP **keine Pulse** an den Servo: Der Arm bleibt liegen, der Schalter wird ignoriert.

### Automatisch (empfohlen)

1. **Hauptschalter aus**, USB anschließen (ESP und Servo laufen dann aus USB), flashen, seriellen Monitor öffnen (115200 Baud, Zeilenende „Neue Zeile“).
2. **Kippschalter AUS**, dann `m` → der Arm fährt auf die **Mitte (1500 µs ≈ 90°)**. Das ist der einzige Sprung, egal an welchem Ende er vorher lag. Danach fährt alles langsam.
3. `<` oder `>` → der Arm fährt langsam Richtung eines Endes. **Enter stoppt.** Fährt er vom Kasten weg: Enter, dann die andere Richtung. Am **Anschlag in der Box** Enter → `H`. Den kleinen Abstand zum Anschlag (gegen Brummen) rechnet die Firmware selbst dazu.
4. **Kippschalter von Hand AN**, dann `a` → der Arm fährt langsam von HOME Richtung Schalter.
   - Drück **Enter in dem Moment, in dem der Arm den Deckel berührt**. Das ist optional, ohne Enter wird der Deckelpunkt geschätzt.
   - Sobald der Schalter umfällt, stoppt der Arm. Die Firmware berechnet **PUSH** (Schaltpunkt + 4°) und **TOUCH** (Schaltpunkt − 8°), **speichert** und fährt zurück nach HOME.
5. Mit `h`, `d`, `t`, `p` prüfen. Für `p` den Schalter vorher wieder AN stellen.
6. `c` → Kalibriermodus aus. USB abziehen, Hauptschalter an → Betrieb.

### Von Hand (Feinjustage)

Einzelne Punkte korrigieren: hinfahren (z. B. `t`), mit `+` / `-` (10 µs ≈ 1°) nachstellen, neu merken (`H`, `D`, `T` oder `P`), dann `w` zum Speichern. `s` zeigt alle Werte und die Winkel ab HOME.

- **TOUCH** muss den Hebel berühren, darf ihn aber **nicht umlegen**. Sonst stören Gesten wie „antippen und zurückzucken“.
- **DECKEL** ist die erste Berührung des Deckels von innen.
- **PUSH** ist der Punkt, an dem der Schalter sicher umkippt, plus 1–2 Schritte.

Mit `l` die Liste ansehen, mit `n0` … `n52` oder `r` einzelne Persönlichkeiten testen.

Neu anfangen: `x` löscht die gespeicherte Kalibrierung.

---

## 9. Typische ESP32-/Servo-Probleme

| Symptom | Ursache / Lösung |
|---|---|
| ESP startet neu, sobald der Servo loslegt; Serial zeigt „BROWNOUT“ | Spannungseinbruch → D1 + C3 einbauen, C1 näher an den Servo, frische Batterien, kürzere/dickere Kabel. Notlösung: `SPEED_LIMIT_PCT_S` auf z. B. `200.0f`. **Den Brownout-Detektor nicht abschalten.** |
| Servo zuckt beim Einschalten | normal kurz beim Anlegen der Spannung. R2 (10 kΩ Pull-down) hilft; der Code setzt den Pin sofort auf LOW und fährt direkt auf HOME. |
| Servo brummt/zittert in Ruhe | Arm drückt gegen den Anschlag → HOME 2–3 Schritte davor neu setzen (`h`, `+`/`-`, `H`, `w`). Die Firmware schaltet die PWM in Ruhe ab. |
| Schalter wird nicht umgelegt, Meldung „ließ sich nicht umlegen“ | PUSH weiter setzen (z. B. `u1850`, `P`, `w`) oder `PUSH_OVERDRIVE_US` in `config.h` erhöhen, Hebelarm kürzer. Beim Test über USB ist der Servo schwächer als mit frischen Batterien, Servo zu schwach oder Batterie leer. Die Maschine wartet dann, bis du den Schalter selbst ausmachst (kein Dauer-Blockieren). |
| Die Maschine hält den Schalter für schon aus | Schaltlogik andersherum → anderen äußeren Kontakt nehmen oder `SWITCH_ON_LEVEL` auf `HIGH` (dann Pull-down statt Pull-up). |
| Schalter löst zufällig aus | Störungen durch Servokabel → R3 + C5, Leitungen verdrillen und getrennt verlegen. |
| Upload klappt nicht, kein USB-Port sichtbar | **BOOT halten, RESET tippen (oder USB einstecken), BOOT loslassen**, dann flashen. |
| Keine Serial-Ausgabe | „USB CDC On Boot: Enabled“ vergessen. |
| Board bootet nicht, wenn der Schalter angeschlossen ist | Schalter hängt an einem Strapping-Pin (GPIO 0/3/45/46) → anderen GPIO nehmen. |
| Servo fährt „falschherum“ | Die Richtung ergibt sich aus der Kalibrierung. `x`, dann neu kalibrieren (Abschnitt 8). |
| Nach dem Einschalten passiert gar nichts, Servo kraftlos | Noch keine Kalibrierung gespeichert → Abschnitt 8. |
| Deckel bleibt offen stehen | Öffnungswinkel zu groß (≥ 90°) → Anschlag einbauen oder PUSH-Punkt/Armgeometrie ändern. |
| Deckel hakt am Arm / Arm stockt beim Aufdrücken | Kontaktstelle glätten (Filz/PTFE), Deckel leichter machen, Kontaktpunkt weiter weg vom Scharnier. |
| Gesten „in der Box“ sind nicht zu sehen | DECKEL-Punkt zu weit → neu setzen (`d`, `+`/`-`, `D`, `w`), er muss genau die erste Berührung des Deckels sein. |
| Schalter kippt nur in die Mitte | Schalter ist ON-OFF-ON → ON-ON-Schalter verwenden. |
| Micro-Servo wird heiß | Servo steht dauerhaft unter Last (Kalibrierung zu weit) oder ist ein Fake-„MG90S“ mit Plastikgetriebe. |
