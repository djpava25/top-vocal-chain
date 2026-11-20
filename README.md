# Top Vocal Chain + Top Tune + Top Summing

Este repositorio contiene **cuatro plugins** (VST3 para Windows y macOS, + AU en macOS): **Top Vocal Chain**, **Top Tune**, **Top Summing** y **Top Strip 737**.

---

## Top Vocal Chain

Plugin **VST3** (Windows y macOS, + AU en macOS) con una cadena vocal completa en un solo plugin:

**Autotune → Preamp → EQ → Compresor → Wider → Delay → Reverb**, más una perilla **IA Voice** que escucha tu voz y ajusta la cadena sola.

Interfaz estilo tira de consola (diseño original), escalable de 100 % a 200 % y con módulos que se pueden ocultar: módulos verticales lado a lado, perillas con tapa de color por función, botones IN iluminados, pantallas ámbar, curva del EQ en vivo, medidores LED de entrada/salida/GR y fader de salida. Doble clic en una perilla la regresa a su valor por defecto; al girarla se muestra su valor exacto.

## Módulos

| Módulo | Controles |
|---|---|
| **Input / Output** | Faders con vúmetro |
| **Autotune** | Motor natural que conserva los formantes. Tonalidad + escala (Cromática, Mayor, Menor natural, Menor armónica, Pentatónicas, Blues) con **AUTO KEY** (detecta la tonalidad cantando). Modos **NATURAL** y **ROBOTIC**. Retune Speed, Amount, **Humanize** (notas largas más naturales), **Flex-Tune** (solo afina al acercarse a la nota), **Formant** (±5 st), **Transpose** (±12 st), **Vibrato**, **Tracking** y **LOW LAT** (~14 ms para grabar). Pantalla con nota objetivo, nota cantada, Hz, desviación en cents, indicador de voz y de afinación |
| **IA Voice** | Perilla *Inteligencia* (0–100 %), *Escuchar*, *Reset*. Botón **ANALIZAR VOZ**: cantas ~8 s y abre un informe (registro vocal Bajo/Barítono/Tenor/Contralto/Mezzosoprano/Soprano, frecuencia media, tonalidad, nivel, dinámica, resonancias, sibilancia, presencia, graves, ruido) con el botón **APLICAR RECOMENDACIONES** |
| **Preamp** | 8 modelos: **Clean** (transparente), **Tube** (armónicos cálidos), **Vintage** (saturación suave), **Modern** (brillante y limpio), **Tape** (compresión de cinta), **Transformer** (color de hierro), **Transistor**, **Diode**. Drive, Warmth, Trim, Input. Oversampling 2x |
| **EQ** | 4 modelos (Limpio, estilo Neve 1073, API 550/5500, Pultec EQP-1A). **Gráfica grande** con la curva real en vivo, **puntos arrastrables** (rueda del mouse en MID = Q, doble clic = reset) y valores al pasar el mouse. Low Cut y **High Cut** con pendientes de 6/12/18/24 dB/oct, Low, Mid (frecuencia, ganancia, Q), Presence, Air |
| **Compresor** | 7 modos: **Clean VCA**, **Vintage Opto**, **FET Punch**, **Vari-Mu Glue**, **LA Vocal**, **Fast**, **Smooth**. Threshold, Ratio, Attack, Release, **Knee**, Makeup, **Mix** (paralela), **filtro HPF del detector** y **sidechain externo**. **Curva de compresión** en vivo y medidores IN / GR / OUT |
| **Stereo Lab** | 5 efectos (**Wide**, **Chorus**, **Double**, **Shift**, **Dimension**) con perilla Efecto, **Width 0–200 %**, **Mix**, botón **MONO**, modo **SAFE VOCAL** (graves siempre en mono y **corrección automática de fase**: si la correlación se vuelve negativa, cierra el ancho solo y lo devuelve cuando la fase está bien; la pantalla muestra "AUTO FASE -x%") y medidor de **correlación** con aviso de **problema de fase** |
| **Delay / Reverb** | Un módulo con dos pestañas. **Delay**: Time, Feedback, Mix, Width, Lo Cut, Hi Cut, Saturation, Modulation, Ducking, Sync (1/2 … 1/16, con puntillo y tresillo), Ping Pong. **Reverb**: tipos **Room, Plate, Hall, Chamber, Vocal Plate**, Pre-delay, Decay, Size, Damping, Early Reflections, Diffusion, Lo Cut, Hi Cut, Ducking, Width, Mix |
| **Meters** | Peak hold, **True Peak**, RMS, **LUFS momentáneo e integrado**, aviso de clip, correlación y medidores IN/OUT/GR. Clic en la pantalla para resetear |

