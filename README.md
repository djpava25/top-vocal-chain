# Top Vocal Chain

Plugin **VST3** (Windows y macOS, + AU en macOS) con una cadena vocal completa en un solo plugin:

**Autotune → Preamp → EQ → Compresor → Delay → Reverb**, más una perilla **IA Voice** que escucha tu voz y ajusta la cadena sola.

## Módulos

| Módulo | Controles |
|---|---|
| **Input / Output** | Faders con vúmetro |
| **Autotune** | Tonalidad (C–B), escala (Cromática, Mayor, Menor, Menor Armónica, Pentatónicas), Velocidad (0 ms = efecto robot/trap), Cantidad. Pantalla con la nota detectada y la corrección en cents |
| **IA Voice** | Perilla *Inteligencia* (0–100 %), botón *Escuchar*, botón *Reiniciar*. Muestra en vivo lo que está corrigiendo |
| **Preamp** | Drive, Warmth (armónicos de válvula), Trim. Saturación con oversampling 2x |
| **EQ** | Low Cut, Low (shelf 150 Hz), Mid (frecuencia, ganancia, Q), Presencia (4 kHz), Aire (11 kHz) |
| **Compresor** | Threshold, Ratio, Attack, Release, Makeup, medidor de reducción de ganancia |
| **Delay** | Tiempo, Feedback, Mix, Hi Cut, Sync al tempo del DAW (1/2 … 1/16, con puntillo y tresillo), Ping Pong |
| **Reverb** | Size, Damping, Width, Mix (con corte de graves en la reverb para que no embarre) |

Cada módulo tiene su botón **ON/OFF**. La ventana se puede agrandar o achicar desde la esquina.

## Presets

Incluye 10 presets de fábrica: *Default, Trap Autotune Duro, Rap Moderno, Reggaeton, Dembow, Pop Brillante, R&B Suave, Balada Íntima, Lo-Fi, Podcast / Voz Hablada*.
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

## Descargar el plugin compilado

Cada vez que se sube código, GitHub Actions compila para Windows y macOS automáticamente:

1. Ve a la pestaña **Actions** del repositorio → abre la última ejecución verde.
2. Descarga **TopVocalChain-Windows** o **TopVocalChain-macOS** (sección *Artifacts*).

Para crear una *Release* con descargas permanentes: crea un tag que empiece por `v` (por ejemplo `v1.0.0`).

## Instalación

**Windows (FL Studio, Ableton, Reaper, Cubase…)**
Copia `Top Vocal Chain.vst3` a `C:\Program Files\Common Files\VST3\`.
En FL Studio: *Options → Manage plugins → Find plugins*, y luego búscalo como **Top Vocal Chain**.

**macOS**
Copia `Top Vocal Chain.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Top Vocal Chain.component` (AU, para Logic) a `~/Library/Audio/Plug-Ins/Components/`.
Como no está firmado con certificado de Apple, la primera vez ejecuta en Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Vocal Chain.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Vocal Chain.component"
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
