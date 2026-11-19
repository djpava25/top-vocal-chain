# Top Vocal Chain + Top Tune

Este repositorio contiene **dos plugins** (VST3 para Windows y macOS, + AU en macOS): **Top Vocal Chain** y **Top Tune**.

---

## Top Vocal Chain

Plugin **VST3** (Windows y macOS, + AU en macOS) con una cadena vocal completa en un solo plugin:

**Autotune → Preamp → EQ → Compresor → Delay → Reverb**, más una perilla **IA Voice** que escucha tu voz y ajusta la cadena sola.

## Módulos

| Módulo | Controles |
|---|---|
| **Input / Output** | Faders con vúmetro |
| **Autotune** | Tonalidad (C–B), escala (Cromática, Mayor, Menor, Menor Armónica, Pentatónicas), Velocidad (0 ms = efecto robot/trap), Cantidad. Pantalla con la nota detectada y la corrección en cents |
| **IA Voice** | Perilla *Inteligencia* (0–100 %), botón *Escuchar*, botón *Reiniciar*. Muestra en vivo lo que está corrigiendo |
| **Preamp** | 5 tipos de saturación: **Válvula**, **Cinta**, **Transistor**, **Transformador**, **Diodo**. Drive, Warmth, Trim. Oversampling 2x |
| **EQ** | 4 modelos: **Limpio**, **Británico (estilo Neve 1073)**, **Americano (estilo API 550/5500)**, **Pasivo (estilo Pultec EQP-1A)**. Low Cut, Low, Mid (frecuencia, ganancia, Q), Presencia, Aire |
| **Compresor** | 4 modelos: **Limpio (VCA)**, **Opto (estilo LA-2A)**, **FET (estilo 1176)**, **Vari-Mu (estilo Fairchild)**. Threshold, Ratio, Attack, Release, Makeup, medidor GR |
| **Delay** | Tiempo, Feedback, Mix, Hi Cut, Sync al tempo del DAW (1/2 … 1/16, con puntillo y tresillo), Ping Pong |
| **Reverb** | Size, Damping, Width, Mix (con corte de graves en la reverb para que no embarre) |

Cada módulo tiene su botón **ON/OFF**. La ventana se puede agrandar o achicar desde la esquina.

### Modelos analógicos

**Saturación (Preamp)**
- **Válvula:** cálida, armónicos pares e impares. *Warmth* = más asimetría (más pares).
- **Cinta:** comprime suave y redondea los agudos como una cinta. *Warmth* = más oscura.
- **Transistor:** armónicos impares, más filo y mordida.
- **Transformador:** satura sobre todo los graves ("hierro"); da cuerpo sin ensuciar.
- **Diodo:** asimétrico y agresivo, para efectos y rap duro.

**Compresor**
- **Limpio (VCA):** transparente, respeta Attack y Release tal cual.
- **Opto (estilo LA-2A):** lento y musical; cuanto más comprime, más lento suelta. Color de válvula suave.
- **FET (estilo 1176):** atrapa los picos en ~1 ms, agresivo y con presencia. Attack se vuelve 10 veces más rápido.
- **Vari-Mu (estilo Fairchild):** el ratio sube con el nivel; "pega" la voz de forma muy suave.

**EQ**
- **Limpio (Digital):** preciso, sin color.
- **Británico (estilo Neve 1073):** frecuencias por pasos como el original (Mid: 360, 700, 1.6k, 3.2k, 4.8k Hz; Low Cut: 50/80/160/300 Hz a 18 dB/oct), curvas anchas y color de transformador.
- **Americano (estilo API 550/5500):** "Q proporcional": cuanto más subes, más estrecha la curva. Mid por pasos (400, 800, 1.5k, 3k, 5k Hz). Punch y color.
- **Pasivo (estilo Pultec EQP-1A):** al subir **Low** hace el famoso "truco Pultec" (realce de graves + corte encima, graves grandes sin barro). Presencia amplia en 8 kHz, aire sedoso y color de válvula.

> Son modelos inspirados en el carácter de esos equipos; no son emulaciones oficiales ni están afiliados a sus fabricantes.

## Presets

Incluye 13 presets de fábrica: *Default, Trap Autotune Duro, Rap Moderno, Reggaeton, Dembow, Pop Brillante, R&B Suave, Balada Íntima, Lo-Fi, Clásico Neve + 1176, Pultec + LA-2A (Suave), API + Cinta (Punch), Podcast / Voz Hablada*. Cada uno usa su propia combinación de modelos.
Con **GUARDAR** / **CARGAR** guardas tus propios presets (`.tvcpreset`) en `Documentos/Top Vocal Chain/Presets`.