Cada módulo tiene su botón **IN** (verde = activo, gris = apagado, naranja = saturando). Los módulos se pueden ocultar desde **MÓDULOS** y la ventana tiene tamaños 100/125/150/200 %.

### Flujo de trabajo (barra superior y módulo OUTPUT)

| Función | Qué hace |
|---|---|
| **Tamaño 100 / 125 / 150 / 200 %** | Agranda toda la interfaz sin perder nitidez |
| **MÓDULOS** | Oculta los módulos que no usas; el resto se reacomoda y la ventana se achica |
| **IN de cada módulo** | Verde = activo, gris = apagado (el módulo se ve atenuado), naranja = saturando |
| **Bypass sin clicks** | Encender/apagar cualquier módulo hace un fundido de 20 ms |
| **UNDO / REDO / RESET** | Deshacer, rehacer y volver a los ajustes por defecto |
| **A / B / COPIAR** | Dos juegos de ajustes para comparar cadenas; COPIAR pasa el actual a la otra ranura |
| **MANTENER: ORIGINAL** | Mientras lo mantienes pulsado escuchas la voz original **al mismo volumen** que la procesada |
| **BYPASS** | Bypass general de toda la cadena (sin clicks y alineado con la latencia) |
| **DRY / WET** | Mezcla general entre la voz original y la procesada |
| **LIMIT -0.3 dB** | Limitador de salida opcional |
| **Medidores** | Clic en un medidor para resetear el pico; CLIP se enciende en rojo si la salida satura |

### Modelos analógicos

**Saturación (Preamp)**
- **Clean:** solo ganancia, transparente.
- **Tube:** cálida, armónicos pares e impares. *Warmth* = más asimetría (más pares).
- **Vintage:** saturación suave, cuerpo en medios-graves y agudos redondeados.
- **Modern:** brillante y limpio, graves firmes.
- **Tape:** comprime suave y redondea los agudos como una cinta. *Warmth* = más oscura.
- **Transformer:** satura sobre todo los graves ("hierro"); da cuerpo sin ensuciar.
- **Transistor:** armónicos impares, más filo y mordida.
- **Diode:** asimétrico y agresivo, para efectos y rap duro.

**Compresor**
- **Clean VCA:** transparente, respeta Attack y Release tal cual.
- **LA Vocal:** opto más rápido y denso, deja la voz al frente.
- **Fast:** ataque y release rapidísimos para controlar picos.
- **Smooth:** detección lenta, compresión muy suave.
- **Opto (estilo LA-2A):** lento y musical; cuanto más comprime, más lento suelta. Color de válvula suave.
- **FET (estilo 1176):** atrapa los picos en ~1 ms, agresivo y con presencia. Attack se vuelve 10 veces más rápido.
- **Vari-Mu (estilo Fairchild):** el ratio sube con el nivel; "pega" la voz de forma muy suave.

**EQ**
- **Limpio (Digital):** preciso, sin color.
- **Británico (estilo Neve 1073):** frecuencias por pasos como el original (Mid: 360, 700, 1.6k, 3.2k, 4.8k Hz; Low Cut: 50/80/160/300 Hz a 18 dB/oct), curvas anchas y color de transformador.
- **Americano (estilo API 550/5500):** "Q proporcional": cuanto más subes, más estrecha la curva. Mid por pasos (400, 800, 1.5k, 3k, 5k Hz). Punch y color.
- **Pasivo (estilo Pultec EQP-1A):** al subir **Low** hace el famoso "truco Pultec" (realce de graves + corte encima, graves grandes sin barro). Presencia amplia en 8 kHz, aire sedoso y color de válvula.

> Son modelos inspirados en el carácter de esos equipos; no son emulaciones oficiales ni están afiliados a sus fabricantes.

### Novedades: visuales premium, más procesamiento y funciones de estudio

**Controles y pantalla**
- Perillas con aro metálico, anillo LED que sigue el valor (desde el centro en las bipolares) y el valor exacto mientras las tocas.
- **Doble clic** en una perilla = escribir el valor exacto · **Shift** al arrastrar = ajuste fino · rueda del mouse = ajuste fino · **Alt+clic** = valor por defecto.
- Tooltips profesionales en todos los controles.
- **Analizador de espectro** en el EQ (POST o PRE + POST).
- **Gráfico de afinación en vivo** en el Autotune: la voz que entra (ámbar) y la corregida (verde) sobre las notas de la escala.
- **Imagen estéreo** semicircular en Stereo Lab con medidor de ancho, **MONO CHECK** y aviso de compatibilidad mono.

**Procesamiento**
- Compresor: nuevos modos **Tube** y **Modern Digital** (auto-release), zona de knee en la curva y gráfico de reducción de ganancia en el tiempo (se ve el Attack y el Release).
- Preamp: botón **ANALOG** (tolerancias de componentes y ruido de fondo muy bajo) e indicador **SAT** verde / amarillo / rojo.
- EQ: banda MID **Bell / Notch**, modo **Stereo / Mid / Side**, **SOLO MID** y **EQ dinámico** (DYNAMIC + DYN THR).
- Delay: tipos **Digital, Tape, Analog, Slap**, **TAP tempo** y gráfico de las repeticiones.
- Reverb: tipos nuevos **Vintage** y **Modern** y gráfico de reflexiones y cola.

**Funciones de estudio**
- **PRESET BROWSER**: presets por género, artista, tipo de voz y estilo de mezcla, más tus presets guardados.
- **MIDI Learn**: clic derecho en una perilla → MIDI Learn → mueve un control de tu teclado MIDI (se guarda con el proyecto).
- **OPCIONES**: calidad **Eco / Normal / High** (oversampling 1x / 2x / 4x), tema **Hardware / Oscuro**, quitar MIDI, **exportar / importar configuración** (.tvcconfig).
- Indicador de **latencia** y calidad en la barra superior.

## Presets

Incluye 18 presets de fábrica: *Default, Dominican Dembow, Trap Aggressive, Modern Reggaeton, Bad Bunny Style, El Alfa Style, Melodic Trap, Clean Pop Vocal, R&B Smooth, Rap Punch, Drill Vocal, Bachata Modern, Studio Clean, Clásico Neve + 1176, Pultec + LA-2A (Suave), API + Cinta (Punch), Lo-Fi, Podcast / Voz Hablada*.

> Los presets "Style" son ajustes inspirados en el sonido general de esos géneros/artistas. No están afiliados ni aprobados por ellos.
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

**Sidechain del compresor en FL Studio:** en el Mixer, envía la pista que debe controlar la compresión a la pista de la voz con *Sidechain to this track*; luego activa **SIDECHAIN EXT** en el compresor.

**MIDI en FL Studio:** en el plugin abre el menú de engranaje (*Wrapper settings*), en **MIDI** pon un *Input port* (por ejemplo 1) y en tu teclado/canal MIDI envía al mismo puerto.

## Top Summing

Sumador analógico estilo consola británica (inspirado en el Neve 8816). Da el "pegamento", peso y profundidad de mezclar por una consola.

**Cómo se usa:** pon una instancia en **modo CHANNEL** en cada pista (o en los grupos) y una en **modo BUS** en el master. Cada instancia es como un canal distinto de la consola.

| Control | Qué hace |
|---|---|
| **DRIVE** | Cuánto empujas la señal hacia el transformador (-6 a +18 dB). Más drive = más densidad y armónicos |
| **IRON** | Color del transformador: satura más los graves que los agudos y da "peso" |
| **BUS / CHANNEL** | Switch de modo. BUS agrega crosstalk entre canales y un poco más de color |
| **WIDTH** | Ancho estéreo (Mid/Side): 0 = mono, 100 = normal, 200 = extra ancho |
| **CROSSTALK** | Cuánto se "escuchan" los canales entre sí, como en una consola real (solo en BUS, ~-40 dB) |
| **ANALOG** | Tolerancias de componentes: izquierda y derecha suenan levemente distintos (más profundidad) |
| **NUEVA UNIDAD** | Cambia esas tolerancias, como usar otra consola física. Se guarda con tu proyecto |
| **NOISE** | Ruido de fondo analógico (~-96 dBFS) |
| **SUM IN** | Encendido / bypass |
| **OUTPUT + VU** | Nivel de salida con VU meter de aguja |