## Cómo funciona la perilla IA Voice

Es un motor de **análisis adaptativo en tiempo real** (no una red neuronal). Mientras cantas mide:

- nivel de la toma → ajusta la ganancia de entrada (gain staging a ~-20 dBFS RMS)
- exceso de graves → sube el Low Cut
- "barro" 200–500 Hz → recorta en 330 Hz
- falta o exceso de brillo → ajusta Presencia y Aire
- sibilancia 5–9 kHz → de-esser en 7 kHz
- rango dinámico → baja el threshold y sube el ratio del compresor
- qué tan desafinado cantas → acelera el autotune

La perilla decide **cuánto** de esas correcciones se aplica (0 % = solo tus ajustes, 100 % = corrección completa). Ignora los silencios. Apaga *Escuchar* para congelar el análisis; *Reiniciar* lo empieza de cero.

## Top Tune

Afinador vocal en tiempo real con cambio de tono y de formantes.

| Función | Qué hace |
|---|---|
| **Corrección en tiempo real** | Afina mientras grabas. Tonalidad + escala (Cromática, Mayor, Menor, Menor Armónica, Pentatónicas, Blues, Personalizada) |
| **Estilos naturales** | *Natural* (respeta vibrato y deslizamientos), *Expresivo* (melodías rápidas), *Sostenido* (solo afina las notas largas) |
| **Velocidad / Cantidad** | Velocidad 0 ms = efecto duro tipo trap; valores altos = corrección suave |
| **Pitch Shift + Fino** | ±12 semitonos y ±100 cents, conservando los formantes (sin voz de "ardilla"). Con **Mix** al 50 % creas armonías |
| **Formant** | ±5 semitonos: cambia el carácter de la voz (más grave/más aguda) sin cambiar la nota |
| **MIDI Repitch** | Toca una nota en tu teclado MIDI y la voz canta esa nota |
| **MIDI Escala** | Toca las notas de tu escala en el teclado MIDI y la voz queda fija a esas notas |
| **Detector de tonalidad** | Pulsa *Detectar tonalidad*, canta o reproduce la pista unos segundos y pulsa *Aplicar* |
| **Teclado de escala** | Haz clic en las teclas para crear tu propia escala |
| **Modo Live** | Latencia de ~6 ms para monitorear mientras grabas (sin control de formantes). Modo normal: ~36 ms, compensados por el DAW |
| **Gráfica de tono** | Gris = tu voz, verde = salida afinada |

**MIDI en FL Studio:** en el plugin abre el menú de engranaje (*Wrapper settings*), en **MIDI** pon un *Input port* (por ejemplo 1) y en tu teclado/canal MIDI envía al mismo puerto.

## Descargar los plugins compilados

Cada vez que se sube código, GitHub Actions compila para Windows y macOS automáticamente:

1. Ve a la pestaña **Actions** del repositorio → abre la última ejecución verde.
2. Descarga **TopVocal-Plugins-Windows** o **TopVocal-Plugins-macOS** (sección *Artifacts*). Cada zip trae los dos plugins.

Para crear una *Release* con descargas permanentes: crea un tag que empiece por `v` (por ejemplo `v1.0.0`).

## Instalación

**Windows (FL Studio, Ableton, Reaper, Cubase…)**
Copia `Top Vocal Chain.vst3` y `Top Tune.vst3` a `C:\Program Files\Common Files\VST3\`.
En FL Studio: *Options → Manage plugins → Find plugins*, y luego búscalo como **Top Vocal Chain**.

**macOS**
Copia `Top Vocal Chain.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Top Vocal Chain.component` (AU, para Logic) a `~/Library/Audio/Plug-Ins/Components/`.
Como no está firmado con certificado de Apple, la primera vez ejecuta en Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Vocal Chain.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Vocal Chain.component"
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Tune.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Tune.component"
```

## Compilar tú mismo

Requisitos: CMake 3.22+, y Visual Studio 2022 (Windows) o Xcode (macOS). JUCE se descarga solo.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Prueba offline del audio (opcional): `cmake -B build -DTVC_BUILD_TESTS=ON` y ejecuta `TVCOfflineTest`.

## Datos técnicos

- Latencia: ~14 ms (reportada al DAW, que la compensa automáticamente)
- CPU: ~2 % de un núcleo a 44.1 kHz
- Autotune: detección de tono YIN + pitch shifter de doble cabezal; precisión medida de ±10 cents
- Limitador suave de seguridad en la salida (nunca pasa de 0 dBFS)
- Hecho con [JUCE](https://juce.com) 8 (licencia AGPLv3 / comercial de JUCE)