Incluye 9 presets: *Neutral (canal), Canal Cálido, Canal Empujado (Hot), Bus de Mezcla, Bus Pegado (Glue), Bus de Batería, Bus de Voces, Master Abierto, Vintage con Ruido*.

> Modelo inspirado en el carácter de esas consolas; no es una emulación oficial ni está afiliado a su fabricante.

## Descargar los plugins compilados

Cada vez que se sube código, GitHub Actions compila para Windows y macOS automáticamente:

1. Ve a la pestaña **Actions** del repositorio → abre la última ejecución verde.
2. Descarga **TopVocal-Plugins-Windows** o **TopVocal-Plugins-macOS** (sección *Artifacts*). Cada zip trae los tres plugins.

Para crear una *Release* con descargas permanentes: crea un tag que empiece por `v` (por ejemplo `v1.0.0`).

## Instalación

**Windows (FL Studio, Ableton, Reaper, Cubase…)**
Copia `Top Vocal Chain.vst3`, `Top Tune.vst3` y `Top Summing.vst3` a `C:\Program Files\Common Files\VST3\`.
En FL Studio: *Options → Manage plugins → Find plugins*, y luego búscalo como **Top Vocal Chain**.

**macOS**
Copia `Top Vocal Chain.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Top Vocal Chain.component` (AU, para Logic) a `~/Library/Audio/Plug-Ins/Components/`.
Como no está firmado con certificado de Apple, la primera vez ejecuta en Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Vocal Chain.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Vocal Chain.component"
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Tune.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Tune.component"
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Top Summing.vst3"
xattr -cr ~/Library/Audio/Plug-Ins/Components/"Top Summing.component"
```

## Compilar tú mismo

Requisitos: CMake 3.22+, y Visual Studio 2022 (Windows) o Xcode (macOS). JUCE se descarga solo.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Prueba offline del audio (opcional): `cmake -B build -DTVC_BUILD_TESTS=ON` y ejecuta `TVCOfflineTest`.

## Datos técnicos

- Latencia: ~36 ms con el autotune natural, ~14 ms con LOW LAT (reportada al DAW, que la compensa automáticamente)
- CPU: ~2 % de un núcleo a 44.1 kHz
- Autotune: detección de tono YIN + TD-PSOLA (conserva formantes); precisión medida de ±2 cents
- Limitador suave de seguridad en la salida (nunca pasa de 0 dBFS)
- Hecho con [JUCE](https://juce.com) 8 (licencia AGPLv3 / comercial de JUCE)

---

## Top Strip 737

Channel strip de válvula (VST3 Windows/macOS + AU en macOS) con diseño propio, inspirado en el flujo de trabajo de los channel strips clásicos de válvula:

| Sección | Controles |
|---|---|
| **Tube Preamp** | Preamp Gain (-20 a +30 dB), **MIC** (más color de válvula al mismo volumen), **Phase**, **Pad -20**, High Pass 30–140 Hz con **HPF IN** |
| **Opto Compressor** | Threshold, Compression (1:1 a 20:1), Attack, Release (óptico, depende del programa), **COMP IN**, **EQ > COMP** (EQ antes del compresor), **SC LINK** (de-esser: el compresor escucha la banda High Mid), **VU = GR** |
| **VU** | VU de aguja: nivel de salida o reducción de ganancia |
| **Equalizer** | Bass shelf (15/30/60/150 Hz, ±24 dB), Low Mid (35–450 Hz, ±16 dB), High Mid (220 Hz–2.8 kHz, ±16 dB), Treble shelf (10/15/20/32 kHz, ±24 dB). Las medias tienen **HI-Q** (banda estrecha) y **X10** (frecuencia ×10). **EQ IN** |
| **Output** | Output (-24 a +12 dB) y **POWER** (bypass sin clicks) |

Presets: *Default, Voz Calida, Voz Rap Presencia, Voz Aire (Pop), De-Esser Suave, Bass DI, Guitarra Acustica, Color de Valvula*.

> Top Strip 737 es un diseño original. No está afiliado a Avalon Design ni a Universal Audio.
